typedef __SIZE_TYPE__ size_t;
typedef unsigned wchar_t;
#define NULL ((void*)0)

// dlmalloc functions
extern void* dlmalloc(size_t);
extern void dlfree(void*);
extern void* dlcalloc(size_t, size_t);
extern void* dlrealloc(void*, size_t);

void* malloc(size_t size) { return dlmalloc(size); }
void free(void* ptr) { dlfree(ptr); }
void* calloc(size_t nmemb, size_t size) { return dlcalloc(nmemb, size); }
void* realloc(void* ptr, size_t size) { return dlrealloc(ptr, size); }

void abort(void) { while(1); }
void _Exit(int status) { while(1); }
void exit(int status) { while(1); }

int atoi(const char* s) {
    int n = 0, neg = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '+') s++;
    else if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return neg ? -n : n;
}

long atol(const char* s) {
    return (long)atoi(s);
}

long long atoll(const char* s) {
    return (long long)atoi(s);
}

// Stub implementations for functions libcxx might need
int atexit(void (*func)(void)) { return 0; }
char* getenv(const char* name) { return NULL; }
int system(const char* command) { return -1; }

// Stub qsort/bsearch
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {}
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) { return NULL; }

// Stubs for division
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;

int abs(int n) { return n < 0 ? -n : n; }
long labs(long n) { return n < 0 ? -n : n; }
long long llabs(long long n) { return n < 0 ? -n : n; }

div_t div(int num, int den) { div_t r; r.quot = num / den; r.rem = num % den; return r; }
ldiv_t ldiv(long num, long den) { ldiv_t r; r.quot = num / den; r.rem = num % den; return r; }
lldiv_t lldiv(long long num, long long den) { lldiv_t r; r.quot = num / den; r.rem = num % den; return r; }

// Stubs for multibyte/wide char (not really needed)
int mblen(const char* s, size_t n) { return -1; }
int mbtowc(wchar_t* pwc, const char* s, size_t n) { return -1; }
int wctomb(char* s, wchar_t wc) { return -1; }
size_t mbstowcs(wchar_t* dest, const char* src, size_t n) { return 0; }
size_t wcstombs(char* dest, const wchar_t* src, size_t n) { return 0; }

// Stub strtol family
long strtol(const char* s, char** endptr, int base) { return atol(s); }
unsigned long strtoul(const char* s, char** endptr, int base) { return (unsigned long)atol(s); }
long long strtoll(const char* s, char** endptr, int base) { return atoll(s); }
unsigned long long strtoull(const char* s, char** endptr, int base) { return (unsigned long long)atoll(s); }
