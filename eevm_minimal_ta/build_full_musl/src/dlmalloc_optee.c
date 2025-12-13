// dlmalloc for OP-TEE - use compiler builtins, not musl headers
#include <tee_internal_api.h>

// Use compiler builtins instead of stddef.h
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INT8_TYPE__ int8_t;
typedef __UINT8_TYPE__ uint8_t;

#define HAVE_MMAP 0
#define LACKS_UNISTD_H
#define LACKS_SYS_PARAM_H  
#define LACKS_SYS_TYPES_H
#define LACKS_TIME_H
#define MORECORE dlmalloc_sbrk
#define ABORT TEE_Panic(0xDEADC0DE)
#define USE_DL_PREFIX
#define LACKS_STDLIB_H
#define LACKS_STRING_H
#define LACKS_ERRNO_H
#define USE_LOCKS 0
#define NO_MALLOC_STATS 1

// errno stub
static int _errno_val = 0;
#define errno _errno_val
#define ENOMEM 12
#define EINVAL 22

static inline void* _memcpy(void* dest, const void* src, size_t n) {
    TEE_MemMove(dest, src, n);
    return dest;
}
static inline void* _memset(void* s, int c, size_t n) {
    TEE_MemFill(s, (uint8_t)c, n);
    return s;
}
#define memcpy _memcpy
#define memset _memset

static uint8_t* _heap = NULL;
static size_t _heap_size = 0;
static size_t _heap_used = 0;

void* dlmalloc_sbrk(ptrdiff_t increment) {
    if (!_heap) {
        _heap_size = 2 * 1024 * 1024; // 2MB heap
        _heap = (uint8_t*)TEE_Malloc(_heap_size, 0);
        if (!_heap) return (void*)-1;
        _heap_used = 0;
    }
    if (increment < 0) return (void*)-1;
    if (_heap_used + increment > _heap_size) return (void*)-1;
    void* ptr = _heap + _heap_used;
    _heap_used += increment;
    return ptr;
}

#include "/home/abc/nhat/optee_examples/eevm_minimal_ta/external/openenclave/3rdparty/dlmalloc/dlmalloc/malloc.c"
