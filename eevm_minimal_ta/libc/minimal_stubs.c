// Minimal stubs for missing musl dependencies
// These are simple implementations sufficient for OP-TEE TA environment

#include <errno.h>
#include <wchar.h>

// ===== Threading stubs (musl stdio needs these) =====
// In OP-TEE TA, we're single-threaded, so these are no-ops
int __lockfile(void* f) { 
    return 0;  // Always succeed
}

void __unlockfile(void* f) { 
    // No-op
}

int __lock(volatile int* l) { 
    *l = 1; 
    return 0; 
}

void __unlock(volatile int* l) { 
    *l = 0; 
}

// __ofl_lock and __ofl_unlock for stdio file list
static void* dummy_file_list = 0;

void** __ofl_lock(void) {
    return &dummy_file_list;
}

void __ofl_unlock(void) {
    // No-op
}

// ===== Wide char / multibyte stubs =====
// Simple ASCII-only implementations

// Forward declare FILE type to avoid including stdio.h
typedef struct _IO_FILE FILE;

int mbtowc(wchar_t* pwc, const char* s, size_t n) {
    if (!s) return 0;  // Stateless encoding
    if (!n || !*s) return 0;
    if (pwc) *pwc = (wchar_t)(unsigned char)*s;
    return 1;  // Always 1 byte per char (ASCII only)
}

wint_t fputwc(wchar_t wc, FILE* stream) {
    return -1;  // Not supported in TA
}

wint_t btowc(int c) {
    return (c >= 0 && c <= 127) ? (wchar_t)c : -1;
}

int fwide(FILE* stream, int mode) {
    return 0;  // Always byte-oriented
}

// ===== Error string stub =====
char* strerror(int errnum) {
    static char buf[32];
    
    // Common errno values
    switch (errnum) {
        case 0: return "Success";
        case 1: return "Operation not permitted";
        case 2: return "No such file or directory";
        case 12: return "Out of memory";
        case 22: return "Invalid argument";
        case 34: return "Numerical result out of range";
        default: break;
    }
    
    // For others, return "Error N"
    buf[0] = 'E'; buf[1] = 'r'; buf[2] = 'r'; buf[3] = 'o'; buf[4] = 'r'; buf[5] = ' ';
    int i = 6;
    int n = errnum < 0 ? -errnum : errnum;
    if (errnum < 0) buf[i++] = '-';
    
    // Simple int to string
    int divisor = 1000;
    int started = 0;
    while (divisor > 0) {
        int digit = n / divisor;
        if (digit > 0 || started || divisor == 1) {
            buf[i++] = '0' + digit;
            started = 1;
        }
        n %= divisor;
        divisor /= 10;
    }
    buf[i] = '\0';
    return buf;
}

// ===== errno location implementations =====
// Provide both 2-underscore and 3-underscore versions
// libcxx needs __errno_location (2 underscores)
// OpenEnclave's musl needs ___errno_location (3 underscores)
static int __thread_errno = 0;

int* __errno_location(void) {
    return &__thread_errno;
}

int* ___errno_location(void) {
    return &__thread_errno;
}
