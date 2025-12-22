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
