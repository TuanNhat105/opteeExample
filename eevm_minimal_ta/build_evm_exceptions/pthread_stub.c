#include "pthread_stub.h"

// Thread-local storage for single-threaded TA
#define MAX_TLS_KEYS 16
static void* _tls_data[MAX_TLS_KEYS] = {0};
static int _next_key = 0;

__attribute__((weak))
int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    (void)destructor;
    *key = _next_key++;
    return 0;
}

__attribute__((weak))
int pthread_once(pthread_once_t* once, void (*init)(void)) {
    if (*once == 0) {
        init();
        *once = 1;
    }
    return 0;
}

__attribute__((weak))
int pthread_setspecific(pthread_key_t key, const void* value) {
    if (key >= 0 && key < MAX_TLS_KEYS) {
        _tls_data[key] = (void*)value;
        return 0;
    }
    return -1;
}

__attribute__((weak))
void* pthread_getspecific(pthread_key_t key) {
    if (key >= 0 && key < MAX_TLS_KEYS) {
        return _tls_data[key];
    }
    return ((void*)0);
}

__attribute__((weak))
int pthread_mutex_lock(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

__attribute__((weak))
int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

__attribute__((weak))
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    (void)cond; (void)mutex;
    return 0;
}

__attribute__((weak))
int pthread_cond_signal(pthread_cond_t* cond) {
    (void)cond;
    return 0;
}
