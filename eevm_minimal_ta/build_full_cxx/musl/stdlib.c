/* Standalone stdlib with dlmalloc - no external dependencies */
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;

#define HAVE_MMAP 0
#define LACKS_UNISTD_H
#define LACKS_SYS_PARAM_H
#define LACKS_SYS_TYPES_H
#define LACKS_TIME_H
#define MORECORE dlmalloc_sbrk
#define ABORT do { while(1); } while(0)
#define USE_DL_PREFIX
#define LACKS_STDLIB_H
#define LACKS_STRING_H
#define LACKS_ERRNO_H
#define USE_LOCKS 0
#define NO_MALLOC_STATS 1

static int _errno_val = 0;
#define errno _errno_val
#define ENOMEM 12
#define EINVAL 22

extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);

static unsigned char _heap[1024*1024*4];
static size_t _heap_used = 0;

void* dlmalloc_sbrk(ptrdiff_t increment) {
    if (increment < 0) return (void*)-1;
    if (_heap_used + increment > sizeof(_heap)) return (void*)-1;
    void* result = _heap + _heap_used;
    _heap_used += increment;
    return result;
}

#include "/home/abc/nhat/optee_examples/eevm_minimal_ta/external/openenclave/3rdparty/dlmalloc/dlmalloc/malloc.c"

void* malloc(size_t size) { return dlmalloc(size); }
void free(void* ptr) { dlfree(ptr); }
void* calloc(size_t nmemb, size_t size) { return dlcalloc(nmemb, size); }
void* realloc(void* ptr, size_t size) { return dlrealloc(ptr, size); }

void abort(void) { while(1); }
void exit(int status) { while(1); }
int atexit(void (*func)(void)) { return 0; }
