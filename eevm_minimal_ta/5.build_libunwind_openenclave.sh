#!/bin/bash
set -e

CROSS_COMPILE=aarch64-none-linux-gnu-
ARCH=aarch64
UNWIND_SRC=external/openenclave/3rdparty/libunwind/libunwind
OE_UNWIND=external/openenclave/3rdparty/libunwind
MUSL_INC=build_oe_libs/musl/include
OE_STUB=build_oe_libs/openenclave_stub
OUTPUT_DIR=build_libunwind
OUTPUT_LIB=build_libunwind/libunwind.a  # Keep in build_libunwind directory
export PATH=/home/abc/arm-toolchain/bin:$PATH
echo "===== Building libunwind for OP-TEE TA (aarch64) ====="
echo "Following OpenEnclave's CMakeLists.txt specification"

# Create output directory
mkdir -p $OUTPUT_DIR
mkdir -p $OUTPUT_DIR/tdep

# Configure files (like CMake does)
echo "Preparing configuration files..."

# Copy libunwind-aarch64.h as libunwind.h
cp "$UNWIND_SRC/include/libunwind-$ARCH.h" "$OUTPUT_DIR/libunwind.h"

# Configure libunwind-common.inc
cp "$UNWIND_SRC/include/libunwind-common.h.in" "$OUTPUT_DIR/libunwind-common.inc"
sed -i 's/@PKG_MAJOR@/1/g; s/@PKG_MINOR@/3/g' "$OUTPUT_DIR/libunwind-common.inc"

# Create empty config.h
echo "/* Empty file */" > "$OUTPUT_DIR/config.h"

# Copy tdep headers
cp -r "$UNWIND_SRC/include/tdep-$ARCH/"* "$OUTPUT_DIR/tdep/" 2>/dev/null || true

# Copy Gstep.c as Gstep.inc (for aarch64)
cp "$UNWIND_SRC/src/$ARCH/Gstep.c" "$OUTPUT_DIR/Gstep.inc"

echo "✓ Configuration files prepared"

# Compiler flags matching OpenEnclave
COMMON_FLAGS=(
    -nostdinc
    -ffreestanding
    -fno-builtin
    -fPIC
    -g
    -O2
    -fstack-protector-strong
    -march=armv8-a
    -I$OE_STUB
    -I$MUSL_INC
    -I"$OE_UNWIND"
    -I"$UNWIND_SRC/include"
    -I"$UNWIND_SRC/src/$ARCH"
    -I"$UNWIND_SRC/src"
    -I"$OUTPUT_DIR/tdep"
    -I"$OUTPUT_DIR"
    -include "$OE_UNWIND/stubs.h"
    -DHAVE_ELF_H
    -DHAVE_ENDIAN_H
    -DHAVE_LINK_H
    -D_GNU_SOURCE
    -DUNW_LOCAL_ONLY=1
    -DHAVE_DL_ITERATE_PHDR
    -DPACKAGE_STRING=\"libunwind-1.3\"
    -DPACKAGE_BUGREPORT=\"unwind.org\"
    -Wno-error
)

ASM_FLAGS=(
    -march=armv8-a
    -I"$OUTPUT_DIR"
    -Werror
)

# Source files from CMakeLists.txt (aarch64 configuration)
C_SOURCES=(
    # dwarf (7 files)
    "libunwind/src/dwarf/global.c"
    "libunwind/src/dwarf/Lexpr.c"
    "libunwind/src/dwarf/Lfde.c"
    "libunwind/src/dwarf/Lfind_proc_info-lsb.c"
    "libunwind/src/dwarf/Lfind_unwind_table.c"
    "libunwind/src/dwarf/Lparser.c"
    "libunwind/src/dwarf/Lpe.c"
    
    # mi (23 files)
    "libunwind/src/mi/_ReadULEB.c"
    "libunwind/src/mi/_ReadSLEB.c"
    "libunwind/src/mi/backtrace.c"
    "libunwind/src/mi/dyn-cancel.c"
    "libunwind/src/mi/dyn-info-list.c"
    "libunwind/src/mi/dyn-register.c"
    "libunwind/src/mi/flush_cache.c"
    "libunwind/src/mi/init.c"
    "libunwind/src/mi/Ldestroy_addr_space.c"
    "libunwind/src/mi/Ldyn-extract.c"
    "libunwind/src/mi/Lfind_dynamic_proc_info.c"
    "libunwind/src/mi/Lget_accessors.c"
    "libunwind/src/mi/Lget_fpreg.c"
    "libunwind/src/mi/Lget_proc_info_by_ip.c"
    "libunwind/src/mi/Lget_proc_name.c"
    "libunwind/src/mi/Lget_reg.c"
    "libunwind/src/mi/Lput_dynamic_unwind_info.c"
    "libunwind/src/mi/Lset_cache_size.c"
    "libunwind/src/mi/Lset_caching_policy.c"
    "libunwind/src/mi/Lset_fpreg.c"
    "libunwind/src/mi/Lset_reg.c"
    "libunwind/src/mi/mempool.c"
    "libunwind/src/mi/strerror.c"
    
    # unwind (18 files)
    "libunwind/src/unwind/Backtrace.c"
    "libunwind/src/unwind/DeleteException.c"
    "libunwind/src/unwind/FindEnclosingFunction.c"
    "libunwind/src/unwind/ForcedUnwind.c"
    "libunwind/src/unwind/GetBSP.c"
    "libunwind/src/unwind/GetCFA.c"
    "libunwind/src/unwind/GetDataRelBase.c"
    "libunwind/src/unwind/GetGR.c"
    "libunwind/src/unwind/GetIPInfo.c"
    "libunwind/src/unwind/GetIP.c"
    "libunwind/src/unwind/GetLanguageSpecificData.c"
    "libunwind/src/unwind/GetRegionStart.c"
    "libunwind/src/unwind/GetTextRelBase.c"
    "libunwind/src/unwind/RaiseException.c"
    "libunwind/src/unwind/Resume.c"
    "libunwind/src/unwind/Resume_or_Rethrow.c"
    "libunwind/src/unwind/SetGR.c"
    "libunwind/src/unwind/SetIP.c"
    
    # aarch64 specific (14 files)
    "libunwind/src/aarch64/is_fpreg.c"
    "libunwind/src/aarch64/Lcreate_addr_space.c"
    "libunwind/src/aarch64/Lget_save_loc.c"
    "libunwind/src/aarch64/Lglobal.c"
    "libunwind/src/aarch64/Linit.c"
    "libunwind/src/aarch64/Linit_local.c"
    "libunwind/src/aarch64/Linit_remote.c"
    "libunwind/src/aarch64/Lget_proc_info.c"
    "libunwind/src/aarch64/Lregs.c"
    "libunwind/src/aarch64/Lresume.c"
    "libunwind/src/aarch64/Lstash_frame.c"
    "libunwind/src/aarch64/Lstep.c"
    "libunwind/src/aarch64/Ltrace.c"
    "libunwind/src/aarch64/regname.c"
    
    # platform (2 files)
    "libunwind/src/os-linux.c"
    "libunwind/src/elf64.c"
)

ASM_SOURCES=(
    # aarch64 assembly (1 file)
    "libunwind/src/aarch64/getcontext.S"
)

OBJECTS=()
FAILED=()

echo ""
echo "Compiling C source files (64 files)..."
for src in "${C_SOURCES[@]}"; do
    # Extract filename for output
    filename=$(basename "$src")
    obj_name="${filename%.c}.o"
    obj_path="$OUTPUT_DIR/$obj_name"
    
    # Show relative path for clarity
    rel_src="${src#libunwind/src/}"
    echo -n "  [$((${#OBJECTS[@]}+1))/65] $rel_src... "
    
    if ${CROSS_COMPILE}gcc "${COMMON_FLAGS[@]}" -c "$OE_UNWIND/$src" -o "$obj_path" 2>"$obj_path.log"; then
        echo "OK"
        OBJECTS+=("$obj_path")
    else
        echo "FAIL"
        FAILED+=("$src")
    fi
done

echo ""
echo "Compiling assembly files (1 file)..."
for src in "${ASM_SOURCES[@]}"; do
    filename=$(basename "$src")
    obj_name="${filename%.S}.o"
    obj_path="$OUTPUT_DIR/$obj_name"
    
    rel_src="${src#libunwind/src/}"
    echo -n "  [65/65] $rel_src... "
    
    if ${CROSS_COMPILE}gcc "${ASM_FLAGS[@]}" -c "$OE_UNWIND/$src" -o "$obj_path" 2>"$obj_path.log"; then
        echo "OK"
        OBJECTS+=("$obj_path")
    else
        echo "FAIL"
        FAILED+=("$src")
    fi
done

echo ""
echo "===== Build Summary ====="
TOTAL=$((${#C_SOURCES[@]} + ${#ASM_SOURCES[@]}))
echo "Successfully built: ${#OBJECTS[@]}/$TOTAL objects"

if [ ${#FAILED[@]} -gt 0 ]; then
    echo ""
    echo "Failed files:"
    for failed in "${FAILED[@]}"; do
        echo "  - $failed"
    done
    echo ""
    echo "Check .log files in $OUTPUT_DIR/ for details"
fi

if [ ${#OBJECTS[@]} -gt 50 ]; then
    echo ""
    echo "Creating static library: $OUTPUT_LIB"
    ${CROSS_COMPILE}ar rcs "$OUTPUT_LIB" "${OBJECTS[@]}"
    
    echo ""
    echo "Library info:"
    ls -lh "$OUTPUT_LIB"
    
    echo ""
    echo "✓ libunwind build successful!"
    echo "  Location: $OUTPUT_LIB"
    echo "  Size: $(du -h $OUTPUT_LIB | cut -f1)"
    echo "  Objects: ${#OBJECTS[@]}/$TOTAL"
    echo ""
    echo "This library provides:"
    echo "  - Stack unwinding for exception handling"
    echo "  - _Unwind_* functions required by libgcc_eh.a"
    echo "  - DWARF debug info parsing"
    echo "  - Platform-specific unwinding (aarch64)"
else
    echo ""
    echo "✗ Build incomplete: only ${#OBJECTS[@]}/$TOTAL objects built"
    echo "Need at least 50 objects for a functional library"
    exit 1
fi
