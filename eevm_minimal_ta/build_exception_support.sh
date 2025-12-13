#!/bin/bash
# Build FULL Exception Support for OP-TEE
# Port libunwind + libcxxrt + libcxx from OpenEnclave
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
OPENENCLAVE_ROOT="$PROJECT_ROOT/external/openenclave"
LIBUNWIND_SRC="$OPENENCLAVE_ROOT/3rdparty/libunwind/libunwind"
LIBCXXRT_SRC="$OPENENCLAVE_ROOT/3rdparty/libcxxrt/libcxxrt"
LIBCXX_SRC="$OPENENCLAVE_ROOT/3rdparty/libcxx/libcxx"
BUILD_DIR="$PROJECT_ROOT/build_exception_support"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
TA_DEV_KIT_DIR="${TA_DEV_KIT_DIR:-/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64}"

export PATH=/home/abc/arm-toolchain/bin:$PATH

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Build Exception Support for OP-TEE${NC}"
echo -e "${BLUE}libunwind + libcxxrt + libcxx${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check prerequisites
if [ ! -d "$LIBUNWIND_SRC" ]; then
    echo -e "${RED}ERROR: libunwind not found!${NC}"
    exit 1
fi

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
# Part 1: Build libunwind (Stack Unwinding)
# ============================================
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Part 1: libunwind (Stack Unwinding)${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""

mkdir -p libunwind_obj

# Copy headers
echo "Copying libunwind headers..."
mkdir -p include/libunwind
cp -r "$LIBUNWIND_SRC/include/"* include/libunwind/
cp "$LIBUNWIND_SRC/include/libunwind-aarch64.h" include/libunwind.h
mkdir -p include/tdep
cp -r "$LIBUNWIND_SRC/include/tdep-aarch64/"* include/tdep/

# Generate config files
cat > config.h <<'EOF'
/* Empty config for freestanding */
EOF

cat > include/libunwind-common.h <<'EOF'
#ifndef _LIBUNWIND_COMMON_H
#define _LIBUNWIND_COMMON_H

#define UNW_LOCAL_ONLY 1
#define UNW_VERSION_MAJOR 1
#define UNW_VERSION_MINOR 3

/* Basic types */
typedef unsigned long unw_word_t;
typedef long unw_sword_t;

#endif /* _LIBUNWIND_COMMON_H */
EOF

UNWIND_INCLUDES=(
    -I"$LIBUNWIND_SRC/include"
    -I"$LIBUNWIND_SRC/src"
    -I"$LIBUNWIND_SRC/src/aarch64"
    -I./include
    -I./include/tdep
)

UNWIND_DEFINES=(
    -DHAVE_ELF_H
    -DHAVE_ENDIAN_H
    -DHAVE_LINK_H
    -D_GNU_SOURCE
    -DUNW_LOCAL_ONLY=1
    -DHAVE_DL_ITERATE_PHDR
    -DPACKAGE_STRING=\"libunwind-1.3\"
)

# List of source files (aarch64 subset)
UNWIND_SOURCES=(
    # DWARF unwinding
    "src/dwarf/global.c"
    "src/dwarf/Lexpr.c"
    "src/dwarf/Lfde.c"
    "src/dwarf/Lfind_proc_info-lsb.c"
    "src/dwarf/Lparser.c"
    "src/dwarf/Lpe.c"
    
    # Machine independent
    "src/mi/_ReadULEB.c"
    "src/mi/_ReadSLEB.c"
    "src/mi/backtrace.c"
    "src/mi/flush_cache.c"
    "src/mi/init.c"
    "src/mi/Ldestroy_addr_space.c"
    "src/mi/Lget_fpreg.c"
    "src/mi/Lget_proc_info_by_ip.c"
    "src/mi/Lget_reg.c"
    "src/mi/Lput_dynamic_unwind_info.c"
    "src/mi/Lset_fpreg.c"
    "src/mi/Lset_reg.c"
    "src/mi/mempool.c"
    "src/mi/strerror.c"
    
    # Unwind API
    "src/unwind/Backtrace.c"
    "src/unwind/DeleteException.c"
    "src/unwind/FindEnclosingFunction.c"
    "src/unwind/ForcedUnwind.c"
    "src/unwind/GetCFA.c"
    "src/unwind/GetIPInfo.c"
    "src/unwind/GetIP.c"
    "src/unwind/GetLanguageSpecificData.c"
    "src/unwind/GetRegionStart.c"
    "src/unwind/RaiseException.c"
    "src/unwind/Resume.c"
    "src/unwind/Resume_or_Rethrow.c"
    "src/unwind/SetGR.c"
    "src/unwind/SetIP.c"
    
    # ARM64 specific
    "src/aarch64/is_fpreg.c"
    "src/aarch64/Lcreate_addr_space.c"
    "src/aarch64/Lget_save_loc.c"
    "src/aarch64/Lglobal.c"
    "src/aarch64/Linit.c"
    "src/aarch64/Linit_local.c"
    "src/aarch64/Linit_remote.c"
    "src/aarch64/Lget_proc_info.c"
    "src/aarch64/Lregs.c"
    "src/aarch64/Lresume.c"
    "src/aarch64/Lstep.c"
    "src/aarch64/regname.c"
    
    # OS specific
    "src/os-linux.c"
    "src/elf64.c"
)

echo "Building libunwind sources..."
compiled_count=0
failed_count=0

for src in "${UNWIND_SOURCES[@]}"; do
    obj_name=$(basename "$src" .c).o
    obj_path="libunwind_obj/$obj_name"
    
    echo -n "  Compiling $(basename $src)... "
    if ${CROSS_COMPILE}gcc \
        "${COMMON_FLAGS[@]}" \
        "${UNWIND_INCLUDES[@]}" \
        "${UNWIND_DEFINES[@]}" \
        -c "$LIBUNWIND_SRC/$src" -o "$obj_path" 2>"libunwind_obj/${obj_name}.log"; then
        echo -e "${GREEN}OK${NC}"
        ((compiled_count++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "Error log: libunwind_obj/${obj_name}.log"
        ((failed_count++))
    fi
done

# Build assembly files
echo -n "  Compiling getcontext.S... "
if ${CROSS_COMPILE}gcc \
    "${COMMON_FLAGS[@]}" \
    "${UNWIND_INCLUDES[@]}" \
    -c "$LIBUNWIND_SRC/src/aarch64/getcontext.S" -o libunwind_obj/getcontext.o 2>libunwind_obj/getcontext.log; then
    echo -e "${GREEN}OK${NC}"
    ((compiled_count++))
else
    echo -e "${RED}FAIL${NC}"
    ((failed_count++))
fi

echo ""
echo -e "Compiled: ${GREEN}${compiled_count}${NC} files"
if [ $failed_count -gt 0 ]; then
    echo -e "Failed: ${RED}${failed_count}${NC} files"
    echo "Continuing with available objects..."
fi

# Create libunwind.a
echo -n "Creating libunwind.a... "
${CROSS_COMPILE}ar rcs libunwind.a libunwind_obj/*.o
echo -e "${GREEN}OK${NC}"
ls -lh libunwind.a
echo ""

# ============================================
# Part 2: Build libcxxrt (C++ ABI Runtime)
# ============================================
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Part 2: libcxxrt (C++ ABI Runtime)${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""

mkdir -p libcxxrt_obj

CXXRT_SOURCES=(
    "src/dynamic_cast.cc"
    "src/exception.cc"
    "src/guard.cc"
    "src/stdexcept.cc"
    "src/typeinfo.cc"
    "src/memory.cc"
    "src/auxhelper.cc"
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
        -I"$LIBCXXRT_SRC/src" \
        -D_GNU_SOURCE \
        -c "$LIBCXXRT_SRC/$src" -o "$obj_path" 2>"libcxxrt_obj/${obj_name}.log"; then
        echo -e "${GREEN}OK${NC}"
        ((compiled_count++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "Error log: libcxxrt_obj/${obj_name}.log"
        cat "libcxxrt_obj/${obj_name}.log" | head -20
        ((failed_count++))
    fi
done

echo ""
echo -e "Compiled: ${GREEN}${compiled_count}${NC} files"
if [ $failed_count -gt 0 ]; then
    echo -e "Failed: ${RED}${failed_count}${NC} files"
fi

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
# Part 3: Build libcxx (C++ STL with exceptions)
# ============================================
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Part 3: libcxx (C++ STL Implementation)${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""

mkdir -p libcxx_obj

# Critical exception-related sources
LIBCXX_SOURCES=(
    "src/exception.cpp"
    "src/stdexcept.cpp"
    "src/new.cpp"
    "src/typeinfo.cpp"
    "src/string.cpp"
    "src/vector.cpp"
    "src/algorithm.cpp"
    "src/any.cpp"
    "src/bind.cpp"
    "src/chrono.cpp"
    "src/debug.cpp"
    "src/functional.cpp"
    "src/hash.cpp"
    "src/ios.cpp"
    "src/iostream.cpp"
    "src/locale.cpp"
    "src/memory.cpp"
    "src/optional.cpp"
    "src/random.cpp"
    "src/regex.cpp"
    "src/system_error.cpp"
    "src/utility.cpp"
    "src/valarray.cpp"
    "src/variant.cpp"
)

echo "Building libcxx sources..."
compiled_count=0
failed_count=0

for src in "${LIBCXX_SOURCES[@]}"; do
    obj_name=$(basename "$src" .cpp).o
    obj_path="libcxx_obj/$obj_name"
    
    echo -n "  Compiling $(basename $src)... "
    if ${CROSS_COMPILE}g++ \
        "${CXX_FLAGS[@]}" \
        -fexceptions \
        -I"$LIBCXX_SRC/src" \
        -DLIBCXXRT \
        -D_LIBCPP_BUILDING_LIBRARY \
        -c "$LIBCXX_SRC/$src" -o "$obj_path" 2>"libcxx_obj/${obj_name}.log"; then
        echo -e "${GREEN}OK${NC}"
        ((compiled_count++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "Error log: libcxx_obj/${obj_name}.log"
        cat "libcxx_obj/${obj_name}.log" | head -10
        ((failed_count++))
    fi
done

echo ""
echo -e "Compiled: ${GREEN}${compiled_count}${NC} files"
if [ $failed_count -gt 0 ]; then
    echo -e "Failed: ${RED}${failed_count}${NC} files"
fi

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
echo -e "${GREEN}Link order (IMPORTANT):${NC}"
echo "  1. libcxx_exceptions.a  (C++ STL with exception support)"
echo "  2. libcxxrt.a           (C++ ABI runtime)"
echo "  3. libunwind.a          (Stack unwinding)"
echo "  4. libmusl.a            (C runtime)"
echo "  5. libcxx.a             (C++ operators)"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "  1. Update ta/sub.mk:"
echo "     - Remove -fno-exceptions -fno-rtti"
echo "     - Add -fexceptions -frtti"
echo "     - Link: libcxx_exceptions libcxxrt libunwind libmusl libcxx"
echo ""
echo "  2. Test with std::map and try-catch!"
echo ""
