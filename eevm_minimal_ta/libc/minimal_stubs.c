// Minimal stubs for missing musl dependencies
// Complete version with libunwind, libgcc, and OpenEnclave stubs

#include <errno.h>
#include <wchar.h>
#include <stdlib.h>
#include <pthread.h>

// ===== Threading stubs (musl stdio needs these) =====
int __lockfile(void* f) { 
    return 0;
}

void __unlockfile(void* f) { 
}

int __lock(volatile int* l) { 
    *l = 1; 
    return 0; 
}

void __unlock(volatile int* l) { 
    *l = 0; 
}

static void* dummy_file_list = 0;

void** __ofl_lock(void) {
    return &dummy_file_list;
}

void __ofl_unlock(void) {
}

// ===== Wide char / multibyte stubs =====
typedef struct _IO_FILE FILE;

int mbtowc(wchar_t* pwc, const char* s, size_t n) {
    if (!s) return 0;
    if (!n || !*s) return 0;
    if (pwc) *pwc = (wchar_t)(unsigned char)*s;
    return 1;
}

wint_t fputwc(wchar_t wc, FILE* stream) {
    return -1;
}

wint_t btowc(int c) {
    return (c >= 0 && c <= 127) ? (wchar_t)c : -1;
}

int fwide(FILE* stream, int mode) {
    return 0;
}

// ===== Error string stub =====
char* strerror(int errnum) {
    static char buf[32];
    switch (errnum) {
        case 0: return "Success";
        case 1: return "Operation not permitted";
        case 2: return "No such file or directory";
        case 12: return "Out of memory";
        case 22: return "Invalid argument";
        case 34: return "Numerical result out of range";
        default: break;
    }
    buf[0] = 'E'; buf[1] = 'r'; buf[2] = 'r'; buf[3] = 'o'; buf[4] = 'r'; buf[5] = ' ';
    int i = 6;
    int n = errnum < 0 ? -errnum : errnum;
    if (errnum < 0) buf[i++] = '-';
    int divisor = 1000;
    int started = 0;
    while (divisor > 0) {
        int digit = n / divisor;
        if (digit > 0 || started || divisor == 1) {
            buf[i++] = '0' + digit;
            started = 1;
        }
        n %= divisor;
        divisor /= 10;
    }
    buf[i] = '\0';
    return buf;
}

// ===== errno location =====
static int __thread_errno = 0;

int* __errno_location(void) {
    return &__thread_errno;
}

int* ___errno_location(void) {
    return &__thread_errno;
}

// ===== pthread stubs for single-threaded OP-TEE =====
// Musl's pthread needs futex syscalls - OP-TEE doesn't have kernel
typedef struct {
    int dummy;
} pthread_mutex_t_stub;

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    return 0;  // Single-threaded - no actual locking needed
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    return 0;  // Single-threaded - no actual unlocking needed
}

int pthread_cond_signal(pthread_cond_t* cond) {
    return 0;  // Single-threaded - no signaling needed
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    return 0;  // Single-threaded - no broadcast needed
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    return 0;  // Single-threaded - no waiting needed
}

// ===== OpenEnclave allocator stubs =====
int oe_allocator_posix_memalign(void** memptr, unsigned long alignment, unsigned long size) {
    void* ptr = malloc(size);
    if (!ptr) return 12;
    *memptr = ptr;
    return 0;
}

void oe_allocator_free(void* ptr) {
    free(ptr);
}

// ===== libunwind signal frame stub =====
int _ULaarch64_is_signal_frame(void* cursor) {
    return 0;
}

// ===== libgcc auxval stub =====
unsigned long __getauxval(unsigned long type) {
    return 0;
}

// ===== OpenEnclave syscall stubs =====
long oe_SYS_close_impl(int fd) {
    return -1;
}

long oe_SYS_lseek_impl(int fd, long offset, int whence) {
    return -1;
}

long oe_SYS_writev_impl(int fd, const void* iov, int iovcnt) {
    return -1;
}

long oe_SYS_futex_impl(int* uaddr, int futex_op, int val, void* timeout, int* uaddr2, int val3) {
    // OP-TEE is single-threaded, futex always succeeds immediately
    return 0;
}

long __syscall_ret(unsigned long r) {
    if (r > -4096UL) {
        __thread_errno = -(long)r;
        return -1;
    }
    return r;
}
