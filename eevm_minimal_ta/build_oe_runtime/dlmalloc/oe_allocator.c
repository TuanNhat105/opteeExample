// Adapted from OpenEnclave dlmalloc allocator for OP-TEE
#include <stddef.h>
#include <stdint.h>
#include <tee_internal_api.h>

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
#define USE_LOCKS 0
#define NO_MALLOC_STATS 1

// Memory operations using TEE APIs
static inline void* _memcpy(void* dest, const void* src, size_t n) {
    TEE_MemMove(dest, src, n);
    return dest;
}
static inline void* _memset(void* s, int c, size_t n) {
    TEE_MemFill(s, (uint8_t)c, n);
    return s;
}
#define LACKS_STRING_H
#define memcpy _memcpy
#define memset _memset

// Simple sbrk implementation using TEE_Malloc
static uint8_t* _heap = NULL;
static size_t _heap_size = 0;
static size_t _heap_used = 0;

void* dlmalloc_sbrk(ptrdiff_t increment) {
    if (!_heap) {
        // Initialize heap (1MB)
        _heap_size = 1024 * 1024;
        _heap = (uint8_t*)TEE_Malloc(_heap_size, 0);
        if (!_heap) return (void*)-1;
        _heap_used = 0;
    }
    
    if (increment < 0) return (void*)-1; // Don't support shrinking
    
    if (_heap_used + increment > _heap_size) {
        return (void*)-1; // Out of memory
    }
    
    void* ptr = _heap + _heap_used;
    _heap_used += increment;
    return ptr;
}

