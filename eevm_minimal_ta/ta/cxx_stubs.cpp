// C++ exception stubs for -fno-exceptions builds
// These provide symbols that libcxx needs even when exceptions are disabled

#include <tee_internal_api.h>

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
