// SPDX-License-Identifier: BSD-2-Clause
/*
 * Stub implementations for missing symbols in OP-TEE environment
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <tee_api.h>

// C++ ABI functions
extern "C" {

// __cxa_atexit: Register function to be called at exit
// In OP-TEE TA, we don't have proper exit, so just ignore
int __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle) {
    (void)func;
    (void)arg;
    (void)dso_handle;
    return 0;
}

// __dso_handle: Dynamic Shared Object handle
// In static TA, we don't need this
void* __dso_handle = nullptr;

// sched_yield: Yield CPU (not applicable in OP-TEE)
int sched_yield(void) {
    // In OP-TEE, we can't really yield to other threads
    // Just return success
    return 0;
}

// posix_memalign: Aligned memory allocation
int posix_memalign(void** memptr, size_t alignment, size_t size) {
    if (!memptr || (alignment % sizeof(void*)) != 0) {
        return 22; // EINVAL
    }
    
    // OP-TEE's malloc already provides good alignment
    // For simplicity, just use regular malloc
    void* ptr = malloc(size);
    if (!ptr) {
        return 12; // ENOMEM
    }
    
    *memptr = ptr;
    return 0;
}

// pthread_once: One-time initialization
// Simple implementation without real thread support
typedef int pthread_once_t;
#define PTHREAD_ONCE_INIT 0

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) {
        return 22; // EINVAL
    }
    
    // Check if already initialized
    if (*once_control == 0) {
        init_routine();
        *once_control = 1;
    }
    
    return 0;
}

// __string_read: Used by vsscanf (musl internal)
size_t __string_read(void* f, unsigned char* buf, size_t len) {
    typedef struct {
        unsigned char* s;
        size_t len;
    } string_context_t;
    
    string_context_t* ctx = (string_context_t*)f;
    
    if (len > ctx->len) {
        len = ctx->len;
    }
    
    memcpy(buf, ctx->s, len);
    ctx->s += len;
    ctx->len -= len;
    
    return len;
}

// Memory mapping functions (not supported in OP-TEE)
void* __mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    (void)addr; (void)length; (void)prot; (void)flags; (void)fd; (void)offset;
    // mmap not supported in OP-TEE
    return (void*)-1;
}

int __munmap(void* addr, size_t length) {
    (void)addr; (void)length;
    // munmap not supported in OP-TEE
    return -1;
}

// OpenEnclave syscall stubs (liboelibc expects these)
long oe_SYS_openat_impl(int dirfd, const char* pathname, int flags, unsigned mode) {
    (void)dirfd; (void)pathname; (void)flags; (void)mode;
    return -1; // Not supported
}

long oe_SYS_fstat_impl(int fd, void* statbuf) {
    (void)fd; (void)statbuf;
    return -1; // Not supported
}

// Additional stubs that might be needed

// __assert_fail: Assertion failure (already in OP-TEE, but may need our version)
void __assert_fail(const char* assertion, const char* file, unsigned line, const char* function) {
    (void)assertion;
    (void)file;
    (void)line;
    (void)function;
    // Just abort - in OP-TEE this will cause TA to terminate
    abort();
}

// gettext: Localization (not needed, just return input)
const char* gettext(const char* msgid) {
    return msgid;
}

} // extern "C"
