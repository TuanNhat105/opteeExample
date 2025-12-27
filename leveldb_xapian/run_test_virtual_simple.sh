#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# Simple script to run virtual functions test (assumes TA is already on device)

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

# Build test program
echo -e "${YELLOW}Building test program...${NC}"
cd host
export CROSS_COMPILE="aarch64-none-linux-gnu-"
export TEEC_EXPORT="/home/abc/optee_client/out/export/usr"
export PATH="/home/abc/arm-toolchain/bin:$PATH"
make test_virtual
cd ..

# Check if TA exists on device and has valid size
echo -e "\n${YELLOW}Checking if TA exists on device...${NC}"
TA_CHECK=$(expect << 'EOF' | tail -1
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -f /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta && stat -c%s /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta || echo '0'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

TA_SIZE=$(echo "$TA_CHECK" | grep -E "^[0-9]+$" | head -1 || echo "0")

if [ "$TA_SIZE" = "0" ] || [ -z "$TA_SIZE" ]; then
    echo -e "${RED}ERROR: TA not found or has invalid size (0 bytes) on device!${NC}"
    echo -e "${YELLOW}Please copy TA first using:${NC}"
    echo "  ./copy_ta_to_device.sh"
    echo ""
    echo -e "${YELLOW}Or manually copy TA to device${NC}"
    exit 1
fi

echo -e "${GREEN}✓ TA found on device (size: $TA_SIZE bytes)${NC}"

# Copy test program using base64 in chunks (to avoid "argument list too long")
echo -e "\n${YELLOW}Copying test program to device (in chunks)...${NC}"
# Split base64 into chunks of 10000 bytes
cat host/test_virtual | base64 | split -b 10000 - /tmp/test_virtual_part_

# Copy first part and create file
FIRST_PART=$(cat /tmp/test_virtual_part_aa 2>/dev/null || echo "")
if [ -n "$FIRST_PART" ]; then
    expect << EOF
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "cat > /tmp/test_virtual_b64 << 'REMOTEEOF'
${FIRST_PART}
REMOTEEOF
echo 'Part 1 received'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
fi

# Append remaining parts
for part in /tmp/test_virtual_part_*; do
    if [ "$part" != "/tmp/test_virtual_part_aa" ] && [ -f "$part" ]; then
        PART_CONTENT=$(cat "$part")
        expect << EOF
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "cat >> /tmp/test_virtual_b64 << 'REMOTEEOF'
${PART_CONTENT}
REMOTEEOF
echo 'Part appended'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
    fi
done

# Decode and install
expect << 'EOF'
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "base64 -d /tmp/test_virtual_b64 > /usr/bin/test_virtual && chmod +x /usr/bin/test_virtual && rm /tmp/test_virtual_b64 && echo 'Test program ready'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Cleanup local temp files
rm -f /tmp/test_virtual_part_*

# Check OP-TEE driver
echo -e "\n${YELLOW}Checking OP-TEE driver...${NC}"
OP_TEE_READY=$(ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 root@${DEVICE_IP} "test -c /dev/tee0 && pgrep -x tee-supplicant > /dev/null && echo 'YES' || echo 'NO'" 2>/dev/null || echo "NO")

if [ "$OP_TEE_READY" != "YES" ]; then
    echo -e "${YELLOW}Restarting tee-supplicant...${NC}"
    expect << 'EOF'
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pkill tee-supplicant 2>/dev/null; sleep 1; /usr/sbin/tee-supplicant & 2>/dev/null || /usr/bin/tee-supplicant & 2>/dev/null || systemctl restart tee-supplicant 2>/dev/null; sleep 2; echo 'tee-supplicant restarted'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
fi

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

