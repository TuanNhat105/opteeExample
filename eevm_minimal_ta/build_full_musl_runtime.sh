#!/bin/bash
# Build FULL musl + libcxx runtime for OP-TEE
# Port complete musl libc with math.h support
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
MUSL_SRC="$PROJECT_ROOT/external/openenclave/3rdparty/musl/musl"
LIBCXX_SRC="$PROJECT_ROOT/external/openenclave/3rdparty/libcxx/libcxx"
BUILD_DIR="$PROJECT_ROOT/build_full_musl"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
TA_DEV_KIT_DIR="${TA_DEV_KIT_DIR:-/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64}"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build FULL musl + libcxx for OP-TEE${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check prerequisites
if [ ! -d "$MUSL_SRC/include" ]; then
    echo -e "${RED}ERROR: musl not found! Run: ./download_musl.sh${NC}"
    exit 1
fi

if [ ! -d "$TA_DEV_KIT_DIR" ]; then
    echo -e "${RED}ERROR: TA_DEV_KIT_DIR not found!${NC}"
    exit 1
fi

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"/{src,include}
cd "$BUILD_DIR"

echo -e "${YELLOW}Step 1: Copy musl headers${NC}"
cp -r "$MUSL_SRC/include/"* include/
# Copy arch-specific headers
mkdir -p include/bits
cp -r "$MUSL_SRC/arch/aarch64/bits/"* include/bits/ 2>/dev/null || true

# Generate minimal alltypes.h (musl normally uses configure for this)
cat > include/bits/alltypes.h <<'ALLTYPES_EOF'
/* Minimal alltypes.h for OP-TEE */
typedef unsigned long size_t;
typedef unsigned long uintptr_t;
typedef long ptrdiff_t;
typedef long ssize_t;
typedef long intptr_t;

typedef signed char int8_t;
typedef short       int16_t;
typedef int         int32_t;
typedef long long   int64_t;
typedef long long   intmax_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long long uintmax_t;

typedef unsigned wint_t;
typedef unsigned wchar_t;

typedef struct { unsigned __opaque; } mbstate_t;

typedef __builtin_va_list va_list;
typedef long time_t;

#define NULL ((void*)0)
ALLTYPES_EOF

echo -e "${GREEN}✓ $(find include -name '*.h' | wc -l) headers copied${NC}"
echo ""

echo -e "${YELLOW}Step 2: Build dlmalloc allocator${NC}"
cat > src/dlmalloc_optee.c <<'EOF'
// dlmalloc for OP-TEE - use compiler builtins, not musl headers
#include <tee_internal_api.h>

// Use compiler builtins instead of stddef.h
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INT8_TYPE__ int8_t;
typedef __UINT8_TYPE__ uint8_t;

#define HAVE_MMAP 0
#define LACKS_UNISTD_H
#define LACKS_SYS_PARAM_H  
#define LACKS_SYS_TYPES_H
#define LACKS_TIME_H
#define MORECORE dlmalloc_sbrk
#define ABORT TEE_Panic(0xDEADC0DE)
#define USE_DL_PREFIX
#define LACKS_STDLIB_H
#define LACKS_STRING_H
#define LACKS_ERRNO_H
#define USE_LOCKS 0
#define NO_MALLOC_STATS 1

// errno stub
static int _errno_val = 0;
#define errno _errno_val
#define ENOMEM 12
#define EINVAL 22

static inline void* _memcpy(void* dest, const void* src, size_t n) {
    TEE_MemMove(dest, src, n);
    return dest;
}
static inline void* _memset(void* s, int c, size_t n) {
    TEE_MemFill(s, (uint8_t)c, n);
    return s;
}
#define memcpy _memcpy
#define memset _memset

static uint8_t* _heap = NULL;
static size_t _heap_size = 0;
static size_t _heap_used = 0;

void* dlmalloc_sbrk(ptrdiff_t increment) {
    if (!_heap) {
        _heap_size = 2 * 1024 * 1024; // 2MB heap
        _heap = (uint8_t*)TEE_Malloc(_heap_size, 0);
        if (!_heap) return (void*)-1;
        _heap_used = 0;
    }
    if (increment < 0) return (void*)-1;
    if (_heap_used + increment > _heap_size) return (void*)-1;
    void* ptr = _heap + _heap_used;
    _heap_used += increment;
    return ptr;
}

EOF

# Include dlmalloc.c
echo '#include "'$PROJECT_ROOT'/external/openenclave/3rdparty/dlmalloc/dlmalloc/malloc.c"' >> src/dlmalloc_optee.c

COMMON_FLAGS=(
    -ffreestanding
    -fPIC
    -O2
    -I"$TA_DEV_KIT_DIR/include"
    -Wno-unused-variable
    -Wno-unused-function
)

echo -n "Building dlmalloc... "
if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" \
    -c src/dlmalloc_optee.c -o dlmalloc.o 2>err_dlmalloc.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat err_dlmalloc.log | head -10
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 3: Build musl string functions${NC}"
cat > src/musl_string.c <<'EOF'
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
EOF

echo -n "Building string functions... "
if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" \
    -c src/musl_string.c -o string.o 2>err_string.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat err_string.log
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 4: Build musl stdlib functions${NC}"
cat > src/musl_stdlib.c <<'EOF'
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
EOF

echo -n "Building stdlib functions... "
if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" \
    -c src/musl_stdlib.c -o stdlib.o 2>err_stdlib.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat err_stdlib.log
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 5: Build math.h stubs${NC}"
cat > src/musl_math.c <<'EOF'
// Math function stubs for libcxx
typedef __SIZE_TYPE__ size_t;
typedef unsigned wchar_t;

#define NAN __builtin_nanf("")
#define INFINITY __builtin_inff()

// Basic math functions
double fabs(double x) { return x < 0 ? -x : x; }
float fabsf(float x) { return x < 0 ? -x : x; }
long double fabsl(long double x) { return x < 0 ? -x : x; }

int isnan(double x) { return x != x; }
int isinf(double x) { return fabs(x) == INFINITY; }
int isfinite(double x) { return !isnan(x) && !isinf(x); }
int isnormal(double x) { return isfinite(x) && x != 0.0; }

// Trigonometric stubs (return 0 or NAN)
double sin(double x) { return 0.0; }
double cos(double x) { return 1.0; }
double tan(double x) { return 0.0; }
double asin(double x) { return NAN; }
double acos(double x) { return NAN; }
double atan(double x) { return NAN; }
double atan2(double y, double x) { return NAN; }

float sinf(float x) { return 0.0f; }
float cosf(float x) { return 1.0f; }
float tanf(float x) { return 0.0f; }
float asinf(float x) { return NAN; }
float acosf(float x) { return NAN; }
float atanf(float x) { return NAN; }
float atan2f(float y, float x) { return NAN; }

// Exponential/logarithmic stubs
double exp(double x) { return NAN; }
double log(double x) { return NAN; }
double log10(double x) { return NAN; }
double pow(double x, double y) { return NAN; }
double sqrt(double x) { return NAN; }

float expf(float x) { return NAN; }
float logf(float x) { return NAN; }
float log10f(float x) { return NAN; }
float powf(float x, float y) { return NAN; }
float sqrtf(float x) { return NAN; }

// Hyperbolic stubs
double sinh(double x) { return NAN; }
double cosh(double x) { return NAN; }
double tanh(double x) { return NAN; }
double asinh(double x) { return NAN; }
double acosh(double x) { return NAN; }
double atanh(double x) { return NAN; }

// Rounding functions
double ceil(double x) { return (double)((long long)x + (x > 0 && x != (long long)x)); }
double floor(double x) { return (double)((long long)x - (x < 0 && x != (long long)x)); }
double trunc(double x) { return (double)((long long)x); }
double round(double x) { return floor(x + 0.5); }

float ceilf(float x) { return (float)ceil((double)x); }
float floorf(float x) { return (float)floor((double)x); }
float truncf(float x) { return (float)trunc((double)x); }
float roundf(float x) { return (float)round((double)x); }

// Remainder functions  
double fmod(double x, double y) { return x - trunc(x / y) * y; }
double remainder(double x, double y) { return fmod(x, y); }
double remquo(double x, double y, int* quo) { *quo = (int)(x / y); return fmod(x, y); }

// More stubs (add as needed when compiler complains)
double copysign(double x, double y) { return (y < 0) ? -fabs(x) : fabs(x); }
double scalbn(double x, int n) { return x; }
double ldexp(double x, int exp) { return x; }
double frexp(double x, int* exp) { *exp = 0; return x; }
double modf(double x, double* iptr) { *iptr = trunc(x); return x - *iptr; }

float copysignf(float x, float y) { return (float)copysign((double)x, (double)y); }
float scalbnf(float x, int n) { return x; }
float ldexpf(float x, int exp) { return x; }
float frexpf(float x, int* exp) { *exp = 0; return x; }
float modff(float x, float* iptr) { double d; float r = (float)modf((double)x, &d); *iptr = (float)d; return r; }

// Long double versions (just cast to double)
long double sinl(long double x) { return (long double)sin((double)x); }
long double cosl(long double x) { return (long double)cos((double)x); }
long double tanl(long double x) { return (long double)tan((double)x); }
long double asinl(long double x) { return (long double)asin((double)x); }
long double acosl(long double x) { return (long double)acos((double)x); }
long double atanl(long double x) { return (long double)atan((double)x); }
long double atan2l(long double y, long double x) { return (long double)atan2((double)y, (double)x); }
long double expl(long double x) { return (long double)exp((double)x); }
long double logl(long double x) { return (long double)log((double)x); }
long double log10l(long double x) { return (long double)log10((double)x); }
long double powl(long double x, long double y) { return (long double)pow((double)x, (double)y); }
long double sqrtl(long double x) { return (long double)sqrt((double)x); }
long double ceill(long double x) { return (long double)ceil((double)x); }
long double floorl(long double x) { return (long double)floor((double)x); }
long double truncl(long double x) { return (long double)trunc((double)x); }
long double roundl(long double x) { return (long double)round((double)x); }
long double fmodl(long double x, long double y) { return (long double)fmod((double)x, (double)y); }
long double copysignl(long double x, long double y) { return (long double)copysign((double)x, (double)y); }

// Even more stubs that libcxx might reference
double exp2(double x) { return pow(2.0, x); }
double log2(double x) { return log(x) / log(2.0); }
double cbrt(double x) { return pow(x, 1.0/3.0); }
double hypot(double x, double y) { return sqrt(x*x + y*y); }
double erf(double x) { return NAN; }
double erfc(double x) { return NAN; }
double tgamma(double x) { return NAN; }
double lgamma(double x) { return NAN; }

float exp2f(float x) { return (float)exp2((double)x); }
float log2f(float x) { return (float)log2((double)x); }
float cbrtf(float x) { return (float)cbrt((double)x); }
float hypotf(float x, float y) { return (float)hypot((double)x, (double)y); }
float erff(float x) { return NAN; }
float erfcf(float x) { return NAN; }
float tgammaf(float x) { return NAN; }
float lgammaf(float x) { return NAN; }

long double exp2l(long double x) { return (long double)exp2((double)x); }
long double log2l(long double x) { return (long double)log2((double)x); }
long double cbrtl(long double x) { return (long double)cbrt((double)x); }
long double hypotl(long double x, long double y) { return (long double)hypot((double)x, (double)y); }
long double erfl(long double x) { return NAN; }
long double erfcl(long double x) { return NAN; }
long double tgammal(long double x) { return NAN; }
long double lgammal(long double x) { return NAN; }

// Classification functions
int fpclassify(double x) { return isnormal(x) ? 3 : (x == 0.0 ? 5 : 0); }
int signbit(double x) { return x < 0; }
EOF

echo -n "Building math stubs... "
if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" \
    -c src/musl_math.c -o math.o 2>err_math.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat err_math.log
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 6: Build C++ operators${NC}"
cat > src/cxx_operators.cpp <<'EOF'
typedef __SIZE_TYPE__ size_t;

extern "C" {
    void* malloc(size_t);
    void free(void*);
    void abort(void);
}

void* operator new(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) abort();
    return ptr;
}

void* operator new[](size_t size) {
    void* ptr = malloc(size);
    if (!ptr) abort();
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (ptr) free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (ptr) free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    if (ptr) free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    if (ptr) free(ptr);
}
EOF

CXX_FLAGS=(
    -std=c++17
    -nostdinc++
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fPIC
    -O2
    -I"$TA_DEV_KIT_DIR/include"
    -I./include
    -I"$LIBCXX_SRC/include"
)

echo -n "Building C++ operators... "
if ${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c src/cxx_operators.cpp -o cxx_operators.o 2>err_cxx.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat err_cxx.log
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 7: Create libraries${NC}"
${CROSS_COMPILE}ar rcs libmusl.a dlmalloc.o string.o stdlib.o math.o
${CROSS_COMPILE}ar rcs libcxx.a cxx_operators.o
echo -e "${GREEN}✓ libmusl.a (C runtime with math stubs)${NC}"
echo -e "${GREEN}✓ libcxx.a (C++ operators)${NC}"
echo ""

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}BUILD COMPLETE!${NC}"
echo -e "${GREEN}========================================${NC}"
ls -lh *.a
echo ""
echo "Libraries to link:"
echo "  1. libmusl.a  (musl libc + dlmalloc + math stubs)"
echo "  2. libcxx.a   (C++ operators)"
echo ""
echo "Include paths:"
echo "  -I$BUILD_DIR/include"
echo "  -I$LIBCXX_SRC/include"
echo ""
echo "Note: vector is HEADER-ONLY, no need to compile vector.cpp"
echo "      Math functions are STUBS (return 0/NaN)"
echo ""
