#!/bin/bash
# Build minimal libcxx for OP-TEE (NO exceptions, NO RTTI initially)
# Based on OpenEnclave approach

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
LIBCXX_SRC="$PROJECT_ROOT/external/openenclave/3rdparty/libcxx/libcxx"
LIBCXXRT_SRC="$PROJECT_ROOT/external/openenclave/3rdparty/libcxxrt/libcxxrt"
BUILD_DIR="$PROJECT_ROOT/build_runtime"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"

echo -e "${GREEN}=======================================${NC}"
echo -e "${GREEN}Build libcxx for OP-TEE TA${NC}"
echo -e "${GREEN}=======================================${NC}"
echo ""

# Check prerequisites
if [ ! -d "$LIBCXX_SRC/include" ]; then
    echo -e "${RED}ERROR: libcxx not found!${NC}"
    echo "Please run: ./download_libcxx.sh"
    exit 1
fi

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo -e "${YELLOW}Step 1: Compiling libcxx sources...${NC}"
echo ""

# Compiler flags (NO exceptions, NO RTTI for now - simpler)
CXXFLAGS=(
    -std=c++17
    -nostdinc++
    -ffreestanding
    -fno-exceptions          # Start simple
    -fno-rtti                # Start simple  
    -fno-threadsafe-statics
    -fno-omit-frame-pointer
    -fPIC
    -fvisibility=hidden
    -O2
    -I"$LIBCXX_SRC/include"
    -I"$LIBCXXRT_SRC/src"
    -I"$PROJECT_ROOT/stubs"
    -D_LIBCPP_BUILDING_LIBRARY
    -D_LIBCPP_HAS_NO_THREADS
    -D_LIBCPP_HAS_NO_EXCEPTIONS
    -D_LIBCPP_HAS_NO_RTTI
    -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS
    -DLIBCXX_BUILDING_LIBCXXABI
    -U__STDCPP_THREADS__
)

# Essential sources only (no iostream, locale, filesystem, etc.)
SOURCES=(
    "$LIBCXX_SRC/src/algorithm.cpp"
    "$LIBCXX_SRC/src/any.cpp"
    "$LIBCXX_SRC/src/bind.cpp"
    "$LIBCXX_SRC/src/charconv.cpp"
    "$LIBCXX_SRC/src/exception.cpp"
    "$LIBCXX_SRC/src/functional.cpp"
    "$LIBCXX_SRC/src/hash.cpp"
    "$LIBCXX_SRC/src/memory.cpp"
    "$LIBCXX_SRC/src/new.cpp"
    "$LIBCXX_SRC/src/optional.cpp"
    "$LIBCXX_SRC/src/stdexcept.cpp"
    "$LIBCXX_SRC/src/string.cpp"
    "$LIBCXX_SRC/src/typeinfo.cpp"
    "$LIBCXX_SRC/src/utility.cpp"
    "$LIBCXX_SRC/src/valarray.cpp"
    "$LIBCXX_SRC/src/variant.cpp"
    "$LIBCXX_SRC/src/vector.cpp"
)

SUCCESS=0
FAILED=0

for src in "${SOURCES[@]}"; do
    if [ ! -f "$src" ]; then
        echo -e "${YELLOW}  [SKIP] $(basename $src) - not found${NC}"
        continue
    fi
    
    obj=$(basename "${src%.cpp}.o")
    echo -n "  Compiling $(basename $src)..."
    
    if ${CROSS_COMPILE}g++ "${CXXFLAGS[@]}" -c "$src" -o "$obj" 2>build_error.log; then
        echo -e " ${GREEN}OK${NC}"
        ((SUCCESS++))
    else
        echo -e " ${RED}FAIL${NC}"
        echo "    Error log:"
        cat build_error.log | head -5 | sed 's/^/      /'
        ((FAILED++))
    fi
done

echo ""
echo "Compilation summary:"
echo "  Success: $SUCCESS files"
echo "  Failed:  $FAILED files"
echo ""

# Create static library
if [ $SUCCESS -gt 0 ]; then
    echo -e "${YELLOW}Step 2: Creating libcxx.a...${NC}"
    
    ${CROSS_COMPILE}ar rcs libcxx.a *.o 2>/dev/null || true
    ${CROSS_COMPILE}ranlib libcxx.a
    
    echo -e "${GREEN}✓ libcxx.a created!${NC}"
    ls -lh libcxx.a
    echo ""
fi

# Build minimal new/delete operators
echo -e "${YELLOW}Step 3: Creating optee_cxx_support.cpp...${NC}"

cat > optee_cxx_support.cpp << 'EOF'
// Minimal C++ support for OP-TEE
extern "C" {
#include <tee_internal_api.h>
}

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
inline void* operator new(size_t, void* p) noexcept { return p; }
inline void operator delete(void*, void*) noexcept { }

// abort for libcxx
extern "C" void abort(void) {
    TEE_Panic(TEE_ERROR_GENERIC);
}

// Basic memory functions
extern "C" {
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
}
EOF

echo "Compiling optee_cxx_support.cpp..."
${CROSS_COMPILE}g++ "${CXXFLAGS[@]}" -c optee_cxx_support.cpp -o optee_cxx_support.o

echo -e "${GREEN}✓ optee_cxx_support.o created!${NC}"
echo ""

echo -e "${GREEN}=======================================${NC}"
echo -e "${GREEN}Build Complete!${NC}"
echo -e "${GREEN}=======================================${NC}"
echo ""
echo "Output files in: $BUILD_DIR"
echo "  - libcxx.a (STL containers)"
echo "  - optee_cxx_support.o (new/delete, memcpy, etc.)"
echo ""
echo "Next: Update ta/sub.mk to link these files"
