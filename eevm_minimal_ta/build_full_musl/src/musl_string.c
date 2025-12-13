#include <tee_internal_api.h>

typedef __SIZE_TYPE__ size_t;

// Use TEE native functions where possible
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

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = 0;
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}
