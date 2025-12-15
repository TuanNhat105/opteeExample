#!/bin/bash
# Build standalone libcxx.a that can be linked with OP-TEE TA
# This creates a self-contained library without depending on OP-TEE build system

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

BUILD_DIR="build_oe_libs"
CROSS_COMPILE="aarch64-none-linux-gnu-"
export PATH=/home/abc/arm-toolchain/bin:$PATH

echo -e "${GREEN}=== Building standalone C++ runtime library ===${NC}"

if [ ! -f "$BUILD_DIR/combined/libcxx_runtime.a" ]; then
    echo -e "${YELLOW}Please run ./build_openenclave_libs.sh first!${NC}"
    exit 1
fi

# Create a standalone library with all symbols
cd "$BUILD_DIR"

echo "Creating standalone libc++_standalone.a with weak symbols..."

# Extract all objects
mkdir -p standalone/obj
cd standalone/obj
ar x ../../combined/libcxx_runtime.a

# Add weak attribute to problematic symbols to avoid conflicts with OP-TEE
cat > fix_symbols.c <<'EOF'
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
EOF

${CROSS_COMPILE}gcc -c fix_symbols.c -o fix_symbols.o

# Combine everything
cd ..
${CROSS_COMPILE}ar rcs libc++_standalone.a obj/*.o
${CROSS_COMPILE}ranlib libc++_standalone.a

echo -e "${GREEN}✓ Created: $PWD/libc++_standalone.a${NC}"
echo ""
echo "Size: $(ls -lh libc++_standalone.a | awk '{print $5}')"
echo "Objects: $(ar t libc++_standalone.a | wc -l)"
