#!/bin/bash
# Script to verify exception handling build setup for OP-TEE TA

set -e

echo "=========================================="
echo "Verifying Exception Handling Build Setup"
echo "=========================================="
echo ""

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
TA_DIR="$PROJECT_ROOT/ta"
# Try to detect cross-compiler prefix
if command -v aarch64-none-linux-gnu-nm >/dev/null 2>&1; then
    CROSS_COMPILE="aarch64-none-linux-gnu-"
elif command -v aarch64-linux-gnu-nm >/dev/null 2>&1; then
    CROSS_COMPILE="aarch64-linux-gnu-"
else
    CROSS_COMPILE=""
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

check_pass() {
    echo -e "${GREEN}✓${NC} $1"
}

check_fail() {
    echo -e "${RED}✗${NC} $1"
}

check_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# 1. Check if libraries exist
echo "1. Checking required libraries..."
LIBCXXRT="$PROJECT_ROOT/build_oe_libs/libcxxrt/libcxxrt.a"
LIBUNWIND="$PROJECT_ROOT/build_libunwind/libunwind.a"
LIBOELIBC="$PROJECT_ROOT/libc/liboelibc.a"

if [ -f "$LIBCXXRT" ]; then
    check_pass "libcxxrt.a exists ($(du -h "$LIBCXXRT" | cut -f1))"
else
    check_fail "libcxxrt.a NOT FOUND at $LIBCXXRT"
    echo "  Run: ./build_libcxxrt_complete.sh"
    exit 1
fi

if [ -f "$LIBUNWIND" ]; then
    check_pass "libunwind.a exists ($(du -h "$LIBUNWIND" | cut -f1))"
else
    check_fail "libunwind.a NOT FOUND at $LIBUNWIND"
    echo "  Run: ./build_libunwind_openenclave.sh"
    exit 1
fi

if [ -f "$LIBOELIBC" ]; then
    check_pass "liboelibc.a exists ($(du -h "$LIBOELIBC" | cut -f1))"
else
    check_fail "liboelibc.a NOT FOUND at $LIBOELIBC"
    echo "  Run: ./build_openenclave_libs.sh"
    exit 1
fi

echo ""

# 2. Check for required symbols in libcxxrt
echo "2. Checking exception symbols in libcxxrt..."
REQUIRED_SYMBOLS=(
    "__cxa_throw"
    "__cxa_begin_catch"
    "__cxa_allocate_exception"
    "__cxa_free_exception"
)

# Use nm with -A flag to show all symbols from archive members
MISSING_SYMBOLS=()
for sym in "${REQUIRED_SYMBOLS[@]}"; do
    # Check for defined (T) or weak (w) symbols in archive
    if nm -A "$LIBCXXRT" 2>/dev/null | grep -qE "[[:space:]][Tw][[:space:]]+.*$sym$"; then
        check_pass "$sym found in libcxxrt"
    else
        check_fail "$sym NOT FOUND in libcxxrt"
        MISSING_SYMBOLS+=("$sym")
    fi
done

if [ ${#MISSING_SYMBOLS[@]} -gt 0 ]; then
    echo "  Rebuild libcxxrt: ./build_libcxxrt_complete.sh"
    exit 1
fi

echo ""

# 3. Check for unwind symbols in libunwind
echo "3. Checking unwind symbols in libunwind..."
if nm -A "$LIBUNWIND" 2>/dev/null | grep -qE "[[:space:]][Tw][[:space:]]+.*_Unwind_RaiseException$"; then
    check_pass "_Unwind_RaiseException found in libunwind"
else
    check_fail "_Unwind_RaiseException NOT FOUND in libunwind"
    echo "  Rebuild libunwind: ./build_libunwind_openenclave.sh"
    exit 1
fi

echo ""

# 4. Check TA binary
echo "4. Checking TA binary..."
TA_ELF="$TA_DIR/8aaaf200-2450-11e4-abe2-0002a5d5c51b.elf"

if [ ! -f "$TA_ELF" ]; then
    check_fail "TA binary not found. Building..."
    cd "$TA_DIR"
    make
    cd "$PROJECT_ROOT"
fi

if [ -f "$TA_ELF" ]; then
    check_pass "TA binary exists ($(du -h "$TA_ELF" | cut -f1))"
    
    # Check if exception symbols are linked
    echo "   Checking linked exception symbols..."
    if nm "$TA_ELF" 2>/dev/null | grep -qE "[[:space:]]T[[:space:]]+__cxa_throw$"; then
        check_pass "__cxa_throw is linked in TA"
    else
        check_fail "__cxa_throw NOT linked in TA"
        echo "   Rebuild TA: cd ta && make clean && make"
        exit 1
    fi
    
    if nm "$TA_ELF" 2>/dev/null | grep -qE "[[:space:]]T[[:space:]]+_Unwind_RaiseException$"; then
        check_pass "_Unwind_RaiseException is linked in TA"
    else
        check_fail "_Unwind_RaiseException NOT linked in TA"
        echo "   Rebuild TA: cd ta && make clean && make"
        exit 1
    fi
else
    check_fail "Failed to build TA binary"
    exit 1
fi

echo ""

# 5. Check compiler flags
echo "5. Checking compiler flags in sub.mk..."
if grep -q "cppflags-y += -fexceptions" "$TA_DIR/sub.mk"; then
    check_pass "-fexceptions flag is set"
else
    check_fail "-fexceptions flag is MISSING"
    echo "   Add to ta/sub.mk: cppflags-y += -fexceptions"
fi

if grep -q "cppflags-y += -funwind-tables" "$TA_DIR/sub.mk"; then
    check_pass "-funwind-tables flag is set"
else
    check_fail "-funwind-tables flag is MISSING"
    echo "   Add to ta/sub.mk: cppflags-y += -funwind-tables"
fi

if grep -q "cppflags-y += -frtti" "$TA_DIR/sub.mk"; then
    check_pass "-frtti flag is set"
else
    check_warn "-frtti flag is not set (optional for basic exceptions)"
fi

echo ""

# 6. Check Makefile linking
echo "6. Checking Makefile linking order..."
if grep -q "LIBCXXRT.*LIBUNWIND" "$TA_DIR/Makefile"; then
    check_pass "Libraries are referenced in Makefile"
else
    check_fail "Libraries not properly linked in Makefile"
    echo "   Check ta/Makefile for LIBCXXRT and LIBUNWIND"
fi

echo ""

# 7. Summary
echo "=========================================="
echo "Summary"
echo "=========================================="
echo -e "${GREEN}All checks passed!${NC}"
echo ""
echo "Your exception handling setup looks correct."
echo ""
echo "To test exception handling:"
echo "  1. Build and deploy TA: cd ta && make"
echo "  2. Run host application: cd host && ./minimal_evm_host"
echo "  3. Test exception command: Use TA_MINIMAL_EVM_CMD_TEST_EXCEPTION"
echo ""
echo "If you encounter runtime errors:"
echo "  - Check OP-TEE logs: dmesg | grep -i optee"
echo "  - Verify TA is loaded: lsmod | grep optee"
echo "  - Check TA logs in secure world console"
echo ""

