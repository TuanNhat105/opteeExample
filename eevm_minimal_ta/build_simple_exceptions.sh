#!/bin/bash
# Build Exception Support for OP-TEE (libcxxrt + libcxx only)
# Simple approach: Build C++ exception support without full stack unwinding
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
OPENENCLAVE_ROOT="$PROJECT_ROOT/external/openenclave"
LIBCXXRT_SRC="$OPENENCLAVE_ROOT/3rdparty/libcxxrt/libcxxrt"
LIBCXX_SRC="$OPENENCLAVE_ROOT/3rdparty/libcxx/libcxx"
BUILD_DIR="$PROJECT_ROOT/build_cxx_exceptions"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
TA_DEV_KIT_DIR="${TA_DEV_KIT_DIR:-/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64}"

export PATH=/home/abc/arm-toolchain/bin:$PATH

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Build C++ Exception Support for OP-TEE${NC}"
echo -e "${BLUE}libcxxrt + libcxx (Simple Approach)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check prerequisites
if [ ! -d "$LIBCXXRT_SRC" ]; then
    echo -e "${RED}ERROR: libcxxrt not found!${NC}"
    exit 1
fi

if [ ! -d "$LIBCXX_SRC" ]; then
    echo -e "${RED}ERROR: libcxx not found!${NC}"
    exit 1
fi

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

COMMON_FLAGS=(
    -ffreestanding
    -fPIC
    -O2
    -I"$TA_DEV_KIT_DIR/include"
    -I"$PROJECT_ROOT/build_full_musl/include"
    -Wno-error
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unused-but-set-variable
)

CXX_FLAGS=(
    "${COMMON_FLAGS[@]}"
    -std=c++17
    -nostdinc++
    -fno-rtti
    -I"$LIBCXX_SRC/include"
    -I"$LIBCXXRT_SRC/src"
)

# ============================================
# Part 1: Build libcxxrt (C++ ABI Runtime)
# ============================================
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Part 1: libcxxrt (C++ ABI Runtime)${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""

mkdir -p libcxxrt_obj

# We need minimal unwind.h for libcxxrt
cat > unwind.h <<'EOF'
/* Minimal unwind.h for libcxxrt without libunwind */
#ifndef _UNWIND_H
#define _UNWIND_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    _URC_NO_REASON = 0,
    _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
    _URC_FATAL_PHASE2_ERROR = 2,
    _URC_FATAL_PHASE1_ERROR = 3,
    _URC_NORMAL_STOP = 4,
    _URC_END_OF_STACK = 5,
    _URC_HANDLER_FOUND = 6,
    _URC_INSTALL_CONTEXT = 7,
    _URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

typedef enum {
    _UA_SEARCH_PHASE = 1,
    _UA_CLEANUP_PHASE = 2,
    _UA_HANDLER_FRAME = 4,
    _UA_FORCE_UNWIND = 8,
    _UA_END_OF_STACK = 16
} _Unwind_Action;

struct _Unwind_Exception;
struct _Unwind_Context;

typedef void (*_Unwind_Exception_Cleanup_Fn)(
    _Unwind_Reason_Code reason,
    struct _Unwind_Exception* exc);

struct _Unwind_Exception {
    unsigned long long exception_class;
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    unsigned long private_1;
    unsigned long private_2;
} __attribute__((__aligned__));

typedef unsigned long _Unwind_Ptr;
typedef long _sleb128_t;
typedef unsigned long _uleb128_t;

/* Stub implementations - will panic if called */
extern _Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception*);
extern void _Unwind_Resume(struct _Unwind_Exception*);
extern void _Unwind_DeleteException(struct _Unwind_Exception*);
extern unsigned long _Unwind_GetGR(struct _Unwind_Context*, int);
extern void _Unwind_SetGR(struct _Unwind_Context*, int, unsigned long);
extern unsigned long _Unwind_GetIP(struct _Unwind_Context*);
extern void _Unwind_SetIP(struct _Unwind_Context*, unsigned long);
extern unsigned long _Unwind_GetLanguageSpecificData(struct _Unwind_Context*);
extern unsigned long _Unwind_GetRegionStart(struct _Unwind_Context*);

#ifdef __cplusplus
}
#endif

#endif /* _UNWIND_H */
EOF

# Create unwind stubs
cat > unwind_stubs.c <<'EOF'
#include <tee_internal_api.h>
#include "unwind.h"

/* Stub implementations - will panic */
_Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception* exc) {
    TEE_Panic(0xECEP0001);
    return _URC_FATAL_PHASE1_ERROR;
}

void _Unwind_Resume(struct _Unwind_Exception* exc) {
    TEE_Panic(0xECEP0002);
}

void _Unwind_DeleteException(struct _Unwind_Exception* exc) {
    /* No-op */
}

unsigned long _Unwind_GetGR(struct _Unwind_Context* ctx, int index) {
    return 0;
}

void _Unwind_SetGR(struct _Unwind_Context* ctx, int index, unsigned long value) {
}

unsigned long _Unwind_GetIP(struct _Unwind_Context* ctx) {
    return 0;
}

void _Unwind_SetIP(struct _Unwind_Context* ctx, unsigned long value) {
}

unsigned long _Unwind_GetLanguageSpecificData(struct _Unwind_Context* ctx) {
    return 0;
}

unsigned long _Unwind_GetRegionStart(struct _Unwind_Context* ctx) {
    return 0;
}
EOF

echo -n "Building unwind stubs... "
if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" -I. \
    -c unwind_stubs.c -o libcxxrt_obj/unwind_stubs.o 2>unwind_stubs.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat unwind_stubs.log
    exit 1
fi

CXXRT_SOURCES=(
    "src/exception.cc"
    "src/stdexcept.cc"
    "src/typeinfo.cc"
    "src/memory.cc"
    "src/auxhelper.cc"
    "src/guard.cc"
    "src/dynamic_cast.cc"
    "src/libelftc_dem_gnu3.c"
)

echo "Building libcxxrt sources..."
compiled_count=0
failed_count=0

for src in "${CXXRT_SOURCES[@]}"; do
    obj_name=$(basename "$src" | sed 's/\.[^.]*$/.o/')
    obj_path="libcxxrt_obj/$obj_name"
    
    echo -n "  Compiling $(basename $src)... "
    
    if [[ "$src" == *.c ]]; then
        compiler="${CROSS_COMPILE}gcc"
        flags=("${COMMON_FLAGS[@]}")
    else
        compiler="${CROSS_COMPILE}g++"
        flags=("${CXX_FLAGS[@]}" "-fexceptions")
    fi
    
    if $compiler \
        "${flags[@]}" \
        -I. \
        -I"$LIBCXXRT_SRC/src" \
        -D_GNU_SOURCE \
        -DLIBCXXRT \
        -c "$LIBCXXRT_SRC/$src" -o "$obj_path" 2>"libcxxrt_obj/${obj_name}.log"; then
        echo -e "${GREEN}OK${NC}"
        ((compiled_count++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "See: libcxxrt_obj/${obj_name}.log"
        tail -20 "libcxxrt_obj/${obj_name}.log"
        ((failed_count++))
    fi
done

echo ""
echo -e "Compiled: ${GREEN}${compiled_count}/${#CXXRT_SOURCES[@]}${NC} files"

if [ $compiled_count -eq 0 ]; then
    echo -e "${RED}ERROR: No libcxxrt objects compiled!${NC}"
    exit 1
fi

# Create libcxxrt.a
echo -n "Creating libcxxrt.a... "
${CROSS_COMPILE}ar rcs libcxxrt.a libcxxrt_obj/*.o
echo -e "${GREEN}OK${NC}"
ls -lh libcxxrt.a
echo ""

# ============================================
# Part 2: Build libcxx (Exception support)
# ============================================
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Part 2: libcxx (Exception Implementation)${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""

mkdir -p libcxx_obj

# Only exception-critical sources
LIBCXX_SOURCES=(
    "src/exception.cpp"
    "src/stdexcept.cpp"
    "src/new.cpp"
    "src/typeinfo.cpp"
)

echo "Building libcxx exception sources..."
compiled_count=0
failed_count=0

for src in "${LIBCXX_SOURCES[@]}"; do
    obj_name=$(basename "$src" .cpp).o
    obj_path="libcxx_obj/$obj_name"
    
    echo -n "  Compiling $(basename $src)... "
    if ${CROSS_COMPILE}g++ \
        "${CXX_FLAGS[@]}" \
        -fexceptions \
        -I. \
        -I"$LIBCXX_SRC/src" \
        -DLIBCXXRT \
        -D_LIBCPP_BUILDING_LIBRARY \
        -c "$LIBCXX_SRC/$src" -o "$obj_path" 2>"libcxx_obj/${obj_name}.log"; then
        echo -e "${GREEN}OK${NC}"
        ((compiled_count++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "See: libcxx_obj/${obj_name}.log"
        tail -20 "libcxx_obj/${obj_name}.log"
        ((failed_count++))
    fi
done

echo ""
echo -e "Compiled: ${GREEN}${compiled_count}/${#LIBCXX_SOURCES[@]}${NC} files"

if [ $compiled_count -eq 0 ]; then
    echo -e "${RED}ERROR: No libcxx objects compiled!${NC}"
    exit 1
fi

# Create libcxx_exceptions.a
echo -n "Creating libcxx_exceptions.a... "
${CROSS_COMPILE}ar rcs libcxx_exceptions.a libcxx_obj/*.o
echo -e "${GREEN}OK${NC}"
ls -lh libcxx_exceptions.a
echo ""

# ============================================
# Summary
# ============================================
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}BUILD COMPLETE!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Libraries created:"
ls -lh *.a
echo ""
echo -e "${GREEN}Link order:${NC}"
echo "  1. libcxx_exceptions.a  (exception.cpp, stdexcept.cpp, new.cpp)"
echo "  2. libcxxrt.a           (C++ ABI + unwind stubs)"
echo "  3. libmusl.a            (C runtime)"
echo "  4. libcxx.a             (C++ operators)"
echo ""
echo -e "${YELLOW}Update ta/sub.mk:${NC}"
echo "  libnames += cxx_exceptions cxxrt musl cxx"
echo "  libdirs += ../build_cxx_exceptions"
echo "  libdeps += ../build_cxx_exceptions/libcxx_exceptions.a"
echo "  libdeps += ../build_cxx_exceptions/libcxxrt.a"
echo ""
echo -e "${YELLOW}Compiler flags:${NC}"
echo "  cppflags-y += -fexceptions -frtti"
echo ""
echo -e "${GREEN}Ready to test std::map!${NC}"
echo ""
