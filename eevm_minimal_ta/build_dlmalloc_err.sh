#!/bin/bash
# Build DLMalloc for OP-TEE TA
# This provides proper malloc/free implementation that musl expects

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Building DLMalloc for OP-TEE${NC}"
echo -e "${GREEN}========================================${NC}"
export PATH=/home/abc/arm-toolchain/bin:$PATH
# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DLMALLOC_SRC="$SCRIPT_DIR/external/openenclave/3rdparty/dlmalloc"
OE_INCLUDE="$SCRIPT_DIR/external/openenclave/include"
BUILD_DIR="$SCRIPT_DIR/build_dlmalloc"
MUSL_INCLUDE="$SCRIPT_DIR/build_oe_libs/musl/include"

# Toolchain
export CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
CC="${CROSS_COMPILE}gcc"
AR="${CROSS_COMPILE}ar"

# Check if dlmalloc source exists
if [ ! -f "$DLMALLOC_SRC/allocator.c" ]; then
    echo -e "${RED}ERROR: DLMalloc source not found at $DLMALLOC_SRC${NC}"
    exit 1
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo -e "\n${YELLOW}Compiling DLMalloc allocator...${NC}"

# Compile allocator.c with OP-TEE compatible flags
$CC -c "$DLMALLOC_SRC/allocator.c" \
    -o allocator.o \
    -I"$OE_INCLUDE" \
    -I"$MUSL_INCLUDE" \
    -I"$DLMALLOC_SRC" \
    -DHAVE_MMAP=0 \
    -DLACKS_UNISTD_H \
    -DLACKS_SYS_PARAM_H \
    -DLACKS_SYS_TYPES_H \
    -DLACKS_TIME_H \
    -DLACKS_STDLIB_H \
    -DLACKS_STRING_H \
    -DUSE_LOCKS=0 \
    -DNO_MALLOC_STATS=1 \
    -DUSE_DL_PREFIX \
    -DOE_TRUSTZONE=1 \
    -std=c99 \
    -fPIC \
    -O2 \
    -Wall \
    -Wno-conversion \
    -Wno-null-pointer-arithmetic \
    -Wno-unused-function \
    -Wno-unused-variable

if [ $? -ne 0 ]; then
    echo -e "${RED}ERROR: Failed to compile allocator.c${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Compiled allocator.o${NC}"

# Create static library
echo -e "\n${YELLOW}Creating libdlmalloc.a...${NC}"
$AR rcs libdlmalloc.a allocator.o

if [ $? -ne 0 ]; then
    echo -e "${RED}ERROR: Failed to create library${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Created libdlmalloc.a${NC}"

# Verify symbols
echo -e "\n${YELLOW}Verifying malloc symbols...${NC}"
nm libdlmalloc.a | grep -E "malloc|calloc|realloc|free" | grep " T " | head -10

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}DLMalloc build completed!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Library: $BUILD_DIR/libdlmalloc.a"
echo -e "\nTo use in your TA Makefile, add:"
echo -e "  LIBDLMALLOC := \$(EEVM_MINIMAL_TA)/build_dlmalloc/libdlmalloc.a"
echo -e "  And link it: \$(LIBDLMALLOC)"
