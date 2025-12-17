// OP-TEE syscall stubs for libunwind
// Only define what musl doesn't have
#ifndef __OPTEE_LIBUNWIND_SYSCALL_STUBS_H
#define __OPTEE_LIBUNWIND_SYSCALL_STUBS_H

// Don't redefine things that musl already has
// Just provide stubs for functions that are declared but not implemented

// pipe2 stub (musl declares it but we don't have kernel support)
static inline int __libunwind_pipe2(int p[2], int f) { (void)p; (void)f; return -1; }

// syscall stub
static inline long __libunwind_syscall(long n, ...) { (void)n; return -1; }

// mincore stub (memory management not needed in TA)
#include <stddef.h>
static inline int __libunwind_mincore(void* a, size_t l, unsigned char* v) { 
    (void)a; (void)l; (void)v; 
    return -1; 
}

#endif
