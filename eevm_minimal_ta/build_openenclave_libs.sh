#!/bin/bash
# Build OpenEnclave libcxx + musl for OP-TEE
# Follow OpenEnclave's CMake approach but use Makefile
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
OE_ROOT="$PROJECT_ROOT/external/openenclave"
BUILD_DIR="$PROJECT_ROOT/build_oe_libs"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
export PATH=/home/abc/arm-toolchain/bin:$PATH

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}Build OpenEnclave libs for OP-TEE${NC}"
echo -e "${GREEN}Following OpenEnclave CMakeLists approach${NC}"
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
mkdir -p "$BUILD_DIR"/{musl,libcxxrt,libcxx}
cd "$BUILD_DIR"

echo -e "${YELLOW}Step 1: Setup musl headers (like OpenEnclave musl/CMakeLists.txt)${NC}"

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
# fp_arch.h might not exist in all musl versions
[ -f "arch/generic/fp_arch.h" ] && mkdir -p "src/include" && cp "arch/generic/fp_arch.h" "src/include/fp_arch.h" || true

# Configure musl
MUSL_CFLAGS="-DSYSCALL_NO_INLINE"
./configure --includedir="$MUSL_BUILD/include" CFLAGS="$MUSL_CFLAGS" \
    CC="${CROSS_COMPILE}gcc" CXX="${CROSS_COMPILE}g++"

# Copy generated headers (OpenEnclave approach)
mkdir -p "$MUSL_BUILD/include/bits"
cp -r include/* "$MUSL_BUILD/include/"
cp -r "arch/generic/bits/"* "$MUSL_BUILD/include/bits/" 2>/dev/null || true
cp -r "arch/$ARCH/bits/"* "$MUSL_BUILD/include/bits/" 2>/dev/null || true

# Generate alltypes.h (musl's special process)
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

# Patch math.h with missing functions for libcxx
chmod +x "$PROJECT_ROOT/patch_musl_math.sh"
"$PROJECT_ROOT/patch_musl_math.sh" "$MUSL_BUILD/include/math.h"

cd "$BUILD_DIR"
echo -e "${GREEN}✓ Musl headers ready ($(find musl/include -name '*.h' | wc -l) files)${NC}"
echo ""

echo -e "${YELLOW}Step 2: Setup libcxxrt headers${NC}"

LIBCXXRT_SRC="$OE_ROOT/3rdparty/libcxxrt/libcxxrt"
mkdir -p libcxxrt/include
cp "$LIBCXXRT_SRC/src/"*.h libcxxrt/include/ 2>/dev/null || true

echo -e "${GREEN}✓ libcxxrt headers ready${NC}"
echo ""

echo -e "${YELLOW}Step 3: Setup libcxx headers (like OpenEnclave libcxx/CMakeLists.txt)${NC}"

LIBCXX_SRC="$OE_ROOT/3rdparty/libcxx/libcxx"
mkdir -p libcxx/include

# Copy all libcxx headers
cp -r "$LIBCXX_SRC/include/"* libcxx/include/

# OpenEnclave specific: backup __config and use custom one
if [ -f "libcxx/include/__config" ]; then
    cp libcxx/include/__config libcxx/include/__config_original
fi
if [ -f "$OE_ROOT/3rdparty/libcxx/__config" ]; then
    cp "$OE_ROOT/3rdparty/libcxx/__config" libcxx/include/
    # Fix: Remove strtoll_l/strtoull_l extern declarations since xlocale.h provides static inline versions
    # This avoids "declared extern and later static" errors
    # Use Python to precisely remove only the function declarations
    python3 <<'PYTHON_SCRIPT'
import sys
import os

config_file = "libcxx/include/__config"
if not os.path.exists(config_file):
    sys.exit(1)

with open(config_file, 'r') as f:
    lines = f.readlines()

# Remove strtoll_l and strtoull_l declarations (2 lines each)
output_lines = []
i = 0
while i < len(lines):
    line = lines[i]
    # Check for strtoll_l declaration start
    if 'extern "C" long long strtoll_l(' in line:
        # Skip this line and the next line (the closing );)
        i += 2
        continue
    # Check for strtoull_l declaration start
    if 'extern "C" unsigned long long int strtoull_l(' in line:
        # Skip this line and the next line (the closing );)
        i += 2
        continue
    output_lines.append(line)
    i += 1

with open(config_file, 'w') as f:
    f.writelines(output_lines)
PYTHON_SCRIPT
fi

# Copy __dso_handle.cpp
cp "$OE_ROOT/3rdparty/libcxx/__dso_handle.cpp" libcxx/

echo -e "${GREEN}✓ libcxx headers ready${NC}"
echo ""

echo -e "${YELLOW}Step 4: Build libcxxrt (minimal for OP-TEE)${NC}"

# Compiler flags matching OpenEnclave + OP-TEE
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

# Build only essential libcxxrt files
LIBCXXRT_SRCS=(
    "$LIBCXXRT_SRC/src/stdexcept.cc"
    "$LIBCXXRT_SRC/src/typeinfo.cc"
    "$LIBCXXRT_SRC/src/guard.cc"
)

LIBCXXRT_OBJS=()
for SRC in "${LIBCXXRT_SRCS[@]}"; do
    if [ ! -f "$SRC" ]; then continue; fi
    BASE=$(basename "$SRC" .cc)
    OBJ="libcxxrt/${BASE}.o"
    
    echo -n "  Building $BASE.o... "
    if ${CROSS_COMPILE}g++ "${CXXFLAGS[@]}" \
        -I"$LIBCXXRT_SRC/src" \
        -c "$SRC" -o "$OBJ" 2>libcxxrt/${BASE}.log; then
        echo -e "${GREEN}OK${NC}"
        LIBCXXRT_OBJS+=("$OBJ")
    else
        echo -e "${YELLOW}SKIP (see libcxxrt/${BASE}.log)${NC}"
    fi
done

if [ ${#LIBCXXRT_OBJS[@]} -gt 0 ]; then
    ${CROSS_COMPILE}ar rcs libcxxrt/libcxxrt.a "${LIBCXXRT_OBJS[@]}"
    echo -e "${GREEN}✓ libcxxrt.a (${#LIBCXXRT_OBJS[@]} objects)${NC}"
else
    echo -e "${YELLOW}⚠ No libcxxrt objects built${NC}"
    touch libcxxrt/libcxxrt.a
fi
echo ""

echo -e "${YELLOW}Step 5: Build libcxx (following OpenEnclave list)${NC}"

# OpenEnclave compilation flags for libcxx
LIBCXX_CXXFLAGS=(
    "${CXXFLAGS[@]}"
    -fno-weak
    -DLIBCXXRT
    -D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
    -D_LIBCPP_BUILDING_LIBRARY
    -D_LIBCPP_HAS_NO_THREADS
    -D_LIBCPP_HAS_NO_EXCEPTIONS
    -U__STDCPP_THREADS__
    -I"$LIBCXX_SRC/src"
    -I"$LIBCXXRT_SRC/src"
)

# Key libcxx sources from OpenEnclave CMakeLists.txt
LIBCXX_SRCS=(
    "$LIBCXX_SRC/src/algorithm.cpp"
    "$LIBCXX_SRC/src/any.cpp"
    "$LIBCXX_SRC/src/bind.cpp"
    "$LIBCXX_SRC/src/charconv.cpp"
    "$LIBCXX_SRC/src/debug.cpp"
    "$LIBCXX_SRC/src/exception.cpp"
    "$LIBCXX_SRC/src/functional.cpp"
    "$LIBCXX_SRC/src/hash.cpp"
    "$LIBCXX_SRC/src/memory.cpp"
    "$LIBCXX_SRC/src/new.cpp"
    "$LIBCXX_SRC/src/optional.cpp"
    "$LIBCXX_SRC/src/stdexcept.cpp"
    "$LIBCXX_SRC/src/string.cpp"
    "$LIBCXX_SRC/src/typeinfo.cpp"
    "$LIBCXX_SRC/src/utility.cpp"
    "$LIBCXX_SRC/src/variant.cpp"
    "$LIBCXX_SRC/src/vector.cpp"
)

LIBCXX_OBJS=()
for SRC in "${LIBCXX_SRCS[@]}"; do
    if [ ! -f "$SRC" ]; then continue; fi
    BASE=$(basename "$SRC" .cpp)
    OBJ="libcxx/${BASE}.o"
    
    echo -n "  Building $BASE.o... "
    if ${CROSS_COMPILE}g++ "${LIBCXX_CXXFLAGS[@]}" \
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
    echo -e "${GREEN}✓ libc++.a (${#LIBCXX_OBJS[@]} objects)${NC}"
else
    echo -e "${RED}✗ No libcxx objects built!${NC}"
    echo "Check log files in $BUILD_DIR/libcxx/*.log"
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 6: Create combined library${NC}"

# Combine everything
mkdir -p combined/obj
cd combined/obj
[ -f ../../libcxxrt/libcxxrt.a ] && ${CROSS_COMPILE}ar x ../../libcxxrt/libcxxrt.a 2>/dev/null || true
[ -f ../../libcxx/libc++.a ] && ${CROSS_COMPILE}ar x ../../libcxx/libc++.a
cd ..
${CROSS_COMPILE}ar rcs libcxx_runtime.a obj/*.o
${CROSS_COMPILE}ranlib libcxx_runtime.a

echo -e "${GREEN}✓ libcxx_runtime.a created${NC}"
echo ""

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}✓ Build Complete!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "Output:"
echo "  Headers:"
echo "    - Musl:    $BUILD_DIR/musl/include/"
echo "    - Libcxx:  $BUILD_DIR/libcxx/include/"
echo "    - Libcxxrt: $BUILD_DIR/libcxxrt/include/"
echo ""
echo "  Libraries:"
echo "    - Combined: $BUILD_DIR/combined/libcxx_runtime.a"
echo "    - Libcxx:   $BUILD_DIR/libcxx/libc++.a"
echo "    - Libcxxrt: $BUILD_DIR/libcxxrt/libcxxrt.a"
echo ""
echo "To use in TA sub.mk:"
echo "  global-incdirs-y += ../build_oe_libs/musl/include"
echo "  global-incdirs-y += ../build_oe_libs/libcxx/include"  
echo "  libdeps += ../build_oe_libs/combined/libcxx_runtime.a"
