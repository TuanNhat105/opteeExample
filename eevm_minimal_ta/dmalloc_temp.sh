#!/bin/bash

# Build DLMalloc directly for OP-TEE (without OpenEnclave dependencies)



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

DLMALLOC_SRC="$SCRIPT_DIR/external/openenclave/3rdparty/dlmalloc/dlmalloc"

BUILD_DIR="$SCRIPT_DIR/build_dlmalloc"



# Toolchain

export CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"

CC="${CROSS_COMPILE}gcc"

AR="${CROSS_COMPILE}ar"



# Check if dlmalloc source exists

if [ ! -f "$DLMALLOC_SRC/malloc.c" ]; then

echo -e "${RED}ERROR: DLMalloc source not found at $DLMALLOC_SRC/malloc.c${NC}"

exit 1

fi



# Create build directory

mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR"



echo -e "\n${YELLOW}Creating wrapper for DLMalloc...${NC}"



# Create a simple wrapper that includes malloc.c

cat > dlmalloc_wrapper.c << 'EOF'

// DLMalloc wrapper for OP-TEE

#include <stddef.h>

#include <stdint.h>



// Provide errno and error codes

static int __errno_val = 0;

#define errno __errno_val

#define ENOMEM 12

#define EINVAL 22



// Provide memset/memcpy

void* memset(void* s, int c, size_t n) {

unsigned char* p = (unsigned char*)s;

for (size_t i = 0; i < n; i++) {

p[i] = (unsigned char)c;

}

return s;

}



void* memcpy(void* dest, const void* src, size_t n) {

unsigned char* d = (unsigned char*)dest;

const unsigned char* s = (const unsigned char*)src;

for (size_t i = 0; i < n; i++) {

d[i] = s[i];

}

return dest;

}



// Configuration for dlmalloc

#define HAVE_MMAP 0

#define LACKS_UNISTD_H

#define LACKS_SYS_PARAM_H

#define LACKS_SYS_TYPES_H

#define LACKS_TIME_H

#define LACKS_FCNTL_H

#define LACKS_SYS_MMAN_H

#define LACKS_STRINGS_H

#define LACKS_STRING_H

#define LACKS_SYS_TYPES_H

#define LACKS_ERRNO_H

#define LACKS_STDLIB_H

#define USE_LOCKS 0

#define NO_MALLOC_STATS 1

#define USE_DL_PREFIX

#define MORECORE dlmalloc_sbrk

#define ABORT do { while(1); } while(0)



// Heap management

static unsigned char heap_memory[8 * 1024 * 1024]; // 8MB heap

static unsigned char* heap_next = heap_memory;

static unsigned char* heap_end = heap_memory + sizeof(heap_memory);



void* dlmalloc_sbrk(ptrdiff_t increment) {

unsigned char* old = heap_next;


if (increment < 0) {

return (void*)-1;

}


if (heap_next + increment > heap_end) {

return (void*)-1;

}


heap_next += increment;

return old;

}



// Include actual dlmalloc

#include "../external/openenclave/3rdparty/dlmalloc/dlmalloc/malloc.c"



// Export standard functions without dl prefix

void* malloc(size_t size) {

return dlmalloc(size);

}



void free(void* ptr) {

dlfree(ptr);

}



void* calloc(size_t nmemb, size_t size) {

return dlcalloc(nmemb, size);

}



void* realloc(void* ptr, size_t size) {

return dlrealloc(ptr, size);

}



void* memalign(size_t alignment, size_t bytes) {

return dlmemalign(alignment, bytes);

}



int posix_memalign(void** memptr, size_t alignment, size_t size) {

if (!memptr || (alignment & (alignment - 1)) != 0) {

return 22; // EINVAL

}


void* ptr = dlmemalign(alignment, size);

if (!ptr) {

return 12; // ENOMEM

}


*memptr = ptr;

return 0;

}

EOF



echo -e "${GREEN}✓ Created wrapper${NC}"



echo -e "\n${YELLOW}Compiling DLMalloc...${NC}"



$CC -c dlmalloc_wrapper.c \

-o dlmalloc.o \

-I"$SCRIPT_DIR/external/openenclave/3rdparty/dlmalloc" \

-std=c99 \

-fPIC \

-O2 \

-Wno-all \

-fno-strict-aliasing



if [ $? -ne 0 ]; then

echo -e "${RED}ERROR: Failed to compile dlmalloc${NC}"

exit 1

fi



echo -e "${GREEN}✓ Compiled dlmalloc.o${NC}"



# Create static library

echo -e "\n${YELLOW}Creating libdlmalloc.a...${NC}"

$AR rcs libdlmalloc.a dlmalloc.o



if [ $? -ne 0 ]; then

echo -e "${RED}ERROR: Failed to create library${NC}"

exit 1

fi



echo -e "${GREEN}✓ Created libdlmalloc.a ($(stat -c%s libdlmalloc.a) bytes)${NC}"



# Verify symbols

echo -e "\n${YELLOW}Verifying symbols:${NC}"

nm libdlmalloc.a | grep -E " [TW] " | grep -E "malloc|calloc|realloc|free|memalign" || echo " (symbols may be present)"



echo -e "\n${GREEN}========================================${NC}"

echo -e "${GREEN}DLMalloc build completed!${NC}"

echo -e "${GREEN}========================================${NC}"

echo -e "Library: $BUILD_DIR/libdlmalloc.a"

echo -e "\nTo use in leveldb_xapian TA Makefile:"

echo -e " LIBDLMALLOC := \$(EEVM_MINIMAL_TA)/build_dlmalloc/libdlmalloc.a"

echo -e " And add to link: \$(LIBDLMALLOC)"