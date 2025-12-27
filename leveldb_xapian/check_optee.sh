#!/bin/bash
# Script to check OP-TEE driver availability on device

echo "=========================================="
echo "Checking OP-TEE Driver on Device"
echo "=========================================="
echo ""

# Check if /dev/tee0 exists
echo "1. Checking /dev/tee0..."
if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -c /dev/tee0" 2>/dev/null; then
    echo "   ✓ /dev/tee0 exists"
    ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ls -l /dev/tee*" 2>/dev/null
else
    echo "   ✗ /dev/tee0 NOT FOUND - OP-TEE driver may not be loaded"
fi

echo ""
echo "2. Checking OP-TEE kernel module..."
ssh -o StrictHostKeyChecking=no root@192.168.1.114 "lsmod | grep -i optee || echo '   ✗ OP-TEE kernel module not loaded'" 2>/dev/null

echo ""
echo "3. Checking OP-TEE supplicant..."
if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pgrep -x tee-supplicant > /dev/null" 2>/dev/null; then
    echo "   ✓ tee-supplicant is running"
else
    echo "   ✗ tee-supplicant is NOT running"
    echo "   Try: systemctl start tee-supplicant"
fi

echo ""
echo "4. Testing TEEC_InitializeContext with a simple test..."
ssh -o StrictHostKeyChecking=no root@192.168.1.114 "cat > /tmp/test_tee.c << 'EOFTEST'
#include <tee_client_api.h>
#include <stdio.h>
int main() {
    TEEC_Context ctx;
    TEEC_Result res = TEEC_InitializeContext(NULL, &ctx);
    if (res == TEEC_SUCCESS) {
        printf(\"SUCCESS: TEEC_InitializeContext works!\\n\");
        TEEC_FinalizeContext(&ctx);
        return 0;
    } else {
        printf(\"ERROR: TEEC_InitializeContext failed with 0x%x\\n\", res);
        return 1;
    }
}
EOFTEST
" 2>/dev/null

echo ""
echo "=========================================="
echo "Summary:"
echo "=========================================="
echo "If /dev/tee0 does not exist, OP-TEE driver is not loaded."
echo "If tee-supplicant is not running, start it with:"
echo "  systemctl start tee-supplicant"
echo ""

