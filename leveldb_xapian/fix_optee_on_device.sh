#!/bin/bash
# Script to run directly on the device to fix OP-TEE
# Usage: Copy this to device and run: bash fix_optee_on_device.sh

echo "=========================================="
echo "OP-TEE Diagnostic and Fix Script"
echo "=========================================="
echo ""

# Colors (if terminal supports)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    NC=''
fi

# Step 1: Check /dev/tee0
echo "1. Checking /dev/tee0..."
if [ -c /dev/tee0 ]; then
    echo -e "${GREEN}✓ /dev/tee0 exists${NC}"
    ls -l /dev/tee*
    DEVICE_OK=true
else
    echo -e "${RED}✗ /dev/tee0 NOT FOUND${NC}"
    DEVICE_OK=false
fi

# Step 2: Check kernel modules
echo ""
echo "2. Checking OP-TEE kernel modules..."
MODULES=$(lsmod | grep -i optee)
if [ -n "$MODULES" ]; then
    echo -e "${GREEN}✓ OP-TEE modules loaded:${NC}"
    echo "$MODULES"
    MODULES_OK=true
else
    echo -e "${YELLOW}⚠ OP-TEE modules not in lsmod${NC}"
    MODULES_OK=false
    echo "   Attempting to load modules..."
    
    # Try to load optee
    if modprobe optee 2>&1; then
        echo -e "${GREEN}✓ Loaded optee module${NC}"
        MODULES_OK=true
    else
        echo -e "${YELLOW}⚠ Could not load optee module${NC}"
        echo "   (May be built into kernel)"
    fi
    
    # Try to load optee_rpc
    if modprobe optee_rpc 2>&1; then
        echo -e "${GREEN}✓ Loaded optee_rpc module${NC}"
    else
        echo -e "${YELLOW}⚠ Could not load optee_rpc module${NC}"
        echo "   (May be built into kernel)"
    fi
    
    # Check again
    sleep 1
    if [ -c /dev/tee0 ]; then
        echo -e "${GREEN}✓ /dev/tee0 now exists after loading modules${NC}"
        DEVICE_OK=true
    fi
fi

# Step 3: Check tee-supplicant
echo ""
echo "3. Checking tee-supplicant..."
if pgrep -x tee-supplicant > /dev/null; then
    echo -e "${GREEN}✓ tee-supplicant is running (PID: $(pgrep -x tee-supplicant))${NC}"
    SUPPLICANT_OK=true
else
    echo -e "${RED}✗ tee-supplicant is NOT running${NC}"
    SUPPLICANT_OK=false
    
    # Try to find and start tee-supplicant
    echo "   Searching for tee-supplicant binary..."
    for path in /usr/sbin/tee-supplicant /usr/bin/tee-supplicant /sbin/tee-supplicant /bin/tee-supplicant; do
        if [ -f "$path" ]; then
            echo -e "${GREEN}   Found at: $path${NC}"
            echo "   Attempting to start..."
            if nohup "$path" > /tmp/tee-supplicant.log 2>&1 & then
                sleep 2
                if pgrep -x tee-supplicant > /dev/null; then
                    echo -e "${GREEN}✓ tee-supplicant started successfully${NC}"
                    SUPPLICANT_OK=true
                    break
                else
                    echo -e "${RED}✗ Failed to start (check /tmp/tee-supplicant.log)${NC}"
                fi
            fi
        fi
    done
    
    if [ "$SUPPLICANT_OK" = false ]; then
        echo -e "${RED}✗ tee-supplicant binary not found${NC}"
        echo "   Try: find /usr /sbin /bin -name tee-supplicant"
    fi
fi

# Step 4: Final status
echo ""
echo "=========================================="
echo "Final Status"
echo "=========================================="

if [ "$DEVICE_OK" = true ] && [ "$SUPPLICANT_OK" = true ]; then
    echo -e "${GREEN}✓ OP-TEE is ready!${NC}"
    echo ""
    echo "You can now test with:"
    echo "  test_simple_shm"
    exit 0
else
    echo -e "${RED}✗ OP-TEE is not ready${NC}"
    echo ""
    if [ "$DEVICE_OK" = false ]; then
        echo "Issues:"
        echo "  - /dev/tee0 does not exist"
        echo "  - OP-TEE driver may not be loaded or built into kernel"
        echo ""
        echo "Possible solutions:"
        echo "  1. Check if OP-TEE is enabled in kernel:"
        echo "     grep -i optee /proc/config.gz 2>/dev/null | gunzip"
        echo "     OR"
        echo "     grep -i optee /boot/config*"
        echo ""
        echo "  2. If using modules, ensure they are available:"
        echo "     ls /lib/modules/\$(uname -r)/kernel/drivers/tee/optee/"
        echo ""
    fi
    if [ "$SUPPLICANT_OK" = false ]; then
        echo "Issues:"
        echo "  - tee-supplicant is not running"
        echo ""
        echo "Possible solutions:"
        echo "  1. Install OP-TEE client:"
        echo "     opkg install optee-client"
        echo "     OR build and install from optee_client repository"
        echo ""
        echo "  2. Start manually:"
        echo "     /path/to/tee-supplicant &"
        echo ""
    fi
    exit 1
fi

