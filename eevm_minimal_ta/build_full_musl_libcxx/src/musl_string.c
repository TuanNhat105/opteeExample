// String functions with WEAK linkage - only implement what OP-TEE doesn't have
#include <tee_internal_api.h>

typedef __SIZE_TYPE__ size_t;

// Use TEE native functions where possible - ALL WEAK to avoid conflicts
__attribute__((weak))
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

__attribute__((weak))
void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

__attribute__((weak))
void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

__attribute__((weak))
int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

__attribute__((weak))
size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

// WEAK symbols - use OP-TEE versions if available
__attribute__((weak))
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

__attribute__((weak))
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = 0;
    return dest;
}

__attribute__((weak))
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

__attribute__((weak))
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

__attribute__((weak))
char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

__attribute__((weak))
char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (*d) d++;
    while (n-- && (*d++ = *src++));
    *d = 0;
    return dest;
}

__attribute__((weak))
char* strchr(const char* s, int c) {
    while (*s && *s != (char)c) s++;
    return (*s == (char)c) ? (char*)s : (char*)0;
}

__attribute__((weak))
char* strrchr(const char* s, int c) {
    const char* last = (const char*)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (char*)last;
}

__attribute__((weak))
size_t strcspn(const char* s, const char* reject) {
    size_t count = 0;
    while (*s) {
        const char* r = reject;
        while (*r && *r != *s) r++;
        if (*r) break;
        s++;
        count++;
    }
    return count;
}

__attribute__((weak))
size_t strspn(const char* s, const char* accept) {
    size_t count = 0;
    while (*s) {
        const char* a = accept;
        while (*a && *a != *s) a++;
        if (!*a) break;
        s++;
        count++;
    }
    return count;
}

__attribute__((weak))
char* strpbrk(const char* s, const char* accept) {
    while (*s) {
        const char* a = accept;
        while (*a && *a != *s) a++;
        if (*a) return (char*)s;
        s++;
    }
    return (char*)0;
}

__attribute__((weak))
char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char*)haystack;
        haystack++;
    }
    return (char*)0;
}

__attribute__((weak))
char* strtok(char* s, const char* delim) {
    static char* last = (char*)0;
    if (s) last = s;
    if (!last) return (char*)0;
    
    while (*last) {
        const char* d = delim;
        while (*d && *d != *last) d++;
        if (!*d) break;
        last++;
    }
    
    if (!*last) return (char*)0;
    
    char* token = last;
    while (*last) {
        const char* d = delim;
        while (*d && *d != *last) d++;
        if (*d) {
            *last++ = 0;
            return token;
        }
        last++;
    }
    
    return token;
}

__attribute__((weak))
int strcoll(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}

__attribute__((weak))
size_t strxfrm(char* dest, const char* src, size_t n) {
    size_t len = 0;
    while (src[len]) len++;
    if (len < n) {
        size_t i;
        for (i = 0; i <= len; i++) dest[i] = src[i];
    }
    return len;
}

__attribute__((weak))
char* strerror(int errnum) {
    return (char*)"Unknown error";
}
