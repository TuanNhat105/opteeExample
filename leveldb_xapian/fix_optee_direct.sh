#!/bin/bash
# Direct fix script - runs commands directly on device via SSH
# No file copy needed, avoids SCP issues

echo "=========================================="
echo "OP-TEE Direct Fix (No File Copy)"
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Function to run command on device
run_on_device() {
    expect << EOF
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "$1"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
}

echo -e "${YELLOW}Step 1: Checking /dev/tee0...${NC}"
run_on_device "test -c /dev/tee0 && echo 'EXISTS' || echo 'NOT_FOUND'"

echo ""
echo -e "${YELLOW}Step 2: Attempting to load OP-TEE kernel modules...${NC}"
run_on_device "modprobe optee 2>&1; modprobe optee_rpc 2>&1; echo 'MODULES_LOADED'"

echo ""
echo -e "${YELLOW}Step 3: Checking /dev/tee0 again...${NC}"
run_on_device "test -c /dev/tee0 && ls -l /dev/tee* || echo 'STILL_NOT_FOUND'"

echo ""
echo -e "${YELLOW}Step 4: Finding and starting tee-supplicant...${NC}"

# Find tee-supplicant
SUPPLICANT_PATH=$(expect << 'EOF' | grep -v "password:" | grep -E "^/" | head -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "find /usr /sbin /bin -name tee-supplicant 2>/dev/null | head -1"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

if [ -n "$SUPPLICANT_PATH" ]; then
    echo -e "${GREEN}Found tee-supplicant at: $SUPPLICANT_PATH${NC}"
    echo "Starting tee-supplicant..."
    run_on_device "pgrep -x tee-supplicant > /dev/null || nohup $SUPPLICANT_PATH > /tmp/tee-supplicant.log 2>&1 & sleep 2; pgrep -x tee-supplicant && echo 'STARTED' || echo 'FAILED'"
else
    echo -e "${RED}tee-supplicant not found${NC}"
    echo "Searching in common locations..."
    for path in /usr/sbin/tee-supplicant /usr/bin/tee-supplicant /sbin/tee-supplicant; do
        echo "Trying $path..."
        run_on_device "test -f $path && echo 'FOUND_AT_$path' || echo 'NOT_FOUND'"
    done
fi

echo ""
echo -e "${YELLOW}Step 5: Final status check...${NC}"
echo ""

# Final check
DEVICE_STATUS=$(expect << 'EOF' | grep -v "password:" | tail -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -c /dev/tee0 && echo 'YES' || echo 'NO'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

SUPPLICANT_STATUS=$(expect << 'EOF' | grep -v "password:" | tail -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pgrep -x tee-supplicant > /dev/null && echo 'YES' || echo 'NO'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

echo "=========================================="
if [ "$DEVICE_STATUS" = "YES" ] && [ "$SUPPLICANT_STATUS" = "YES" ]; then
    echo -e "${GREEN}✓ OP-TEE is ready!${NC}"
    echo "=========================================="
    echo ""
    echo "You can now test with:"
    echo "  ssh root@192.168.1.114 'test_simple_shm'"
    exit 0
else
    echo -e "${RED}✗ OP-TEE is still not ready${NC}"
    echo "=========================================="
    echo ""
    echo "Device status: $DEVICE_STATUS"
    echo "Supplicant status: $SUPPLICANT_STATUS"
    echo ""
    echo "Manual steps:"
    echo "  1. SSH to device: ssh root@192.168.1.114"
    echo "  2. Load modules: modprobe optee optee_rpc"
    echo "  3. Find supplicant: find /usr /sbin -name tee-supplicant"
    echo "  4. Start supplicant: /path/to/tee-supplicant &"
    exit 1
fi

