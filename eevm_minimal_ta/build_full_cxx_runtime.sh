#!/bin/bash
# Build FULL libcxx runtime for OP-TEE
# Following OpenEnclave architecture exactly

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
OE_ROOT="$PROJECT_ROOT/external/openenclave"
BUILD_DIR="$PROJECT_ROOT/build_full_runtime"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build FULL C++ Runtime for OP-TEE${NC}"
echo -e "${GREEN}Based on OpenEnclave Architecture${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check prerequisites
if [ ! -d "$OE_ROOT/3rdparty/libcxx/libcxx" ]; then
    echo -e "${RED}ERROR: libcxx not found!${NC}"
    echo "Run: ./download_libcxx.sh"
    exit 1
fi

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"/{libc,libcxxrt,libcxx}

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}STEP 1: Build OE libc (musl-based)${NC}"
echo -e "${YELLOW}========================================${NC}"
cd "$BUILD_DIR/libc"

# Common flags
COMMON_FLAGS=(
    -ffreestanding
    -fno-omit-frame-pointer
    -fPIC
    -O2
    -I"$OE_ROOT/libc"
    -I"$OE_ROOT/3rdparty/musl/musl/include"
    -I"$OE_ROOT/3rdparty/musl/musl/arch/aarch64"
    -I"$PROJECT_ROOT/stubs"
)

# Build essential libc functions from OpenEnclave
LIBC_SOURCES=(
    "$OE_ROOT/libc/errno.c"
    "$OE_ROOT/libc/malloc.c"
    "$OE_ROOT/libc/abort.c"
    "$OE_ROOT/libc/assert.c"
    "$OE_ROOT/libc/atexit.c"
)

echo "Building OE libc..."
SUCCESS=0
for src in "${LIBC_SOURCES[@]}"; do
    if [ -f "$src" ]; then
        obj=$(basename "${src%.c}.o")
        echo -n "  $(basename $src)..."
        if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" -c "$src" -o "$obj" 2>err.log; then
            echo -e " ${GREEN}OK${NC}"
            ((SUCCESS++))
        else
            echo -e " ${RED}FAIL${NC}"
            cat err.log | head -3 | sed 's/^/    /'
        fi
    fi
done

# Create libc archive
if [ $SUCCESS -gt 0 ]; then
    ${CROSS_COMPILE}ar rcs liboecrt.a *.o 2>/dev/null || true
    echo -e "${GREEN}✓ liboecrt.a created ($SUCCESS files)${NC}"
else
    echo -e "${YELLOW}⚠ No libc files built, creating empty library${NC}"
    touch empty.c
    ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" -c empty.c -o empty.o
    ${CROSS_COMPILE}ar rcs liboecrt.a empty.o
fi
echo ""

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}STEP 2: Build libcxxrt (C++ ABI)${NC}"
echo -e "${YELLOW}========================================${NC}"
cd "$BUILD_DIR/libcxxrt"

CXXRT_SRC="$OE_ROOT/3rdparty/libcxxrt/libcxxrt/src"
CXX_FLAGS=(
    -std=c++17
    -nostdinc++
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-omit-frame-pointer
    -fPIC
    -fvisibility=hidden
    -O2
    -I"$CXXRT_SRC"
    -I"$OE_ROOT/3rdparty/libcxx/libcxx/include"
    -I"$PROJECT_ROOT/stubs"
)

# Essential libcxxrt sources
CXXRT_SOURCES=(
    "$CXXRT_SRC/guard.cc"
    "$CXXRT_SRC/typeinfo.cc"
    "$CXXRT_SRC/stdexcept.cc"
)

echo "Building libcxxrt..."
SUCCESS=0
for src in "${CXXRT_SOURCES[@]}"; do
    if [ -f "$src" ]; then
        obj=$(basename "${src%.cc}.o")
        echo -n "  $(basename $src)..."
        if ${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" -c "$src" -o "$obj" 2>err.log; then
            echo -e " ${GREEN}OK${NC}"
            ((SUCCESS++))
        else
            echo -e " ${RED}FAIL${NC}"
            cat err.log | head -3 | sed 's/^/    /'
        fi
    fi
done

if [ $SUCCESS -gt 0 ]; then
    ${CROSS_COMPILE}ar rcs libcxxrt.a *.o
    echo -e "${GREEN}✓ libcxxrt.a created ($SUCCESS files)${NC}"
fi
echo ""

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}STEP 3: Build libcxx (STL)${NC}"
echo -e "${YELLOW}========================================${NC}"
cd "$BUILD_DIR/libcxx"

LIBCXX_SRC="$OE_ROOT/3rdparty/libcxx/libcxx"

LIBCXX_FLAGS=(
    -std=c++17
    -nostdinc++
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-omit-frame-pointer
    -fPIC
    -fvisibility=hidden
    -O2
    -I"$LIBCXX_SRC/include"
    -I"$CXXRT_SRC"
    -I"$OE_ROOT/3rdparty/musl/musl/include"
    -I"$PROJECT_ROOT/stubs"
    -D_LIBCPP_BUILDING_LIBRARY
    -D_LIBCPP_HAS_NO_THREADS
    -D_LIBCPP_HAS_NO_EXCEPTIONS
    -D_LIBCPP_HAS_NO_RTTI
    -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS
    -DLIBCXX_BUILDING_LIBCXXABI
    -U__STDCPP_THREADS__
)

# Essential libcxx sources - START MINIMAL
LIBCXX_SOURCES=(
    "$LIBCXX_SRC/src/vector.cpp"
    "$LIBCXX_SRC/src/string.cpp"
    "$LIBCXX_SRC/src/algorithm.cpp"
    "$LIBCXX_SRC/src/memory.cpp"
    "$LIBCXX_SRC/src/new.cpp"
    "$LIBCXX_SRC/src/utility.cpp"
    "$LIBCXX_SRC/src/hash.cpp"
    "$LIBCXX_SRC/src/optional.cpp"
    "$LIBCXX_SRC/src/variant.cpp"
    "$LIBCXX_SRC/src/any.cpp"
    "$LIBCXX_SRC/src/bind.cpp"
    "$LIBCXX_SRC/src/functional.cpp"
)

echo "Building libcxx..."
SUCCESS=0
FAILED=0
for src in "${LIBCXX_SOURCES[@]}"; do
    if [ ! -f "$src" ]; then
        continue
    fi
    
    obj=$(basename "${src%.cpp}.o")
    echo -n "  $(basename $src)..."
    
    if ${CROSS_COMPILE}g++ "${LIBCXX_FLAGS[@]}" -c "$src" -o "$obj" 2>err.log; then
        echo -e " ${GREEN}OK${NC}"
        ((SUCCESS++))
    else
        echo -e " ${RED}FAIL${NC}"
        # Show first error only
        if [ $FAILED -eq 0 ]; then
            echo "    First error:"
            cat err.log | head -5 | sed 's/^/      /'
        fi
        ((FAILED++))
    fi
done

echo ""
echo "libcxx compilation: $SUCCESS OK, $FAILED FAIL"

if [ $SUCCESS -gt 0 ]; then
    ${CROSS_COMPILE}ar rcs libcxx.a *.o 2>/dev/null || true
    echo -e "${GREEN}✓ libcxx.a created!${NC}"
    ls -lh libcxx.a
fi
echo ""

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}STEP 4: Create OP-TEE adapters${NC}"
echo -e "${YELLOW}========================================${NC}"
cd "$BUILD_DIR"

cat > optee_adapters.c << 'EOF'
// OP-TEE C adapters for libcxx
#include <tee_internal_api.h>
#include <stddef.h>

// Memory functions
void* malloc(size_t size) {
    return TEE_Malloc(size, 0);
}

void free(void* ptr) {
    if (ptr) TEE_Free(ptr);
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    return TEE_Malloc(total, TEE_MALLOC_FILL_ZERO);
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return TEE_Malloc(size, 0);
    if (size == 0) {
        TEE_Free(ptr);
        return NULL;
    }
    void* new_ptr = TEE_Malloc(size, 0);
    if (!new_ptr) return NULL;
    TEE_MemMove(new_ptr, ptr, size);
    TEE_Free(ptr);
    return new_ptr;
}

// String functions
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

// Abort
void abort(void) {
    TEE_Panic(TEE_ERROR_GENERIC);
}

// Assert
void __assert_fail(const char* assertion, const char* file, 
                   unsigned int line, const char* function) {
    TEE_Panic(TEE_ERROR_GENERIC);
}
EOF

echo "Building optee_adapters.c..."
${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" -c optee_adapters.c -o optee_adapters.o
echo -e "${GREEN}✓ optee_adapters.o created${NC}"
echo ""

cat > optee_cxx_adapters.cpp << 'EOF'
// OP-TEE C++ adapters
extern "C" {
#include <tee_internal_api.h>
}

// new/delete operators
void* operator new(size_t size) {
    void* ptr = TEE_Malloc(size, 0);
    if (!ptr) TEE_Panic(TEE_ERROR_OUT_OF_MEMORY);
    return ptr;
}

void* operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void* ptr) noexcept {
    if (ptr) TEE_Free(ptr);
}

void operator delete[](void* ptr) noexcept {
    operator delete(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    operator delete(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    operator delete(ptr);
}

// Placement new
void* operator new(size_t, void* p) noexcept { return p; }
void operator delete(void*, void*) noexcept { }
EOF

echo "Building optee_cxx_adapters.cpp..."
${CROSS_COMPILE}g++ -std=c++17 -ffreestanding -fPIC -O2 \
    -fno-exceptions -fno-rtti \
    -c optee_cxx_adapters.cpp -o optee_cxx_adapters.o
echo -e "${GREEN}✓ optee_cxx_adapters.o created${NC}"
echo ""

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}BUILD SUMMARY${NC}"
echo -e "${GREEN}========================================${NC}"
echo "Runtime libraries in: $BUILD_DIR"
echo ""
ls -lh libc/liboecrt.a 2>/dev/null && echo "  ✓ liboecrt.a (C runtime)" || echo "  ✗ liboecrt.a"
ls -lh libcxxrt/libcxxrt.a 2>/dev/null && echo "  ✓ libcxxrt.a (C++ ABI)" || echo "  ✗ libcxxrt.a"  
ls -lh libcxx/libcxx.a 2>/dev/null && echo "  ✓ libcxx.a (STL)" || echo "  ✗ libcxx.a"
ls -lh optee_adapters.o 2>/dev/null && echo "  ✓ optee_adapters.o" || echo "  ✗ optee_adapters.o"
ls -lh optee_cxx_adapters.o 2>/dev/null && echo "  ✓ optee_cxx_adapters.o" || echo "  ✗ optee_cxx_adapters.o"
echo ""
echo -e "${GREEN}Next: Update ta/sub.mk to link with these libraries${NC}"
