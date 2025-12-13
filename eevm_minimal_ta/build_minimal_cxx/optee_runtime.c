#include <tee_internal_api.h>
#include <stddef.h>

void* malloc(size_t size) {
    return TEE_Malloc(size, 0);
}

void free(void* ptr) {
    TEE_Free(ptr);
}

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = TEE_Malloc(total, 0);
    if (ptr) TEE_MemFill(ptr, 0, total);
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    return TEE_Realloc(ptr, size);
}

void* memcpy(void* dest, const void* src, size_t n) {
    TEE_MemMove(dest, src, n);
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    TEE_MemMove(dest, src, n);
    return dest;
}

void* memset(void* s, int c, size_t n) {
    TEE_MemFill(s, c, n);
    return s;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    return TEE_MemCompare(s1, s2, n);
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

void abort(void) {
    TEE_Panic(0xDEADBEEF);
}

void __assert_fail(const char* expr, const char* file, unsigned line, const char* func) {
    TEE_Panic(0xA5531ED);
}
