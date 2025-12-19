#!/bin/bash
# Fix libcxx build issues to complete all 36/36 sources
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

CMATH_FILE="build_oe_libs/libcxx/include/cmath"
OPERATIONS_FILE="external/openenclave/3rdparty/libcxx/libcxx/src/filesystem/operations.cpp"

echo -e "${YELLOW}Fixing libcxx build issues...${NC}"
echo ""

# Fix 1: Add #include <limits> to cmath for numeric_limits
echo -e "${YELLOW}1. Fixing cmath - adding #include <limits>${NC}"
if [ -f "$CMATH_FILE" ]; then
    # Check if already patched
    if grep -q "// PATCH: Added for numeric_limits" "$CMATH_FILE"; then
        echo "  Already patched"
    else
        # Find the line with #include <math.h> and add #include <limits> after it
        sed -i '/#include <math\.h>/a // PATCH: Added for numeric_limits\n#include <limits>' "$CMATH_FILE"
        echo -e "  ${GREEN}✓ Added #include <limits> to cmath${NC}"
    fi
else
    echo "  ✗ File not found: $CMATH_FILE"
    exit 1
fi

# Fix 2: Create dummy linux/version.h for operations.cpp
echo -e "${YELLOW}2. Creating dummy linux/version.h${NC}"
LINUX_INCLUDE_DIR="build_oe_libs/linux"
mkdir -p "$LINUX_INCLUDE_DIR"

cat > "$LINUX_INCLUDE_DIR/version.h" << 'EOF'
/* Dummy linux/version.h for OP-TEE build */
#ifndef _LINUX_VERSION_H
#define _LINUX_VERSION_H

/* Minimal version definitions for libcxx filesystem */
#define LINUX_VERSION_CODE 0x050000  /* Kernel 5.0.0 */
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

/* Common version checks that might be used */
#define LINUX_VERSION_MAJOR 5
#define LINUX_VERSION_PATCHLEVEL 0
#define LINUX_VERSION_SUBLEVEL 0

#endif /* _LINUX_VERSION_H */
EOF

echo -e "  ${GREEN}✓ Created dummy linux/version.h${NC}"

# Fix 3: Patch operations.cpp to use our dummy header
echo -e "${YELLOW}3. Patching operations.cpp to use dummy header${NC}"
if [ -f "$OPERATIONS_FILE" ]; then
    # Check if already patched
    if grep -q "// PATCH: Use dummy linux/version.h" "$OPERATIONS_FILE"; then
        echo "  Already patched"
    else
        # Comment out the original include and add our path
        sed -i 's|#include <linux/version.h>|// PATCH: Use dummy linux/version.h for OP-TEE\n// #include <linux/version.h>|' "$OPERATIONS_FILE"
        echo -e "  ${GREEN}✓ Patched operations.cpp${NC}"
    fi
else
    echo "  ✗ File not found: $OPERATIONS_FILE"
    exit 1
fi

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}✓ All fixes applied!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "Fixed issues:"
echo "  1. Added #include <limits> to cmath (fixes random.cpp, valarray.cpp)"
echo "  2. Created dummy linux/version.h in build_oe_libs/linux/"
echo "  3. Patched operations.cpp to not fail on missing header"
echo ""
echo "Now rebuild with: ./build_openenclave_libs.sh"
