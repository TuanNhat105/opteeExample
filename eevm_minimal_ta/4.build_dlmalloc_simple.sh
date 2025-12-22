#!/bin/bash
# Build DLMalloc with PREFIXED names to avoid OP-TEE conflicts

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Building DLMalloc (Renamed to oe_*)${NC}"
echo -e "${GREEN}========================================${NC}"

export PATH=/home/abc/arm-toolchain/bin:$PATH

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DLMALLOC_SRC="$SCRIPT_DIR/external/openenclave/3rdparty/dlmalloc/dlmalloc"
BUILD_DIR="$SCRIPT_DIR/build_dlmalloc"

export CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
CC="${CROSS_COMPILE}gcc"
AR="${CROSS_COMPILE}ar"

if [ ! -f "$DLMALLOC_SRC/malloc.c" ]; then
    echo -e "${RED}ERROR: Source not found${NC}"
    exit 1
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo -e "\n${YELLOW}Creating wrapper with oe_ prefix...${NC}"

cat > dlmalloc_wrapper.c << 'EOF'
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int __errno_val = 0;
#define errno __errno_val
#define ENOMEM 12
#define EINVAL 22

#define HAVE_MMAP 0
#define LACKS_UNISTD_H
#define LACKS_SYS_PARAM_H  
#define LACKS_SYS_TYPES_H
#define LACKS_TIME_H
#define LACKS_FCNTL_H
#define LACKS_SYS_MMAN_H
#define LACKS_SYS_TYPES_H
#define LACKS_ERRNO_H
#define LACKS_STDLIB_H
#define USE_LOCKS 0
#define NO_MALLOC_STATS 1
#define USE_DL_PREFIX      // Keep internal names as dl*
#define MORECORE dlmalloc_sbrk
#define ABORT do { while(1); } while(0)

// --- HEAP CONFIG (Ensure TA_DATA_SIZE > 8MB) ---
#define HEAP_SIZE (8 * 1024 * 1024)
static unsigned char heap_memory[HEAP_SIZE] __attribute__((aligned(16))); 
static unsigned char* heap_next = heap_memory;
static unsigned char* heap_end = heap_memory + HEAP_SIZE;

void* dlmalloc_sbrk(ptrdiff_t increment) {
    unsigned char* old = heap_next;
    if (increment == 0) return old;
    if (increment < 0) {
        if (heap_next + increment < heap_memory) return (void*)-1;
    } else {
        if (heap_next + increment > heap_end) return (void*)-1;
    }
    heap_next += increment;
    return old;
}

#include "../external/openenclave/3rdparty/dlmalloc/dlmalloc/malloc.c"

// --- EXPORT WITH OE_ PREFIX (Tránh xung đột) ---

void* oe_malloc(size_t size) {
    return dlmalloc(size);
}

void oe_free(void* ptr) {
    dlfree(ptr);
}

void* oe_calloc(size_t nmemb, size_t size) {
    return dlcalloc(nmemb, size);
}

void* oe_realloc(void* ptr, size_t size) {
    return dlrealloc(ptr, size);
}

void* oe_memalign(size_t alignment, size_t bytes) {
    return dlmemalign(alignment, bytes);
}

int oe_posix_memalign(void** memptr, size_t alignment, size_t size) {
    if (!memptr || (alignment & (alignment - 1)) != 0) return EINVAL;
    void* ptr = dlmemalign(alignment, size);
    if (!ptr) return ENOMEM;
    *memptr = ptr;
    return 0;
}
EOF

echo -e "${GREEN}✓ Created wrapper${NC}"

echo -e "\n${YELLOW}Compiling...${NC}"
$CC -c dlmalloc_wrapper.c -o dlmalloc.o -I"$DLMALLOC_SRC" \
    -std=gnu99 -fPIC -O3 -Wall -Wno-unused-function -fno-strict-aliasing

echo -e "\n${YELLOW}Creating library...${NC}"
$AR rcs libdlmalloc.a dlmalloc.o

echo -e "\n${YELLOW}Verifying symbols (Should see T oe_malloc):${NC}"
nm libdlmalloc.a | grep "oe_malloc"

echo -e "\n${GREEN}DONE! Library at: $BUILD_DIR/libdlmalloc.a${NC}"