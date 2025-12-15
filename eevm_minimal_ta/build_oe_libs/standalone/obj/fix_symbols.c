// Weak implementations of missing libc functions
__attribute__((weak))
char* secure_getenv(const char* name) {
    return 0; // Always return NULL in OP-TEE
}

__attribute__((weak))
unsigned long __isoc23_strtoul(const char* str, char** endptr, int base) {
    // Simple strtoul implementation
    unsigned long result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * base + (*str - '0');
        str++;
    }
    if (endptr) *endptr = (char*)str;
    return result;
}

// Pthread stubs (not used with -fno-threadsafe-statics)
__attribute__((weak))
int pthread_once(void* once, void (*init)(void)) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        init();
    }
    return 0;
}

__attribute__((weak))
int pthread_cond_wait(void* cond, void* mutex) { return 0; }

__attribute__((weak))
int pthread_cond_broadcast(void* cond) { return 0; }

// DL stub
__attribute__((weak))
void* _dl_find_object(void* addr, void* result) { return 0; }
