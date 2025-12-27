#!/bin/bash

# Diagnostic script to check OP-TEE setup on device

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Device connection
DEVICE_USER="${DEVICE_USER:-root}"
DEVICE_HOST="${DEVICE_HOST:-192.168.1.234}"

# Allow device IP as argument
if [ -n "$1" ]; then
    DEVICE_HOST="$1"
fi

if [ -z "$DEVICE_HOST" ]; then
    echo -e "${RED}Error: DEVICE_HOST not set${NC}"
    echo "Usage: DEVICE_HOST=<ip> ./check_optee.sh"
    echo "   or: ./check_optee.sh <device_ip>"
    exit 1
fi

echo -e "${GREEN}=== OP-TEE Diagnostic Check ===${NC}"
echo "Device: ${DEVICE_USER}@${DEVICE_HOST}"
echo ""

# Function to run command on device
run_on_device() {
    ssh "${DEVICE_USER}@${DEVICE_HOST}" "$1"
}

# Check 1: OP-TEE kernel modules
echo -e "${YELLOW}[1/6] Checking OP-TEE kernel modules...${NC}"
if run_on_device "lsmod | grep -E 'optee|tee'" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ OP-TEE modules loaded${NC}"
    run_on_device "lsmod | grep -E 'optee|tee'"
else
    echo -e "${RED}✗ OP-TEE modules not loaded${NC}"
    echo "  Trying to load modules..."
    run_on_device "modprobe optee 2>&1 || insmod /lib/modules/\$(uname -r)/kernel/drivers/tee/optee/optee.ko 2>&1" || true
    run_on_device "modprobe optee_rpc 2>&1 || insmod /lib/modules/\$(uname -r)/kernel/drivers/tee/optee/optee_rpc.ko 2>&1" || true
fi
echo ""

# Check 2: OP-TEE device nodes
echo -e "${YELLOW}[2/6] Checking OP-TEE device nodes...${NC}"
if run_on_device "test -c /dev/tee0" 2>/dev/null; then
    echo -e "${GREEN}✓ /dev/tee0 exists${NC}"
    run_on_device "ls -l /dev/tee*"
else
    echo -e "${RED}✗ /dev/tee0 not found${NC}"
    echo "  This usually means OP-TEE driver is not loaded"
fi
echo ""

# Check 3: tee-supplicant process
echo -e "${YELLOW}[3/6] Checking tee-supplicant...${NC}"
if run_on_device "pgrep tee-supplicant" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ tee-supplicant is running${NC}"
    run_on_device "ps aux | grep tee-supplicant | grep -v grep"
else
    echo -e "${RED}✗ tee-supplicant is not running${NC}"
    echo "  Trying to start tee-supplicant..."
    
    # Try to find tee-supplicant
    TEE_SUPPLICANT=$(run_on_device "which tee-supplicant 2>/dev/null || find /usr /sbin /bin -name tee-supplicant 2>/dev/null | head -1" || echo "")
    
    if [ -n "$TEE_SUPPLICANT" ]; then
        echo "  Found at: $TEE_SUPPLICANT"
        run_on_device "$TEE_SUPPLICANT &" || true
        sleep 1
        if run_on_device "pgrep tee-supplicant" > /dev/null 2>&1; then
            echo -e "${GREEN}✓ tee-supplicant started${NC}"
        else
            echo -e "${RED}✗ Failed to start tee-supplicant${NC}"
        fi
    else
        echo -e "${RED}✗ tee-supplicant not found${NC}"
        echo "  Please install optee-client or copy tee-supplicant to device"
    fi
fi
echo ""

# Check 4: TA directory
echo -e "${YELLOW}[4/6] Checking TA directory...${NC}"
if run_on_device "test -d /lib/optee_armtz" 2>/dev/null; then
    echo -e "${GREEN}✓ /lib/optee_armtz exists${NC}"
    run_on_device "ls -lh /lib/optee_armtz/"
else
    echo -e "${YELLOW}⚠ /lib/optee_armtz does not exist, creating...${NC}"
    run_on_device "mkdir -p /lib/optee_armtz"
    echo -e "${GREEN}✓ Created /lib/optee_armtz${NC}"
fi
echo ""

# Check 5: Hello World TA
echo -e "${YELLOW}[5/6] Checking Hello World TA...${NC}"
TA_FILE="/lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
if run_on_device "test -f $TA_FILE" 2>/dev/null; then
    echo -e "${GREEN}✓ TA file exists${NC}"
    run_on_device "ls -lh $TA_FILE"
    run_on_device "file $TA_FILE"
else
    echo -e "${RED}✗ TA file not found: $TA_FILE${NC}"
    echo "  Please copy TA file to device:"
    echo "    scp ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta ${DEVICE_USER}@${DEVICE_HOST}:${TA_FILE}"
fi
echo ""

# Check 6: Test TEEC_InitializeContext
echo -e "${YELLOW}[6/6] Testing TEEC_InitializeContext...${NC}"
if run_on_device "test -c /dev/tee0" 2>/dev/null; then
    # Try to run a simple test if possible
    if run_on_device "which optee_example_hello_world" > /dev/null 2>&1; then
        echo "  Running hello_world test..."
        run_on_device "optee_example_hello_world 2>&1" || {
            EXIT_CODE=$?
            if [ $EXIT_CODE -eq 1 ]; then
                echo -e "${RED}✗ Test failed (exit code: $EXIT_CODE)${NC}"
                echo "  Check error message above"
            fi
        }
    else
        echo -e "${YELLOW}⚠ optee_example_hello_world not found on device${NC}"
        echo "  Copy it first: scp host/optee_example_hello_world ${DEVICE_USER}@${DEVICE_HOST}:/usr/bin/"
    fi
else
    echo -e "${RED}✗ Cannot test - /dev/tee0 not available${NC}"
fi
echo ""

# Summary
echo -e "${GREEN}=== Diagnostic Summary ===${NC}"
echo ""
echo "If all checks passed, try running:"
echo "  ssh ${DEVICE_USER}@${DEVICE_HOST}"
echo "  optee_example_hello_world"
echo ""
echo "If you see TE_ERROR_ITEM_NOT_FOUND (0xFFFF0008):"
echo "  1. Make sure TA is in /lib/optee_armtz/"
echo "  2. Make sure tee-supplicant is running"
echo "  3. Check dmesg for OP-TEE errors: dmesg | grep -i optee"
echo "  4. Check TA permissions: ls -l /lib/optee_armtz/*.ta"

