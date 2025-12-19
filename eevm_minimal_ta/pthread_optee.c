// pthread.c for OP-TEE - Simplified version from OpenEnclave
// Single-threaded implementation for OP-TEE Trusted Applications

#include <stdint.h>
#include <stddef.h>

// Include musl's pthread_impl.h to get struct pthread definition
#define hidden
#define weak
#define weak_alias(old, new)

// Paths are relative to this file (project root)
#include "build_oe_libs/musl/src/src/internal/pthread_impl.h"
#include "build_oe_libs/musl/src/src/internal/locale_impl.h"

// Thread-local storage for main thread
static __thread struct pthread _pthread_self;
static __thread int _pthread_initialized = 0;

// Initialize pthread on first use
static void _pthread_init_once(void) {
    if (!_pthread_initialized) {
        _pthread_self.self = &_pthread_self;
        _pthread_self.tid = 1;
        _pthread_self.locale = C_LOCALE;
        _pthread_self.canceldisable = 0;
        _pthread_self.cancelasync = 0;
        _pthread_initialized = 1;
    }
}

// Get thread pointer - THIS is what musl's CURRENT_LOCALE macro calls
struct pthread *__pthread_self(void) {
    _pthread_init_once();
    return &_pthread_self;
}

pthread_t pthread_self(void) {
    _pthread_init_once();
    return &_pthread_self;
}

// Stub implementations for OP-TEE single-threaded environment
int pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg)
{
    return -1;  // OP-TEE TAs are single-threaded
}

int pthread_join(pthread_t thread, void **retval) {
    return -1;
}

int pthread_detach(pthread_t thread) {
    return -1;
}
