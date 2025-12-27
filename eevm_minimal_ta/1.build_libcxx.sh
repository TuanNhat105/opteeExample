#!/bin/bash
# Build ONLY OpenEnclave libcxx + musl headers for OP-TEE
# Removed libcxxrt library build step
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
OE_ROOT="$PROJECT_ROOT/external/openenclave"
BUILD_DIR="$PROJECT_ROOT/build_oe_libs"
CROSS_COMPILE="${CROSS_COMPILE:-}"
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}Build OpenEnclave libc++ for OP-TEE${NC}"
echo -e "${GREEN}(Skipping libcxxrt library build)${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""

# Verify sources
for DIR in "$OE_ROOT/3rdparty/musl/musl" "$OE_ROOT/3rdparty/libcxx/libcxx" "$OE_ROOT/3rdparty/libcxxrt/libcxxrt"; do
    if [ ! -d "$DIR" ]; then
        echo -e "${RED}ERROR: $DIR not found${NC}"
        echo "Please run download scripts first!"
        exit 1
    fi
done

# Clean build
rm -rf "$BUILD_DIR"
# Chỉ tạo thư mục cho musl và libcxx, libcxxrt chỉ cần folder include tạm
mkdir -p "$BUILD_DIR"/{musl,libcxxrt,libcxx}
cd "$BUILD_DIR"

echo -e "${YELLOW}Step 1: Setup musl headers (Needed for libc++)${NC}"

MUSL_SRC="$OE_ROOT/3rdparty/musl/musl"
MUSL_BUILD="$BUILD_DIR/musl"
ARCH="aarch64"

# Copy musl source
cp -r "$MUSL_SRC" "$MUSL_BUILD/src"
cd "$MUSL_BUILD/src"

# Apply OpenEnclave patches
PATCHES_DIR="$OE_ROOT/3rdparty/musl/patches"
cp "$PATCHES_DIR/syscall.h" src/internal/syscall.h
cp "$PATCHES_DIR/syscall_arch.h" "arch/$ARCH/syscall_arch.h"
cp "$PATCHES_DIR/pthread_${ARCH}.h" "arch/$ARCH/pthread_arch.h"
cp "$PATCHES_DIR/setjmp.h" include/setjmp.h
cp "$PATCHES_DIR/execinfo.h" include/execinfo.h
cp "arch/$ARCH/syscall_arch.h" "arch/$ARCH/__syscall_arch.h"
[ -f "arch/generic/fp_arch.h" ] && mkdir -p "src/include" && cp "arch/generic/fp_arch.h" "src/include/fp_arch.h" || true

# Configure musl
MUSL_CFLAGS="-DSYSCALL_NO_INLINE"
./configure --includedir="$MUSL_BUILD/include" CFLAGS="$MUSL_CFLAGS" \
    CC="${CROSS_COMPILE}gcc" CXX="${CROSS_COMPILE}g++" > /dev/null

# Copy generated headers
mkdir -p "$MUSL_BUILD/include/bits"
cp -r include/* "$MUSL_BUILD/include/"
cp -r "arch/generic/bits/"* "$MUSL_BUILD/include/bits/" 2>/dev/null || true
cp -r "arch/$ARCH/bits/"* "$MUSL_BUILD/include/bits/" 2>/dev/null || true

# Generate alltypes.h
mkdir -p "$MUSL_BUILD/include/bits"
sed -f tools/mkalltypes.sed \
    "arch/$ARCH/bits/alltypes.h.in" \
    "include/alltypes.h.in" \
    > "$MUSL_BUILD/include/bits/alltypes.h"

# Generate syscall.h
cp "arch/$ARCH/bits/syscall.h.in" "$MUSL_BUILD/include/bits/syscall.h"
sed -n -e 's/__NR_/SYS_/p' < "arch/$ARCH/bits/syscall.h.in" \
    >> "$MUSL_BUILD/include/bits/syscall.h"

# Endian patch
cp "$MUSL_BUILD/include/endian.h" "$MUSL_BUILD/include/__endian.h"
cp "$PATCHES_DIR/endian.h" "$MUSL_BUILD/include/endian.h"

# Patch math.h
if [ -f "$PROJECT_ROOT/patch_musl_math.sh" ]; then
    chmod +x "$PROJECT_ROOT/patch_musl_math.sh"
    "$PROJECT_ROOT/patch_musl_math.sh" "$MUSL_BUILD/include/math.h"
fi

cd "$BUILD_DIR"
echo -e "${GREEN}✓ Musl headers ready${NC}"
echo ""

echo -e "${YELLOW}Step 2: Setup libcxxrt headers (Required for compiling libc++)${NC}"
# NOTE: Chỉ copy headers để libc++ có cái include (cxxabi.h), không build thư viện .a
LIBCXXRT_SRC="$OE_ROOT/3rdparty/libcxxrt/libcxxrt"
mkdir -p libcxxrt/include
cp "$LIBCXXRT_SRC/src/"*.h libcxxrt/include/ 2>/dev/null || true
echo -e "${GREEN}✓ libcxxrt headers ready${NC}"
echo ""

echo -e "${YELLOW}Step 3: Setup libcxx headers${NC}"

LIBCXX_SRC="$OE_ROOT/3rdparty/libcxx/libcxx"
mkdir -p libcxx/include

# Copy all libcxx headers
cp -r "$LIBCXX_SRC/include/"* libcxx/include/

# OpenEnclave specific config
if [ -f "libcxx/include/__config" ]; then
    cp libcxx/include/__config libcxx/include/__config_original
fi
if [ -f "$OE_ROOT/3rdparty/libcxx/__config" ]; then
    cp "$OE_ROOT/3rdparty/libcxx/__config" libcxx/include/
    sed -i '/extern "C" long long strtoll_l(/,/const char \*__restrict, char \*\*__restrict, int, locale_t loc);/d' libcxx/include/__config
    sed -i '/extern "C" unsigned long long int strtoull_l(/,/const char \*nptr, char \*\*endptr, int base, locale_t loc);/d' libcxx/include/__config
fi

# Copy __dso_handle.cpp
cp "$OE_ROOT/3rdparty/libcxx/__dso_handle.cpp" libcxx/

# Fixes for libcxx build
if [ -f "libcxx/include/cmath" ]; then
    if ! grep -q "// PATCH: Added for numeric_limits" "libcxx/include/cmath"; then
        sed -i '/#include <math\.h>/a // PATCH: Added for numeric_limits\n#include <limits>' "libcxx/include/cmath"
    fi
    if grep -q "^using ::abs;$" "libcxx/include/cmath"; then
        sed -i 's|^using ::abs;$|// PATCH: abs not in musl math.h\n// using ::abs;|' "libcxx/include/cmath"
    fi
fi

# Create dummy linux/version.h
mkdir -p linux
cat > linux/version.h << 'LINUX_VERSION_EOF'
#ifndef _LINUX_VERSION_H
#define _LINUX_VERSION_H
#define LINUX_VERSION_CODE 0x050000
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif
LINUX_VERSION_EOF

echo -e "${GREEN}✓ libcxx headers ready${NC}"
echo ""

# SKIPPING Step 4 (Build libcxxrt) as requested
echo -e "${YELLOW}Step 4: Skipped building libcxxrt.a (User request)${NC}"
echo ""

echo -e "${YELLOW}Step 5: Build libcxx/libc++.a${NC}"

GCC_BUILTIN=$( ${CROSS_COMPILE}gcc -print-file-name=include )

CXXFLAGS=(
    -fPIC
    -nostdinc
    -nostdinc++
    -isystem "$GCC_BUILTIN"
    -I"$MUSL_BUILD/include"
    -I"$BUILD_DIR/libcxxrt/include"
    -I"$BUILD_DIR/libcxx/include"
    -std=c++17
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -DLIBCXXRT
    -D__ELF__
    -D__linux__
    -Wno-all
    -Wno-error
    -D_LIBCPP_HAS_MUSL_LIBC
)

LIBCXX_CXXFLAGS=(
    "${CXXFLAGS[@]}"
    -fno-weak
    -frtti
    -DLIBCXXRT
    -D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
    -D_LIBCPP_BUILDING_LIBRARY
    # -D_LIBCPP_HAS_NO_THREADS
    -U__STDCPP_THREADS__
    -I"$LIBCXX_SRC/src"
    -I"$LIBCXXRT_SRC/src"
    -I.
)

# Complete libcxx sources
LIBCXX_SRCS=(
    "$LIBCXX_SRC/src/algorithm.cpp"
    "$LIBCXX_SRC/src/any.cpp"
    "$LIBCXX_SRC/src/bind.cpp"
    "$LIBCXX_SRC/src/charconv.cpp"
    "$LIBCXX_SRC/src/chrono.cpp"
    "$LIBCXX_SRC/src/condition_variable.cpp"
    "$LIBCXX_SRC/src/condition_variable_destructor.cpp"
    "$LIBCXX_SRC/src/debug.cpp"
    "$LIBCXX_SRC/src/exception.cpp"
    "$LIBCXX_SRC/src/functional.cpp"
    "$LIBCXX_SRC/src/future.cpp"
    "$LIBCXX_SRC/src/hash.cpp"
    "$LIBCXX_SRC/src/ios.cpp"
    "$LIBCXX_SRC/src/iostream.cpp"
    "$LIBCXX_SRC/src/locale.cpp"
    "$LIBCXX_SRC/src/memory.cpp"
    "$LIBCXX_SRC/src/mutex.cpp"
    "$LIBCXX_SRC/src/mutex_destructor.cpp"
    "$LIBCXX_SRC/src/new.cpp"
    "$LIBCXX_SRC/src/optional.cpp"
    "$LIBCXX_SRC/src/random.cpp"
    "$LIBCXX_SRC/src/regex.cpp"
    "$LIBCXX_SRC/src/shared_mutex.cpp"
    "$LIBCXX_SRC/src/stdexcept.cpp"
    "$LIBCXX_SRC/src/string.cpp"
    "$LIBCXX_SRC/src/strstream.cpp"
    "$LIBCXX_SRC/src/system_error.cpp"
    "$LIBCXX_SRC/src/thread.cpp"
    "$LIBCXX_SRC/src/typeinfo.cpp"
    "$LIBCXX_SRC/src/utility.cpp"
    "$LIBCXX_SRC/src/valarray.cpp"
    "$LIBCXX_SRC/src/variant.cpp"
    "$LIBCXX_SRC/src/vector.cpp"
    "$LIBCXX_SRC/src/filesystem/operations.cpp"
    "$LIBCXX_SRC/src/filesystem/int128_builtins.cpp"
    "$LIBCXX_SRC/src/filesystem/directory_iterator.cpp"
)

LIBCXX_OBJS=()
for SRC in "${LIBCXX_SRCS[@]}"; do
    if [ ! -f "$SRC" ]; then continue; fi
    BASE=$(basename "$SRC" .cpp)
    OBJ="libcxx/${BASE}.o"
    
    echo -n "  Building $BASE.o... "
    
    COMPILE_FLAGS=("${LIBCXX_CXXFLAGS[@]}")
    if [ "$BASE" == "future" ]; then
        COMPILE_FLAGS+=(-O0)
    fi
    
    if ${CROSS_COMPILE}g++ "${COMPILE_FLAGS[@]}" \
        -c "$SRC" -o "$OBJ" 2>libcxx/${BASE}.log; then
        echo -e "${GREEN}OK${NC}"
        LIBCXX_OBJS+=("$OBJ")
    else
        echo -e "${YELLOW}SKIP (see libcxx/${BASE}.log)${NC}"
    fi
done

# Build __dso_handle
echo -n "  Building __dso_handle.o... "
if ${CROSS_COMPILE}g++ "${LIBCXX_CXXFLAGS[@]}" \
    -c libcxx/__dso_handle.cpp -o libcxx/__dso_handle.o 2>libcxx/__dso_handle.log; then
    echo -e "${GREEN}OK${NC}"
    LIBCXX_OBJS+=("libcxx/__dso_handle.o")
else
    echo -e "${YELLOW}SKIP${NC}"
fi

if [ ${#LIBCXX_OBJS[@]} -gt 0 ]; then
    ${CROSS_COMPILE}ar rcs libcxx/libc++.a "${LIBCXX_OBJS[@]}"
    ${CROSS_COMPILE}ranlib libcxx/libc++.a
    echo -e "${GREEN}✓ libc++.a created (${#LIBCXX_OBJS[@]} objects)${NC}"
else
    echo -e "${RED}✗ No libcxx objects built!${NC}"
    exit 1
fi
echo ""

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}✓ Build Complete!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "Output:"
echo "  Headers (Include these in TA):"
echo "    - Musl:    $BUILD_DIR/musl/include/"
echo "    - Libcxx:  $BUILD_DIR/libcxx/include/"
echo ""
echo "  Libraries (Link this):"
echo "    - Libcxx:   $BUILD_DIR/libcxx/libc++.a ($(${CROSS_COMPILE}size -t $BUILD_DIR/libcxx/libc++.a 2>/dev/null | tail -1 | awk '{print $1}' || echo '?') bytes)"
echo ""
echo "Note: libcxxrt.a was NOT built."