#!/bin/bash

# Fix script for OP-TEE setup issues

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Device connection
DEVICE_USER="${DEVICE_USER:-orangepi}"
DEVICE_HOST="${DEVICE_HOST:-192.168.1.160}"

# Allow device IP as argument
if [ -n "$1" ]; then
    DEVICE_HOST="$1"
fi

if [ -z "$DEVICE_HOST" ]; then
    echo -e "${RED}Error: DEVICE_HOST not set${NC}"
    echo "Usage: DEVICE_HOST=<ip> ./fix_optee.sh"
    echo "   or: ./fix_optee.sh <device_ip>"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo -e "${GREEN}=== Fixing OP-TEE Setup ===${NC}"
echo "Device: ${DEVICE_USER}@${DEVICE_HOST}"
echo ""

# Function to run command on device
run_on_device() {
    ssh "${DEVICE_USER}@${DEVICE_HOST}" "$1"
}

# Step 1: Load OP-TEE modules
echo -e "${YELLOW}[1/4] Loading OP-TEE kernel modules...${NC}"
run_on_device "modprobe optee 2>&1 || insmod /lib/modules/\$(uname -r)/kernel/drivers/tee/optee/optee.ko 2>&1 || true"
run_on_device "modprobe optee_rpc 2>&1 || insmod /lib/modules/\$(uname -r)/kernel/drivers/tee/optee/optee_rpc.ko 2>&1 || true"
sleep 1
if run_on_device "test -c /dev/tee0" 2>/dev/null; then
    echo -e "${GREEN}✓ OP-TEE driver loaded${NC}"
else
    echo -e "${RED}✗ Failed to load OP-TEE driver${NC}"
    echo "  Check if OP-TEE is built into kernel or modules are available"
fi
echo ""

# Step 2: Start tee-supplicant
echo -e "${YELLOW}[2/4] Starting tee-supplicant...${NC}"
if run_on_device "pgrep tee-supplicant" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ tee-supplicant already running${NC}"
else
    TEE_SUPPLICANT=$(run_on_device "which tee-supplicant 2>/dev/null || find /usr /sbin /bin -name tee-supplicant 2>/dev/null | head -1" || echo "")
    
    if [ -n "$TEE_SUPPLICANT" ]; then
        echo "  Starting: $TEE_SUPPLICANT"
        run_on_device "nohup $TEE_SUPPLICANT > /tmp/tee-supplicant.log 2>&1 &" || true
        sleep 2
        if run_on_device "pgrep tee-supplicant" > /dev/null 2>&1; then
            echo -e "${GREEN}✓ tee-supplicant started${NC}"
        else
            echo -e "${RED}✗ Failed to start tee-supplicant${NC}"
            echo "  Check logs: ssh ${DEVICE_USER}@${DEVICE_HOST} cat /tmp/tee-supplicant.log"
        fi
    else
        echo -e "${RED}✗ tee-supplicant not found${NC}"
        echo "  Please install optee-client on device"
    fi
fi
echo ""

# Step 3: Create TA directory and copy TA
echo -e "${YELLOW}[3/4] Setting up TA directory...${NC}"
TA_FILE="${SCRIPT_DIR}/ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
if [ -f "$TA_FILE" ]; then
    echo "  Copying TA to device (via /tmp with sudo)..."
    # Copy to /tmp first (user has write permission)
    scp "$TA_FILE" "${DEVICE_USER}@${DEVICE_HOST}:/tmp/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta" || {
        echo -e "${RED}✗ Failed to copy TA to /tmp${NC}"
        exit 1
    }
    # Use sudo to move to system directory
    run_on_device "sudo mkdir -p /lib/optee_armtz && \
                   sudo cp /tmp/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta /lib/optee_armtz/ && \
                   sudo chmod 644 /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta && \
                   rm /tmp/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta" || {
        echo -e "${RED}✗ Failed to move TA to /lib/optee_armtz/ (need sudo)${NC}"
        exit 1
    }
    echo -e "${GREEN}✓ TA copied${NC}"
else
    echo -e "${YELLOW}⚠ TA file not found locally: $TA_FILE${NC}"
    echo "  Please build first: ./build.sh"
fi
echo ""

# Step 4: Copy host binary
echo -e "${YELLOW}[4/4] Setting up host binary...${NC}"
HOST_BINARY="${SCRIPT_DIR}/host/optee_example_hello_world"
if [ -f "$HOST_BINARY" ]; then
    echo "  Copying host binary to device (via /tmp with sudo)..."
    # Copy to /tmp first (user has write permission)
    scp "$HOST_BINARY" "${DEVICE_USER}@${DEVICE_HOST}:/tmp/optee_example_hello_world" || {
        echo -e "${RED}✗ Failed to copy host binary to /tmp${NC}"
        exit 1
    }
    # Use sudo to move to system directory
    run_on_device "sudo cp /tmp/optee_example_hello_world /usr/bin/ && \
                   sudo chmod +x /usr/bin/optee_example_hello_world && \
                   rm /tmp/optee_example_hello_world" || {
        echo -e "${RED}✗ Failed to move host binary to /usr/bin/ (need sudo)${NC}"
        exit 1
    }
    echo -e "${GREEN}✓ Host binary copied${NC}"
else
    echo -e "${YELLOW}⚠ Host binary not found locally: $HOST_BINARY${NC}"
    echo "  Please build first: ./build.sh"
fi
echo ""

# Final check
echo -e "${GREEN}=== Setup Complete ===${NC}"
echo ""
echo "Verification:"
run_on_device "ls -l /dev/tee* 2>/dev/null || echo 'No /dev/tee* found'"
run_on_device "pgrep tee-supplicant && echo 'tee-supplicant: running' || echo 'tee-supplicant: not running'"
run_on_device "ls -lh /lib/optee_armtz/*.ta 2>/dev/null || echo 'No TA files found'"
echo ""
echo "To test:"
echo "  ssh ${DEVICE_USER}@${DEVICE_HOST}"
echo "  optee_example_hello_world"
echo ""
echo "If you still see errors, run diagnostic:"
echo "  ./check_optee.sh ${DEVICE_HOST}"

