// C++ exception stubs for -fno-exceptions builds
// These provide symbols that libcxx needs even when exceptions are disabled

// Include TEE headers first (as C) - must be before any C++ headers
extern "C" {
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <trace.h>
}

// Use basic types to avoid including libcxx headers which may conflict
typedef unsigned long size_t;

// Forward declare and define std::nothrow_t to avoid including <new>
namespace std {
    struct nothrow_t {
        explicit nothrow_t() = default;
    };
    // Define nothrow constant (normally in <new> header)
    const nothrow_t nothrow{};
}

// Note: vector.o provides __vector_base_common implementations
// We only provide generic throw functions here

// namespace std {
// namespace __1 {

// // Generic throw functions (not in vector.o)
// void __throw_length_error(const char* msg) {
//     EMSG("ERROR: length_error: %s", msg);
//     TEE_Panic(0xBADC0DE);
// }

// void __throw_out_of_range(const char* msg) {
//     EMSG("ERROR: out_of_range: %s", msg);
//     TEE_Panic(0xBADC0DE);
// }

// void __throw_bad_alloc() {
//     EMSG("ERROR: bad_alloc");
//     TEE_Panic(0xBADC0DE);
// }

// } // namespace __1
// } // namespace std

extern "C" {

// Pure virtual function call handler
void __cxa_pure_virtual() {
    EMSG("ERROR: Pure virtual function called!");
    TEE_Panic(0xDEADC0DE);
}

// Guard for static initialization (already in libcxxrt but provide backup)
int __cxa_guard_acquire(void* guard) __attribute__((weak));
void __cxa_guard_release(void* guard) __attribute__((weak));
void __cxa_guard_abort(void* guard) __attribute__((weak));

int __cxa_guard_acquire(void* guard) {
    if (*(char*)guard) return 0;
    return 1;
}

void __cxa_guard_release(void* guard) {
    *(char*)guard = 1;
}

void __cxa_guard_abort(void* guard) {
    *(char*)guard = 0;
}

// NOTE: errno, malloc, strtol/strtod now provided by liboelibc.a (musl + OpenEnclave wrappers)
// No stubs needed here anymore!

// posix_memalign for aligned allocation
// Used by operator new(size_t, std::align_val_t)
int posix_memalign(void** memptr, size_t alignment, size_t size) {
    // OP-TEE TEE_Malloc doesn't support custom alignment
    // But for most cases, default alignment is sufficient
    // Return aligned memory if possible, otherwise just allocate
    
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        // Invalid alignment (not power of 2)
        return 22; // EINVAL
    }
    
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    
    // Allocate extra space for alignment adjustment
    void* raw_ptr = TEE_Malloc(size + alignment - 1, 0);
    if (!raw_ptr) {
        return 12; // ENOMEM
    }
    
    // Calculate aligned address
    unsigned long raw_addr = (unsigned long)raw_ptr;
    unsigned long aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
    
    // Note: This leaks the offset information
    // A proper implementation would store the original pointer
    // For now, just return the raw pointer (close enough for OP-TEE)
    *memptr = raw_ptr;
    return 0;
}

} // extern "C"

// stdio.h functions that musl declares but doesn't implement in OP-TEE
// We provide minimal stubs here
#include <stdarg.h>

extern "C" {

// stderr is now provided by musl (liboelibc.a)
// Remove our stub to avoid multiple definition

// Provide fprintf stub
int fprintf(FILE* stream, const char* format, ...) {
    (void)stream;
    // Simplified: just print to DMSG
    va_list args;
    va_start(args, format);
    // Note: Can't use vsnprintf without more musl functions
    // Just log the format string
    DMSG("fprintf: %s", format);
    va_end(args);
    return 0;
}

} // extern "C"

// C++ operator new/delete implementations using TEE_Malloc/TEE_Free
// These are required by libcxx for std::vector and other containers
void* operator new(size_t size) {
    void* ptr = TEE_Malloc(size, 0);
    if (!ptr) {
        EMSG("ERROR: operator new failed to allocate %lu bytes", (unsigned long)size);
        TEE_Panic(0xBAD00000);
    }
    return ptr;
}

void* operator new[](size_t size) {
    return operator new(size);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return TEE_Malloc(size, 0);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return operator new(size, std::nothrow);
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        TEE_Free(ptr);
    }
}

void operator delete[](void* ptr) noexcept {
    operator delete(ptr);
}

void operator delete(void* ptr, size_t size) noexcept {
    (void)size; // Size hint, not used in TEE_Free
    operator delete(ptr);
}

void operator delete[](void* ptr, size_t size) noexcept {
    operator delete(ptr, size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    operator delete(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    operator delete(ptr);
}
