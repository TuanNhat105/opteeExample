/*
 * C++ STL Support for OP-TEE TA (Freestanding Environment)
 * Provides minimal C library functions required by libstdc++
 */

extern "C" {
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <stddef.h>
#include <stdint.h>
}

/*
 * ===================================================================
 * Memory Operations (required by STL containers)
 * ===================================================================
 */

extern "C" void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

extern "C" void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

extern "C" void* memmove(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    
    if (d < s) {
        // Forward copy
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        // Backward copy to handle overlap
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    return dest;
}

extern "C" int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

/*
 * ===================================================================
 * String Operations (required by std::string)
 * ===================================================================
 */

extern "C" size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

extern "C" char* strcpy(char* dest, const char* src) {
    size_t i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
    return dest;
}

extern "C" int strcmp(const char* s1, const char* s2) {
    size_t i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        i++;
    }
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

extern "C" char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

/*
 * ===================================================================
 * Memory Allocation (using TEE allocator)
 * ===================================================================
 */

extern "C" void* malloc(size_t size) {
    void* ptr = TEE_Malloc(size, TEE_MALLOC_FILL_ZERO);
    if (!ptr) {
        EMSG("malloc failed: size=%zu", size);
    }
    return ptr;
}

extern "C" void free(void* ptr) {
    if (ptr) {
        TEE_Free(ptr);
    }
}

extern "C" void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = TEE_Malloc(total, TEE_MALLOC_FILL_ZERO);
    if (!ptr) {
        EMSG("calloc failed: nmemb=%zu, size=%zu", nmemb, size);
    }
    return ptr;
}

extern "C" void* realloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return malloc(new_size);
    }
    
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }
    
    // TEE doesn't have realloc, so we do it manually
    void* new_ptr = TEE_Malloc(new_size, 0);
    if (!new_ptr) {
        EMSG("realloc failed: new_size=%zu", new_size);
        return NULL;
    }
    
    // Note: We don't know the old size, so we copy as much as possible
    // This is a limitation. For production, consider using TEE_MemMove
    // with known sizes or avoid realloc in critical paths.
    TEE_MemMove(new_ptr, ptr, new_size);
    
    TEE_Free(ptr);
    return new_ptr;
}

/*
 * ===================================================================
 * C++ Operators new/delete (required by STL)
 * ===================================================================
 */

void* operator new(size_t size) {
    void* ptr = TEE_Malloc(size, TEE_MALLOC_FILL_ZERO);
    if (!ptr) {
        EMSG("operator new failed: size=%zu", size);
        // In freestanding, we can't throw std::bad_alloc
        // Return null and hope the caller checks
    }
    return ptr;
}

void* operator new[](size_t size) {
    void* ptr = TEE_Malloc(size, TEE_MALLOC_FILL_ZERO);
    if (!ptr) {
        EMSG("operator new[] failed: size=%zu", size);
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        TEE_Free(ptr);
    }
}

void operator delete[](void* ptr) noexcept {
    if (ptr) {
        TEE_Free(ptr);
    }
}

// C++14 sized delete
void operator delete(void* ptr, size_t) noexcept {
    if (ptr) {
        TEE_Free(ptr);
    }
}

void operator delete[](void* ptr, size_t) noexcept {
    if (ptr) {
        TEE_Free(ptr);
    }
}

/*
 * ===================================================================
 * C++ Runtime Support (required by STL)
 * ===================================================================
 */

extern "C" void __cxa_pure_virtual() {
    EMSG("Pure virtual function called!");
    TEE_Panic(0);
}

extern "C" int __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle) {
    // In TA, we don't support atexit handlers
    // Just ignore and return success
    (void)func;
    (void)arg;
    (void)dso_handle;
    return 0;
}

extern "C" void __cxa_finalize(void* d) {
    // No-op for TA
    (void)d;
}

/*
 * ===================================================================
 * Exception Support Stubs (we disable exceptions with -fno-exceptions)
 * ===================================================================
 */

extern "C" void __cxa_call_unexpected(void*) {
    EMSG("Unexpected exception!");
    TEE_Panic(0);
}

/*
 * ===================================================================
 * Math Functions (if needed by some STL operations)
 * ===================================================================
 */

extern "C" int abs(int n) {
    return (n < 0) ? -n : n;
}

extern "C" long labs(long n) {
    return (n < 0) ? -n : n;
}

/*
 * ===================================================================
 * Character Classification (for std::transform, etc.)
 * ===================================================================
 */

extern "C" int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || 
            c == '\f' || c == '\v');
}

extern "C" int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

extern "C" int isalpha(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

extern "C" int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

extern "C" int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

extern "C" int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

/*
 * ===================================================================
 * Guard Variables (for static local variables)
 * ===================================================================
 */

extern "C" int __cxa_guard_acquire(uint64_t* guard) {
    // Simple implementation: use atomic check
    // In single-threaded TA, this is safe
    if (*guard == 0) {
        *guard = 1;
        return 1;
    }
    return 0;
}

extern "C" void __cxa_guard_release(uint64_t* guard) {
    *guard = 1;
}

extern "C" void __cxa_guard_abort(uint64_t* guard) {
    *guard = 0;
}

/*
 * ===================================================================
 * Stack Protector (if enabled)
 * ===================================================================
 */

#ifdef __STACK_PROTECTOR__
extern "C" uintptr_t __stack_chk_guard = 0xDEADBEEF;

extern "C" void __stack_chk_fail(void) {
    EMSG("Stack smashing detected!");
    TEE_Panic(0);
}
#endif
