#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# Build script for eEVM OP-TEE application

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color
export TA_DEV_KIT_DIR="/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64"
export CROSS_COMPILE="aarch64-none-linux-gnu-"
export TEEC_EXPORT="/home/abc/optee_client/out/export/usr"
export PATH="/home/abc/arm-toolchain/bin:$PATH"
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}eEVM OP-TEE Build Script${NC}"
echo -e "${GREEN}========================================${NC}"

# Check required environment variables
if [ -z "$TA_DEV_KIT_DIR" ]; then
    echo -e "${RED}ERROR: TA_DEV_KIT_DIR is not set${NC}"
    echo "Please set TA_DEV_KIT_DIR to point to your OP-TEE TA dev kit"
    echo "Example: export TA_DEV_KIT_DIR=/path/to/optee_os/out/arm/export-ta_arm64"
    exit 1
fi

if [ -z "$TEEC_EXPORT" ]; then
    echo -e "${RED}ERROR: TEEC_EXPORT is not set${NC}"
    echo "Please set TEEC_EXPORT to point to your OP-TEE client export"
    echo "Example: export TEEC_EXPORT=/path/to/optee_client/out/export"
    exit 1
fi

if [ -z "$CROSS_COMPILE" ]; then
    echo -e "${YELLOW}WARNING: CROSS_COMPILE is not set, using native compiler${NC}"
fi

# Check if required libraries exist
EEVM_MINIMAL_TA="../eevm_minimal_ta"

echo -e "\n${YELLOW}Checking required libraries...${NC}"

if [ ! -f "$EEVM_MINIMAL_TA/build_oe_libs/libcxx/libc++.a" ]; then
    echo -e "${RED}ERROR: libcxx not found at $EEVM_MINIMAL_TA/build_oe_libs/libcxx/libc++.a${NC}"
    echo "Please build libcxx first using build_openenclave_libs.sh in eevm_minimal_ta"
    exit 1
fi

if [ ! -f "$EEVM_MINIMAL_TA/libc/liboelibc.a" ]; then
    echo -e "${RED}ERROR: liboelibc not found at $EEVM_MINIMAL_TA/libc/liboelibc.a${NC}"
    echo "Please build libc first using build_libc.sh in eevm_minimal_ta"
    exit 1
fi

if [ ! -f "$EEVM_MINIMAL_TA/build_oe_libs/libcxxrt/libcxxrt.a" ]; then
    echo -e "${RED}ERROR: libcxxrt not found${NC}"
    echo "Please build libcxxrt first using build_libcxxrt_complete.sh in eevm_minimal_ta"
    exit 1
fi

if [ ! -f "$EEVM_MINIMAL_TA/build_libunwind/libunwind.a" ]; then
    echo -e "${RED}ERROR: libunwind not found${NC}"
    echo "Please build libunwind first using build_libunwind_openenclave.sh in eevm_minimal_ta"
    exit 1
fi

echo -e "${GREEN}All required libraries found!${NC}"

# Build TA with DEBUG flags enabled for testing
echo -e "\n${YELLOW}Building Trusted Application (TA) with DEBUG enabled...${NC}"
echo -e "${YELLOW}Debug flags: DEBUG_ENABLED=1, DEBUG_PARAMS=1, DEBUG_STEPS=1, DEBUG_TEST_CODE=1${NC}"
cd ta
make clean
# Build with all debug flags enabled for testing
make DEBUG_ENABLED=1 DEBUG_PARAMS=1 DEBUG_STEPS=1 DEBUG_TEST_CODE=1 DEBUG_PERF=1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}TA built successfully with DEBUG enabled!${NC}"
    ls -lh *.ta
else
    echo -e "${RED}TA build failed!${NC}"
    exit 1
fi
cd ..

# Build Host application
echo -e "\n${YELLOW}Building Host Application...${NC}"
cd host
make clean
make -j$(nproc)
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Host application built successfully!${NC}"
    echo -e "${GREEN}Binaries:" 
    echo -e "  - leveldb_host (main application)"
    echo -e "  - test_simple_shm (simple shared memory test - NO LevelDB)${NC}"
    ls -lh leveldb_host test_simple_shm 2>/dev/null || true
else
    echo -e "${RED}Host build failed!${NC}"
    exit 1
fi
cd ..
# scp -O ta/*.ta root@192.168.1.74:/lib/optee_armtz/
# scp -O host/leveldb_host root@192.168.1.74:/usr/bin/

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "TA binary: ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"
echo "Host binaries:"
# echo "  - host/leveldb_host (main application)"
echo "  - host/test_simple_shm (simple shared memory test - NO LevelDB)"
echo ""
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Testing Options${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""
echo "To deploy and run tests:"
echo "1. Copy the TA to your device: /lib/optee_armtz/"
echo -e "${YELLOW}Attempting to copy TA...${NC}"
# Use specific TA file name (wildcard doesn't work in expect)
TA_FILE="ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"
if [ ! -f "$TA_FILE" ]; then
    echo -e "${RED}ERROR: TA file not found: $TA_FILE${NC}"
    exit 1
fi

# Try scp first
expect << 'EOF'
set timeout 30
spawn scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta root@192.168.1.114:/lib/optee_armtz/
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Verify TA was copied (wait a bit for file to be written)
sleep 2
TA_SIZE=$(expect << 'EOF' | tail -1 | grep -E "^[0-9]+$" || echo "0"
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ls -lh /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta 2>/dev/null | awk '{print \$5}' && wc -c /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta 2>/dev/null | awk '{print \$1}'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

# Extract just the numeric size
TA_SIZE=$(echo "$TA_SIZE" | grep -E "^[0-9]+$" | head -1 || echo "0")

if [ "$TA_SIZE" = "0" ] || [ -z "$TA_SIZE" ]; then
    echo -e "${YELLOW}scp failed, trying HTTP method...${NC}"
    ./copy_ta_via_http.sh 2>/dev/null || echo -e "${RED}Failed to copy TA. Please copy manually.${NC}"
    # Check again after HTTP copy
    sleep 2
    TA_SIZE=$(expect << 'EOF' | tail -1 | grep -E "^[0-9]+$" || echo "0"
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "wc -c /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta 2>/dev/null | awk '{print \$1}'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)
    TA_SIZE=$(echo "$TA_SIZE" | grep -E "^[0-9]+$" | head -1 || echo "0")
fi

if [ "$TA_SIZE" != "0" ] && [ -n "$TA_SIZE" ] && [ "$TA_SIZE" -gt 1000000 ]; then
    echo -e "${GREEN}✓ TA copied successfully (size: $TA_SIZE bytes)${NC}"
else
    echo -e "${RED}WARNING: TA copy may have failed (size: $TA_SIZE bytes)${NC}"
    echo -e "${YELLOW}Please copy TA manually:${NC}"
    echo "  File: $TA_FILE"
    echo "  Destination: /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"
fi
echo ""
echo "2. Choose a test:"
echo "   a) Simple Shared Memory Test (NO LevelDB):"
echo "      scp -O host/test_simple_shm root@192.168.1.114:/usr/bin/"
echo "      ssh root@192.168.1.114 \"test_simple_shm\""
echo ""
echo "   b) Full LevelDB Test:"
echo "      scp -O host/leveldb_host root@192.168.1.114:/usr/bin/"
echo "      ssh root@192.168.1.114 \"leveldb_host\""
echo ""
echo -e "${YELLOW}Checking OP-TEE driver before running test...${NC}"
# Quick check if OP-TEE is ready
OP_TEE_READY=$(ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 root@192.168.1.114 "test -c /dev/tee0 && pgrep -x tee-supplicant > /dev/null && echo 'YES' || echo 'NO'" 2>/dev/null || echo "NO")

if [ "$OP_TEE_READY" != "YES" ]; then
    echo -e "${RED}⚠ WARNING: OP-TEE driver may not be ready on device${NC}"
    echo -e "${YELLOW}Attempting to fix OP-TEE...${NC}"
    # Try to load modules and start tee-supplicant
    expect << 'EOF'
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "modprobe optee 2>/dev/null; modprobe optee_rpc 2>/dev/null; sleep 1; /usr/sbin/tee-supplicant & 2>/dev/null || /usr/bin/tee-supplicant & 2>/dev/null || systemctl restart tee-supplicant 2>/dev/null; sleep 2; echo 'OP-TEE fix attempted'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
    echo ""
fi

# Check TA before running test (simpler check - just verify file exists and has reasonable size)
TA_EXISTS=$(expect << 'EOF' | grep -qE "1\.[0-9]+M|M" && echo "YES" || echo "NO"
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ls -lh /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta 2>/dev/null | awk '{print \$5}'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

if [ "$TA_EXISTS" != "YES" ]; then
    echo -e "${RED}ERROR: TA not found or has invalid size on device!${NC}"
    echo -e "${YELLOW}Please copy TA manually. File: ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta${NC}"
    echo -e "${YELLOW}Destination: /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta${NC}"
    exit 1
fi

echo -e "${GREEN}✓ TA verified on device${NC}"

echo -e "${GREEN}Running Simple Shared Memory Test (recommended first)...${NC}"
# Copy test program using HTTP server (same reliable method as TA)
echo -e "${YELLOW}Copying test program via HTTP...${NC}"
BUILD_IP=$(ip route get 8.8.8.8 2>/dev/null | awk '{print $7; exit}' || hostname -I | awk '{print $1}')
PORT=8002
while lsof -Pi :${PORT} -sTCP:LISTEN -t >/dev/null 2>&1 ; do
    PORT=$((PORT + 1))
    if [ $PORT -gt 8100 ]; then
        echo -e "${RED}ERROR: Could not find available port${NC}"
        exit 1
    fi
done

# Start HTTP server
cd "$(dirname "$0")"
python3 -m http.server ${PORT} --bind 0.0.0.0 > /tmp/http_test.log 2>&1 &
HTTP_PID=$!
sleep 2

# Download test program
expect << EOF
set timeout 120
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "wget http://${BUILD_IP}:${PORT}/host/test_simple_shm -O /usr/bin/test_simple_shm && chmod +x /usr/bin/test_simple_shm && ls -lh /usr/bin/test_simple_shm && echo 'Test program downloaded'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Stop HTTP server
kill $HTTP_PID 2>/dev/null || true
wait $HTTP_PID 2>/dev/null || true

# Restart tee-supplicant to reload TA before running test
echo -e "${YELLOW}Restarting tee-supplicant to reload TA...${NC}"
expect << 'EOF'
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pkill tee-supplicant 2>/dev/null; sleep 2; /usr/sbin/tee-supplicant & 2>/dev/null || /usr/bin/tee-supplicant & 2>/dev/null || systemctl restart tee-supplicant 2>/dev/null; sleep 3; echo 'tee-supplicant restarted'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Check if test program exists and is valid
TEST_SIZE=$(expect << 'EOF' | tail -1 | grep -E "^[0-9]+$" || echo "0"
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "wc -c /usr/bin/test_simple_shm 2>/dev/null | awk '{print \$1}' || echo '0'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)
TEST_SIZE=$(echo "$TEST_SIZE" | grep -E "^[0-9]+$" | head -1 || echo "0")

if [ "$TEST_SIZE" = "0" ] || [ -z "$TEST_SIZE" ] || [ "$TEST_SIZE" -lt 300000 ]; then
    echo -e "${RED}ERROR: Test program not found or corrupted (size: $TEST_SIZE bytes)${NC}"
    echo -e "${YELLOW}Skipping test. Please copy test program manually.${NC}"
    exit 1
fi

expect << 'EOF'
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test_simple_shm"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
echo ""
echo -e "${YELLOW}Test completed! Check output above for:${NC}"
echo "  - [DEBUG] Parameter types and values"
echo "  - [STEP] Step-by-step execution"
echo "  - [TEST] Memory access tests"
echo "  - [PERF] Performance timing"
echo "  - [INFO] Operation results"
echo ""
