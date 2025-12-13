#!/bin/bash
# Build Exception Support for OP-TEE (Patched libcxxrt approach)
# For EVM execution - MUST have try-catch support
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
MUSL_BUILD_DIR="$PROJECT_ROOT/build_full_musl_libcxx"
BUILD_DIR="$PROJECT_ROOT/build_evm_exceptions"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
TA_DEV_KIT_DIR="${TA_DEV_KIT_DIR:-/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64}"

export PATH=/home/abc/arm-toolchain/bin:$PATH

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Build Exception Support for EVM on OP-TEE${NC}"
echo -e "${BLUE}Patched libcxxrt (single-threaded)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check prerequisites
if [ ! -d "$LIBCXXRT_SRC" ]; then
    echo -e "${RED}ERROR: libcxxrt not found!${NC}"
    exit 1
fi

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"/{patched_sources,obj_cxxrt,obj_libcxx}
cd "$BUILD_DIR"

COMMON_FLAGS=(
    -ffreestanding
    -fPIC
    -O2
    -I"$MUSL_BUILD_DIR/include"
    -I.
    -I"$TA_DEV_KIT_DIR/include"
    -Wno-error
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unused-but-set-variable
)

CXX_FLAGS=(
    "${COMMON_FLAGS[@]}"
    -std=c++17
    -nostdinc++
    -I"$LIBCXX_SRC/include"
    -Dfallthrough=
)

# ============================================
# Step 1: Create stub headers
# ============================================
echo -e "${YELLOW}Step 1: Creating stub headers...${NC}"

# unwind.h stub
cat > unwind.h <<'EOF'
/* Minimal unwind.h for OP-TEE */
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

_Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception*);
void _Unwind_Resume(struct _Unwind_Exception*);
void _Unwind_DeleteException(struct _Unwind_Exception*);
unsigned long _Unwind_GetGR(struct _Unwind_Context*, int);
void _Unwind_SetGR(struct _Unwind_Context*, int, unsigned long);
unsigned long _Unwind_GetIP(struct _Unwind_Context*);
void _Unwind_SetIP(struct _Unwind_Context*, unsigned long);
unsigned long _Unwind_GetLanguageSpecificData(struct _Unwind_Context*);
unsigned long _Unwind_GetRegionStart(struct _Unwind_Context*);
unsigned long _Unwind_GetDataRelBase(struct _Unwind_Context*);
unsigned long _Unwind_GetTextRelBase(struct _Unwind_Context*);

#ifdef __cplusplus
}
#endif

#endif /* _UNWIND_H */
EOF

# pthread.h stub (single-threaded)
cat > pthread_stub.h <<'EOF'
/* Minimal pthread stub for single-threaded OP-TEE TA */
#ifndef _PTHREAD_STUB_H
#define _PTHREAD_STUB_H

#include <stddef.h>
#include <stdio.h>

typedef int pthread_key_t;
typedef int pthread_once_t;
typedef struct { int dummy; } pthread_mutex_t;
typedef struct { int dummy; } pthread_cond_t;

#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_INITIALIZER { 0 }

// Declare weak pthread functions
__attribute__((weak)) int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
__attribute__((weak)) int pthread_once(pthread_once_t* once, void (*init)(void));
__attribute__((weak)) int pthread_setspecific(pthread_key_t key, const void* value);
__attribute__((weak)) void* pthread_getspecific(pthread_key_t key);
__attribute__((weak)) int pthread_mutex_lock(pthread_mutex_t* mutex);
__attribute__((weak)) int pthread_mutex_unlock(pthread_mutex_t* mutex);
__attribute__((weak)) int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
__attribute__((weak)) int pthread_cond_signal(pthread_cond_t* cond);

// stdio stub  
static inline int fprintf(FILE* stream, const char* format, ...) {
    (void)stream; (void)format;
    return 0;
}

// Atomic operations (single-threaded)
#define ATOMIC_SWAP(ptr, val) ({ \
    __typeof__(*(ptr)) _old = *(ptr); \
    *(ptr) = (val); \
    _old; \
})

#define ATOMIC_LOAD(ptr) (*(ptr))

#endif /* _PTHREAD_STUB_H */
EOF

# Stdlib wrapper to use musl's stdlib.h and avoid OP-TEE's fallthrough macro
cat > stdlib_wrapper.h <<WRAPPEREOF
#ifndef _STDLIB_WRAPPER_H
#define _STDLIB_WRAPPER_H

/* Use musl's stdlib.h which doesn't have fallthrough conflicts */
#include "$MUSL_BUILD_DIR/include/stdlib.h"

/* Declare TEE functions that might be needed */
#include <stddef.h>
#include <string.h>

#endif /* _STDLIB_WRAPPER_H */
WRAPPEREOF

# pthread.c with weak implementations
cat > pthread_stub.c <<'EOF'
#include "pthread_stub.h"

// Thread-local storage for single-threaded TA
#define MAX_TLS_KEYS 16
static void* _tls_data[MAX_TLS_KEYS] = {0};
static int _next_key = 0;

__attribute__((weak))
int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    (void)destructor;
    *key = _next_key++;
    return 0;
}

__attribute__((weak))
int pthread_once(pthread_once_t* once, void (*init)(void)) {
    if (*once == 0) {
        init();
        *once = 1;
    }
    return 0;
}

__attribute__((weak))
int pthread_setspecific(pthread_key_t key, const void* value) {
    if (key >= 0 && key < MAX_TLS_KEYS) {
        _tls_data[key] = (void*)value;
        return 0;
    }
    return -1;
}

__attribute__((weak))
void* pthread_getspecific(pthread_key_t key) {
    if (key >= 0 && key < MAX_TLS_KEYS) {
        return _tls_data[key];
    }
    return ((void*)0);
}

__attribute__((weak))
int pthread_mutex_lock(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

__attribute__((weak))
int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

__attribute__((weak))
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    (void)cond; (void)mutex;
    return 0;
}

__attribute__((weak))
int pthread_cond_signal(pthread_cond_t* cond) {
    (void)cond;
    return 0;
}
EOF

echo -e "${GREEN}✓ Stub headers created${NC}"
echo ""

# ============================================
# Step 2: Create unwind stubs implementation
# ============================================
echo -e "${YELLOW}Step 2: Creating unwind stubs...${NC}"

cat > unwind_stubs.c <<'EOF'
#include <tee_internal_api.h>
#include "unwind.h"

/* Stub implementations - will terminate on actual exception */
_Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception* exc) {
    DMSG("ERROR: Exception raised but no unwinder! Class: 0x%llx", exc->exception_class);
    TEE_Panic(0xEC000001);
    return _URC_FATAL_PHASE1_ERROR;
}

void _Unwind_Resume(struct _Unwind_Exception* exc) {
    DMSG("ERROR: _Unwind_Resume called!");
    TEE_Panic(0xEC000002);
}

void _Unwind_DeleteException(struct _Unwind_Exception* exc) {
    /* No-op for now */
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

unsigned long _Unwind_GetDataRelBase(struct _Unwind_Context* ctx) {
    return 0;
}

unsigned long _Unwind_GetTextRelBase(struct _Unwind_Context* ctx) {
    return 0;
}
EOF

echo -n "  Compiling unwind_stubs.c... "
if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" -I. \
    -c unwind_stubs.c -o obj_cxxrt/unwind_stubs.o 2>unwind_stubs.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat unwind_stubs.log
    exit 1
fi

echo -n "  Compiling pthread_stub.c... "
if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" -I. \
    -c pthread_stub.c -o obj_cxxrt/pthread_stub.o 2>pthread_stub.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat pthread_stub.log
    exit 1
fi
echo ""

# ============================================
# Step 3: Patch libcxxrt exception.cc
# ============================================
echo -e "${YELLOW}Step 3: Patching libcxxrt exception.cc...${NC}"

# Use Python script to patch exception.cc
python3 ../patch_exception.py "$LIBCXXRT_SRC/src/exception.cc" patched_sources/exception.cc

# Copy other sources as-is
cp "$LIBCXXRT_SRC/src/stdexcept.cc" patched_sources/
cp "$LIBCXXRT_SRC/src/typeinfo.cc" patched_sources/
cp "$LIBCXXRT_SRC/src/memory.cc" patched_sources/
cp "$LIBCXXRT_SRC/src/auxhelper.cc" patched_sources/
cp "$LIBCXXRT_SRC/src/guard.cc" patched_sources/
cp "$LIBCXXRT_SRC/src/dynamic_cast.cc" patched_sources/
cp "$LIBCXXRT_SRC/src/libelftc_dem_gnu3.c" patched_sources/

echo ""

# ============================================
# Step 4: Build libcxxrt
# ============================================
echo -e "${YELLOW}Step 4: Building libcxxrt...${NC}"

CXXRT_SOURCES=(
    "exception.cc"
    "stdexcept.cc"
    "typeinfo.cc"
    "memory.cc"
    "auxhelper.cc"
    "guard.cc"
    "dynamic_cast.cc"
    "libelftc_dem_gnu3.c"
)

compiled_count=0
failed_count=0

for src in "${CXXRT_SOURCES[@]}"; do
    obj_name=$(basename "$src" | sed 's/\.[^.]*$/.o/')
    
    echo -n "  Compiling $src... "
    
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
        -c "patched_sources/$src" -o "obj_cxxrt/$obj_name" 2>"obj_cxxrt/${obj_name}.log"; then
        echo -e "${GREEN}OK${NC}"
        ((compiled_count++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "See: obj_cxxrt/${obj_name}.log"
        tail -30 "obj_cxxrt/${obj_name}.log"
        ((failed_count++))
    fi
done

echo ""
echo -e "Compiled: ${GREEN}${compiled_count}/${#CXXRT_SOURCES[@]}${NC} files"

if [ $compiled_count -eq 0 ]; then
    echo -e "${RED}ERROR: No libcxxrt objects compiled!${NC}"
    exit 1
fi

echo -n "Creating libcxxrt.a... "
${CROSS_COMPILE}ar rcs libcxxrt.a obj_cxxrt/*.o
echo -e "${GREEN}OK ($(ls -lh libcxxrt.a | awk '{print $5}'))${NC}"
echo ""

# ============================================
# Step 5: Build libcxx exception support
# ============================================
echo -e "${YELLOW}Step 5: Building libcxx exception support...${NC}"

LIBCXX_SOURCES=(
    "exception.cpp"
    "stdexcept.cpp"
    "new.cpp"
    "typeinfo.cpp"
)

compiled_count=0
failed_count=0

for src in "${LIBCXX_SOURCES[@]}"; do
    obj_name=$(basename "$src" .cpp).o
    
    echo -n "  Compiling $src... "
    if ${CROSS_COMPILE}g++ \
        "${CXX_FLAGS[@]}" \
        -fexceptions \
        -I. \
        -I"$LIBCXX_SRC/src" \
        -I"$LIBCXXRT_SRC/src" \
        -DLIBCXXRT \
        -D_LIBCPP_BUILDING_LIBRARY \
        -c "$LIBCXX_SRC/src/$src" -o "obj_libcxx/$obj_name" 2>"obj_libcxx/${obj_name}.log"; then
        echo -e "${GREEN}OK${NC}"
        ((compiled_count++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "See: obj_libcxx/${obj_name}.log"
        tail -20 "obj_libcxx/${obj_name}.log"
        ((failed_count++))
    fi
done

echo ""
echo -e "Compiled: ${GREEN}${compiled_count}/${#LIBCXX_SOURCES[@]}${NC} files"

if [ $compiled_count -gt 0 ]; then
    echo -n "Creating libcxx_exceptions.a... "
    ${CROSS_COMPILE}ar rcs libcxx_exceptions.a obj_libcxx/*.o
    echo -e "${GREEN}OK ($(ls -lh libcxx_exceptions.a | awk '{print $5}'))${NC}"
fi
echo ""

# ============================================
# Summary
# ============================================
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}BUILD COMPLETE!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Libraries created:"
ls -lh *.a 2>/dev/null || echo "Check errors above"
echo ""
echo -e "${GREEN}For EVM integration, update ta/sub.mk:${NC}"
echo ""
echo "  # Enable exceptions"
echo "  cppflags-y := \$(filter-out -fno-exceptions,\$(cppflags-y))"
echo "  cppflags-y := \$(filter-out -fno-rtti,\$(cppflags-y))"
echo "  cppflags-y += -fexceptions -frtti"
echo ""
echo "  # Link exception libraries"
echo "  libnames += cxx_exceptions cxxrt musl cxx"
echo "  libdirs += ../build_evm_exceptions"
echo "  libdeps += ../build_evm_exceptions/libcxx_exceptions.a"
echo "  libdeps += ../build_evm_exceptions/libcxxrt.a"
echo ""
echo -e "${YELLOW}Note: Exceptions will PANIC if thrown (no unwinder)${NC}"
echo -e "${YELLOW}EVM must catch ALL exceptions before they propagate!${NC}"
echo ""
