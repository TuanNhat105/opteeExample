typedef __SIZE_TYPE__ size_t;
typedef unsigned wchar_t;
#define NULL ((void*)0)

// dlmalloc functions - ALL WEAK to use OP-TEE versions
extern void* dlmalloc(size_t);
extern void dlfree(void*);
extern void* dlcalloc(size_t, size_t);
extern void* dlrealloc(void*, size_t);

__attribute__((weak))
void* malloc(size_t size) { return dlmalloc(size); }

__attribute__((weak))
void free(void* ptr) { dlfree(ptr); }

__attribute__((weak))
void* calloc(size_t nmemb, size_t size) { return dlcalloc(nmemb, size); }

__attribute__((weak))
void* realloc(void* ptr, size_t size) { return dlrealloc(ptr, size); }

__attribute__((weak))
void abort(void) { while(1); }

// Other stdlib functions with WEAK linkage
__attribute__((weak))
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

__attribute__((weak))
long atol(const char* s) {
    return (long)atoi(s);
}

__attribute__((weak))
long long atoll(const char* s) {
    return (long long)atoi(s);
}

__attribute__((weak))
double atof(const char* s) {
    return 0.0; // Stub
}

// Stub implementations for functions libcxx might need
__attribute__((weak))
int atexit(void (*func)(void)) { return 0; }

__attribute__((weak))
void exit(int status) { while(1); }

__attribute__((weak))
void _Exit(int status) { while(1); }

__attribute__((weak))
char* getenv(const char* name) { return NULL; }

__attribute__((weak))
int system(const char* command) { return -1; }

// Stub qsort/bsearch
__attribute__((weak))
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {}

__attribute__((weak))
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) { return NULL; }

// Stubs for division
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;

__attribute__((weak))
int abs(int n) { return n < 0 ? -n : n; }

__attribute__((weak))
long labs(long n) { return n < 0 ? -n : n; }

__attribute__((weak))
long long llabs(long long n) { return n < 0 ? -n : n; }

__attribute__((weak))
div_t div(int num, int den) { div_t r; r.quot = num / den; r.rem = num % den; return r; }

__attribute__((weak))
ldiv_t ldiv(long num, long den) { ldiv_t r; r.quot = num / den; r.rem = num % den; return r; }

__attribute__((weak))
lldiv_t lldiv(long long num, long long den) { lldiv_t r; r.quot = num / den; r.rem = num % den; return r; }

// Stubs for multibyte/wide char (not really needed)
__attribute__((weak))
int mblen(const char* s, size_t n) { return -1; }

__attribute__((weak))
int mbtowc(wchar_t* pwc, const char* s, size_t n) { return -1; }

__attribute__((weak))
int wctomb(char* s, wchar_t wc) { return -1; }

__attribute__((weak))
size_t mbstowcs(wchar_t* dest, const char* src, size_t n) { return 0; }

__attribute__((weak))
size_t wcstombs(char* dest, const wchar_t* src, size_t n) { return 0; }

// Stub strtol family
__attribute__((weak))
long strtol(const char* s, char** endptr, int base) { return atol(s); }

__attribute__((weak))
unsigned long strtoul(const char* s, char** endptr, int base) { return (unsigned long)atol(s); }

__attribute__((weak))
long long strtoll(const char* s, char** endptr, int base) { return atoll(s); }

__attribute__((weak))
unsigned long long strtoull(const char* s, char** endptr, int base) { return (unsigned long long)atoll(s); }

__attribute__((weak))
double strtod(const char* s, char** endptr) { return atof(s); }

__attribute__((weak))
float strtof(const char* s, char** endptr) { return (float)atof(s); }

__attribute__((weak))
long double strtold(const char* s, char** endptr) { return (long double)atof(s); }

// rand/srand
static unsigned int _rand_next = 1;

__attribute__((weak))
int rand(void) {
    _rand_next = _rand_next * 1103515245 + 12345;
    return (unsigned int)(_rand_next / 65536) % 32768;
}

__attribute__((weak))
void srand(unsigned int seed) {
    _rand_next = seed;
}
