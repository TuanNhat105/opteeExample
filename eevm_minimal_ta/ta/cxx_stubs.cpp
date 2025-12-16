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

namespace std {
namespace __1 {

// Forward declarations
template<bool> struct __vector_base_common;

template<>
struct __vector_base_common<true> {
    void __throw_length_error() const;
    void __throw_out_of_range() const;
};

// Implementation
void __vector_base_common<true>::__throw_length_error() const {
    EMSG("ERROR: std::vector length_error!");
    TEE_Panic(0xBADC0DE);
}

void __vector_base_common<true>::__throw_out_of_range() const {
    EMSG("ERROR: std::vector out_of_range!");
    TEE_Panic(0xBADC0DE);
}

// Generic throw functions
void __throw_length_error(const char* msg) {
    EMSG("ERROR: length_error: %s", msg);
    TEE_Panic(0xBADC0DE);
}

void __throw_out_of_range(const char* msg) {
    EMSG("ERROR: out_of_range: %s", msg);
    TEE_Panic(0xBADC0DE);
}

void __throw_bad_alloc() {
    EMSG("ERROR: bad_alloc");
    TEE_Panic(0xBADC0DE);
}

} // namespace __1
} // namespace std

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
