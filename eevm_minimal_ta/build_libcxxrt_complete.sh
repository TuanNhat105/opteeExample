#!/bin/bash
set -e

CROSS_COMPILE=aarch64-none-linux-gnu-
LIBCXXRT_SRC=external/openenclave/3rdparty/libcxxrt/libcxxrt/src
LIBCXX_INC=build_oe_libs/libcxx/include
LIBCXXRT_INC=build_oe_libs/libcxxrt/include
MUSL_INC=build_oe_libs/musl/include
OE_STUB=build_oe_libs/openenclave_stub
OUTPUT_DIR=build_oe_libs/libcxxrt
OUTPUT_LIB=build_oe_libs/libcxxrt/libcxxrt.a  # Keep in libcxxrt directory

echo "===== Building Complete libcxxrt for OP-TEE TA ====="
echo "Following OpenEnclave's CMakeLists.txt specification"

# Create output directory
mkdir -p $OUTPUT_DIR

# Compiler flags matching OpenEnclave's libcxxrt build
COMMON_FLAGS=(
    -nostdinc++
    -ffreestanding
    -fno-builtin
    -fPIC
    -g
    -O2
    -fstack-protector-strong
    -march=armv8-a
    -I$OE_STUB
    -I$LIBCXXRT_INC
    -I$MUSL_INC
    -I$LIBCXX_INC
    -std=c++14
    -fexceptions
    -funwind-tables
    -frtti
    -D__STDC_HOSTED__=1
    -D_GNU_SOURCE
    -D_LIBCPP_HAS_NO_THREADS
    -DLIBCXXRT
    -Wno-builtin-macro-redefined
    -Wno-error
)

# C compiler flags for libelftc_dem_gnu3.c
C_FLAGS=(
    -nostdinc
    -ffreestanding
    -fno-builtin
    -fPIC
    -g
    -O2
    -fstack-protector-strong
    -march=armv8-a
    -I$MUSL_INC
    -D_GNU_SOURCE
)

# Source files to compile (from CMakeLists.txt)
CPP_SOURCES=(
    "dynamic_cast.cc"
    "exception.cc"
    "guard.cc"
    "stdexcept.cc"
    "typeinfo.cc"
    "memory.cc"
    "auxhelper.cc"
)

C_SOURCES=(
    "libelftc_dem_gnu3.c"
)

OBJECTS=()

echo ""
echo "Compiling C++ source files..."
for src in "${CPP_SOURCES[@]}"; do
    obj_name="${src%.cc}.o"
    obj_path="$OUTPUT_DIR/$obj_name"
    
    echo -n "Building $src... "
    
    if ${CROSS_COMPILE}g++ "${COMMON_FLAGS[@]}" -c "$LIBCXXRT_SRC/$src" -o "$obj_path" 2>&1 | tee "$obj_path.log"; then
        if [ -f "$obj_path" ]; then
            echo "OK"
            OBJECTS+=("$obj_path")
        else
            echo "FAIL (no output file)"
            echo "Check log: $obj_path.log"
        fi
    else
        echo "FAIL"
        echo "Check log: $obj_path.log"
    fi
done

echo ""
echo "Compiling C source files..."
for src in "${C_SOURCES[@]}"; do
    obj_name="${src%.c}.o"
    obj_path="$OUTPUT_DIR/$obj_name"
    
    echo -n "Building $src... "
    
    if ${CROSS_COMPILE}gcc "${C_FLAGS[@]}" -c "$LIBCXXRT_SRC/$src" -o "$obj_path" 2>&1 | tee "$obj_path.log"; then
        if [ -f "$obj_path" ]; then
            echo "OK"
            OBJECTS+=("$obj_path")
        else
            echo "FAIL (no output file)"
            echo "Check log: $obj_path.log"
        fi
    else
        echo "FAIL"
        echo "Check log: $obj_path.log"
    fi
done

echo ""
echo "===== Build Summary ====="
TOTAL=$((${#CPP_SOURCES[@]} + ${#C_SOURCES[@]}))
echo "Successfully built: ${#OBJECTS[@]}/$TOTAL objects"

if [ ${#OBJECTS[@]} -eq $TOTAL ]; then
    echo ""
    echo "Creating static library: $OUTPUT_LIB"
    ${CROSS_COMPILE}ar rcs "$OUTPUT_LIB" "${OBJECTS[@]}"
    
    echo ""
    echo "Library info:"
    ls -lh "$OUTPUT_LIB"
    echo ""
    echo "Exception handling symbols:"
    ${CROSS_COMPILE}nm -C "$OUTPUT_LIB" | grep -E "__cxa_allocate_exception|__cxa_throw|__cxa_free_exception|__cxa_demangle|__gxx_personality" || echo "  (no exception symbols found - check build)"
    
    echo ""
    echo "✓ Complete libcxxrt build successful!"
    echo "  Location: $OUTPUT_LIB"
    echo "  Size: $(du -h $OUTPUT_LIB | cut -f1)"
    echo ""
    echo "This library provides:"
    echo "  - C++ exception handling (__cxa_allocate_exception, __cxa_throw, etc)"
    echo "  - RTTI support (typeinfo, dynamic_cast)"
    echo "  - Name demangling (__cxa_demangle_gnu3)"
    echo "  - Guard variables (__cxa_guard_*)"
else
    echo ""
    echo "✗ Build incomplete: ${#OBJECTS[@]}/$TOTAL objects built"
    echo "Check individual .log files for errors"
    exit 1
fi

