#!/bin/bash
# Comprehensive script to ensure OP-TEE is ready
# Checks and starts everything needed

echo "=========================================="
echo "OP-TEE Ready Check and Auto-Fix"
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Step 1: Always load OP-TEE modules first
echo "1. Loading OP-TEE kernel modules..."
MODULE_OUTPUT=$(expect << 'EOF' | grep -v "password:"
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "modprobe optee 2>&1; modprobe optee_rpc 2>&1; echo 'MODULES_LOADED'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

if echo "$MODULE_OUTPUT" | grep -q "MODULES_LOADED"; then
    echo -e "${GREEN}✓ Modules loaded${NC}"
    sleep 2  # Wait for devices to be created
else
    echo -e "${YELLOW}⚠ Module loading output:${NC}"
    echo "$MODULE_OUTPUT" | grep -v "password:" | grep -v "MODULES_LOADED"
fi

# Step 2: Check /dev/tee0
echo ""
echo "2. Checking /dev/tee0..."
DEVICE_CHECK=$(expect << 'EOF' | grep -v "password:" | grep -E "^(YES|NO)$" | tail -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -c /dev/tee0 && echo YES || echo NO"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

if [ "$DEVICE_CHECK" = "YES" ]; then
    echo -e "${GREEN}✓ /dev/tee0 exists${NC}"
    DEVICE_OK="YES"
else
    echo -e "${YELLOW}⚠ /dev/tee0 check returned: $DEVICE_CHECK${NC}"
    # Try listing devices directly
    DEVICE_LIST=$(expect << 'EOF' | grep -v "password:" | grep "/dev/tee" | head -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ls -l /dev/tee* 2>&1"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)
    if [ -n "$DEVICE_LIST" ]; then
        echo -e "${GREEN}✓ /dev/tee0 exists (found via ls)${NC}"
        DEVICE_OK="YES"
    else
        echo -e "${RED}✗ /dev/tee0 NOT FOUND${NC}"
        DEVICE_OK="NO"
    fi
fi

# Step 3: Check tee-supplicant (using escaped pattern to avoid expect syntax error)
echo ""
echo "3. Checking tee-supplicant..."
SUPPLICANT_CHECK=$(expect << 'EOF' | grep -v "password:" | grep -E "^(YES|NO|tee-supplicant)" | head -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ps | grep tee-supplicant | grep -v grep > /dev/null && echo YES || echo NO"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

# Also check by looking for process directly
SUPPLICANT_PROCESS=$(expect << 'EOF' | grep -v "password:" | grep "tee-supplicant" | head -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ps | grep tee-supplicant"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

if [ "$SUPPLICANT_CHECK" = "YES" ] || [ -n "$SUPPLICANT_PROCESS" ]; then
    SUPPLICANT_OK="YES"
    echo -e "${GREEN}✓ tee-supplicant is running${NC}"
else
    SUPPLICANT_OK="NO"
    echo -e "${YELLOW}⚠ tee-supplicant is NOT running${NC}"
    echo "   Starting tee-supplicant..."
    expect << 'EOF' > /dev/null 2>&1
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "/usr/sbin/tee-supplicant -d /dev/teepriv0 > /tmp/tee-supplicant.log 2>&1 &"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
    sleep 3
    # Check again
    SUPPLICANT_PROCESS2=$(expect << 'EOF' | grep -v "password:" | grep "tee-supplicant" | head -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ps | grep tee-supplicant"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)
    if [ -n "$SUPPLICANT_PROCESS2" ]; then
        SUPPLICANT_OK="YES"
        echo -e "${GREEN}✓ tee-supplicant started${NC}"
    else
        SUPPLICANT_OK="NO"
        echo -e "${RED}✗ Failed to start tee-supplicant${NC}"
    fi
fi

# Final status
echo ""
echo "=========================================="
if [ "$DEVICE_OK" = "YES" ] && [ "$SUPPLICANT_OK" = "YES" ]; then
    echo -e "${GREEN}✓ OP-TEE is ready!${NC}"
    echo "=========================================="
    echo ""
    echo "You can now run:"
    echo "  ./build.sh"
    echo "  OR"
    echo "  ssh root@192.168.1.114 'test_simple_shm'"
    exit 0
else
    echo -e "${RED}✗ OP-TEE is not ready${NC}"
    echo "=========================================="
    exit 1
fi

