#include <stddef.h>
#include <tee_internal_api.h>

void* memcpy(void* dest, const void* src, size_t n) {
    TEE_MemMove(dest, src, n);
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    TEE_MemMove(dest, src, n);
    return dest;
}

void* memset(void* s, int c, size_t n) {
    TEE_MemFill(s, (uint8_t)c, n);
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
