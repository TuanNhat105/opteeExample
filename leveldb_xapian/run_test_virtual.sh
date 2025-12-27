#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# Script to run virtual functions test

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Device IP
DEVICE_IP="${DEVICE_IP:-192.168.1.114}"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Virtual Functions Test${NC}"
echo -e "${GREEN}========================================${NC}\n"

# Check if TA exists, if not build it
if [ ! -f "ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta" ]; then
    echo -e "${YELLOW}TA not found, building...${NC}"
    ./build.sh
fi

# Build test program
echo -e "${YELLOW}Building test program...${NC}"
cd host
export CROSS_COMPILE="aarch64-none-linux-gnu-"
export TEEC_EXPORT="/home/abc/optee_client/out/export/usr"
export PATH="/home/abc/arm-toolchain/bin:$PATH"
make test_virtual
cd ..

# Check OP-TEE driver
echo -e "\n${YELLOW}Checking OP-TEE driver...${NC}"
OP_TEE_READY=$(ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 root@${DEVICE_IP} "test -c /dev/tee0 && pgrep -x tee-supplicant > /dev/null && echo 'YES' || echo 'NO'" 2>/dev/null || echo "NO")

if [ "$OP_TEE_READY" != "YES" ]; then
    echo -e "${RED}⚠ WARNING: OP-TEE driver may not be ready on device${NC}"
    echo -e "${YELLOW}Run './fix_optee.sh' to attempt automatic fix, or manually:${NC}"
    echo "  1. ssh root@${DEVICE_IP} 'modprobe optee optee_rpc'"
    echo "  2. ssh root@${DEVICE_IP} 'systemctl start tee-supplicant'"
    echo ""
    echo -e "${YELLOW}Continuing with test anyway...${NC}"
    echo ""
fi

# Copy TA to device using base64 (since scp doesn't work)
echo -e "${YELLOW}Copying TA to device...${NC}"
expect << EOF
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "cat > /tmp/ta_b64 << 'REMOTEEOF'
$(cat ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta | base64)
REMOTEEOF
base64 -d /tmp/ta_b64 > /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta && rm /tmp/ta_b64 && echo 'TA copied successfully'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Copy test program to device using base64
echo -e "${YELLOW}Copying test program to device...${NC}"
expect << EOF
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "cat > /tmp/test_virtual_b64 << 'REMOTEEOF'
$(cat host/test_virtual | base64)
REMOTEEOF
base64 -d /tmp/test_virtual_b64 > /usr/bin/test_virtual && chmod +x /usr/bin/test_virtual && rm /tmp/test_virtual_b64 && echo 'Test program copied successfully'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Restart tee-supplicant to reload TA
echo -e "${YELLOW}Restarting tee-supplicant to reload TA...${NC}"
expect << 'EOF'
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pkill tee-supplicant; sleep 1; /usr/sbin/tee-supplicant & || /usr/bin/tee-supplicant & || systemctl restart tee-supplicant; sleep 2; echo 'tee-supplicant restarted'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Run test on device
echo -e "\n${GREEN}Running virtual functions test on device...${NC}"
echo "========================================"
expect << 'EOF'
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test_virtual"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

echo -e "\n${GREEN}Test completed!${NC}"

