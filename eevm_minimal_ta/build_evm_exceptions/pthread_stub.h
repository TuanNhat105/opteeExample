/* Minimal pthread stub for single-threaded OP-TEE TA */
#ifndef _PTHREAD_STUB_H
#define _PTHREAD_STUB_H

#include <stddef.h>
#include <stdio.h>

typedef int pthread_key_t;
typedef int pthread_once_t;
typedef struct { int dummy; } pthread_mutex_t;
typedef struct { int dummy; } pthread_cond_t;

#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_INITIALIZER { 0 }

// Declare weak pthread functions
__attribute__((weak)) int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
__attribute__((weak)) int pthread_once(pthread_once_t* once, void (*init)(void));
__attribute__((weak)) int pthread_setspecific(pthread_key_t key, const void* value);
__attribute__((weak)) void* pthread_getspecific(pthread_key_t key);
__attribute__((weak)) int pthread_mutex_lock(pthread_mutex_t* mutex);
__attribute__((weak)) int pthread_mutex_unlock(pthread_mutex_t* mutex);
__attribute__((weak)) int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
__attribute__((weak)) int pthread_cond_signal(pthread_cond_t* cond);

// stdio stub  
static inline int fprintf(FILE* stream, const char* format, ...) {
    (void)stream; (void)format;
    return 0;
}

// Atomic operations (single-threaded)
#define ATOMIC_SWAP(ptr, val) ({ \
    __typeof__(*(ptr)) _old = *(ptr); \
    *(ptr) = (val); \
    _old; \
})

#define ATOMIC_LOAD(ptr) (*(ptr))

#endif /* _PTHREAD_STUB_H */
