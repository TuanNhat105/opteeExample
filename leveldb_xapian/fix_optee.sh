#!/bin/bash
# Script to fix OP-TEE driver issues on device

echo "=========================================="
echo "Fixing OP-TEE Driver on Device"
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Step 1: Checking current status...${NC}"
echo ""

# Check if /dev/tee0 exists
if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -c /dev/tee0" 2>/dev/null; then
    echo -e "${GREEN}✓ /dev/tee0 already exists${NC}"
    DEVICE_EXISTS=true
else
    echo -e "${RED}✗ /dev/tee0 NOT FOUND${NC}"
    DEVICE_EXISTS=false
fi

# Check tee-supplicant
if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pgrep -x tee-supplicant > /dev/null" 2>/dev/null; then
    echo -e "${GREEN}✓ tee-supplicant is running${NC}"
    SUPPLICANT_RUNNING=true
else
    echo -e "${RED}✗ tee-supplicant is NOT running${NC}"
    SUPPLICANT_RUNNING=false
fi

echo ""
echo -e "${YELLOW}Step 2: Running fix commands directly on device...${NC}"
echo "(Using inline commands to avoid SCP issues)"
echo ""

# Try to load OP-TEE kernel modules first
echo "Attempting to load OP-TEE kernel modules..."
MODPROBE_RESULT=$(ssh -o StrictHostKeyChecking=no root@192.168.1.114 "modprobe optee 2>&1; modprobe optee_rpc 2>&1" 2>/dev/null)
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ OP-TEE kernel modules loaded${NC}"
    sleep 1
    # Check if /dev/tee0 appeared
    if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -c /dev/tee0" 2>/dev/null; then
        echo -e "${GREEN}✓ /dev/tee0 now exists${NC}"
        DEVICE_EXISTS=true
    fi
else
    echo -e "${YELLOW}⚠ Could not load modules (may be built into kernel)${NC}"
    echo "   Checking if OP-TEE is built into kernel..."
    KERNEL_CONFIG=$(ssh -o StrictHostKeyChecking=no root@192.168.1.114 "grep -i optee /proc/config.gz 2>/dev/null | gunzip 2>/dev/null || grep -i optee /boot/config* 2>/dev/null | head -1" 2>/dev/null)
    if [ -n "$KERNEL_CONFIG" ]; then
        echo "   Found OP-TEE in kernel config"
    fi
fi

# Try to start tee-supplicant
if [ "$SUPPLICANT_RUNNING" = false ]; then
    echo "Attempting to start tee-supplicant..."
    # Try multiple methods
    SUPPLICANT_STARTED=false
    
    # Method 1: systemctl (if available)
    if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "command -v systemctl > /dev/null 2>&1" 2>/dev/null; then
        ssh -o StrictHostKeyChecking=no root@192.168.1.114 "systemctl start tee-supplicant" 2>/dev/null
        sleep 2
        if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pgrep -x tee-supplicant > /dev/null" 2>/dev/null; then
            echo -e "${GREEN}✓ tee-supplicant started via systemctl${NC}"
            SUPPLICANT_STARTED=true
        fi
    fi
    
    # Method 2: Direct execution (if systemctl not available)
    if [ "$SUPPLICANT_STARTED" = false ]; then
        # Try common paths
        for SUPPLICANT_PATH in /usr/sbin/tee-supplicant /usr/bin/tee-supplicant /sbin/tee-supplicant; do
            if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -f $SUPPLICANT_PATH" 2>/dev/null; then
                echo "   Found tee-supplicant at $SUPPLICANT_PATH"
                ssh -o StrictHostKeyChecking=no root@192.168.1.114 "nohup $SUPPLICANT_PATH > /dev/null 2>&1 &" 2>/dev/null
                sleep 2
                if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pgrep -x tee-supplicant > /dev/null" 2>/dev/null; then
                    echo -e "${GREEN}✓ tee-supplicant started directly${NC}"
                    SUPPLICANT_STARTED=true
                    break
                fi
            fi
        done
    fi
    
    if [ "$SUPPLICANT_STARTED" = false ]; then
        echo -e "${RED}✗ Failed to start tee-supplicant${NC}"
        echo "   tee-supplicant binary not found in common paths"
        echo "   You may need to:"
        echo "   1. Check if OP-TEE client is installed"
        echo "   2. Find tee-supplicant: find /usr /sbin -name tee-supplicant"
        echo "   3. Start manually: /path/to/tee-supplicant &"
    fi
fi

# Check for OP-TEE kernel modules (after attempting to load)
echo ""
echo "Checking OP-TEE kernel modules..."
KERNEL_MODULES=$(ssh -o StrictHostKeyChecking=no root@192.168.1.114 "lsmod | grep -i optee" 2>/dev/null)
if [ -z "$KERNEL_MODULES" ]; then
    # Check if OP-TEE is built into kernel by checking /dev/tee0
    if ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -c /dev/tee0" 2>/dev/null; then
        echo -e "${GREEN}✓ OP-TEE appears to be built into kernel (/dev/tee0 exists)${NC}"
    else
        echo -e "${YELLOW}⚠ OP-TEE kernel modules not loaded and /dev/tee0 not found${NC}"
        echo "   This usually means:"
        echo "   - OP-TEE is not built into the kernel"
        echo "   - Or OP-TEE kernel modules need to be loaded manually"
        echo ""
        echo "   Try loading modules manually:"
        echo "   ssh root@192.168.1.114 'modprobe optee'"
        echo "   ssh root@192.168.1.114 'modprobe optee_rpc'"
    fi
else
    echo -e "${GREEN}✓ OP-TEE kernel modules found:${NC}"
    echo "$KERNEL_MODULES" | sed 's/^/   /'
fi

# Run all fix commands in one SSH session
FIX_COMMANDS=$(cat << 'ENDOFCOMMANDS'
echo "=== Loading OP-TEE kernel modules ==="
modprobe optee 2>&1
modprobe optee_rpc 2>&1
sleep 1
echo ""
echo "=== Checking /dev/tee0 ==="
if [ -c /dev/tee0 ]; then
    echo "✓ /dev/tee0 exists"
    ls -l /dev/tee*
else
    echo "✗ /dev/tee0 NOT FOUND"
fi
echo ""
echo "=== Finding and starting tee-supplicant ==="
SUPPLICANT=""
for path in /usr/sbin/tee-supplicant /usr/bin/tee-supplicant /sbin/tee-supplicant /bin/tee-supplicant; do
    if [ -f "$path" ]; then
        SUPPLICANT="$path"
        echo "Found at: $path"
        break
    fi
done
if [ -n "$SUPPLICANT" ]; then
    if ! pgrep -x tee-supplicant > /dev/null; then
        nohup "$SUPPLICANT" > /tmp/tee-supplicant.log 2>&1 &
        sleep 2
        if pgrep -x tee-supplicant > /dev/null; then
            echo "✓ tee-supplicant started (PID: $(pgrep -x tee-supplicant))"
        else
            echo "✗ Failed to start"
        fi
    else
        echo "✓ tee-supplicant already running"
    fi
else
    echo "✗ tee-supplicant not found"
fi
ENDOFCOMMANDS
)

expect << EOF
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "bash -c '$FIX_COMMANDS'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

echo ""
echo -e "${YELLOW}Step 3: Final status check...${NC}"
echo ""

# Final check
FINAL_DEVICE=$(ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -c /dev/tee0 && echo 'YES' || echo 'NO'" 2>/dev/null)
FINAL_SUPPLICANT=$(ssh -o StrictHostKeyChecking=no root@192.168.1.114 "pgrep -x tee-supplicant > /dev/null && echo 'YES' || echo 'NO'" 2>/dev/null)

if [ "$FINAL_DEVICE" = "YES" ] && [ "$FINAL_SUPPLICANT" = "YES" ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✓ OP-TEE is ready!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "You can now run:"
    echo "  ssh root@192.168.1.114 'test_simple_shm'"
    exit 0
else
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}✗ OP-TEE is still not ready${NC}"
    echo -e "${RED}========================================${NC}"
    echo ""
    echo "Manual steps to fix:"
    echo ""
    if [ "$FINAL_DEVICE" = "NO" ]; then
        echo "1. Load OP-TEE kernel modules:"
        echo "   ssh root@192.168.1.114 'modprobe optee optee_rpc'"
        echo ""
    fi
    if [ "$FINAL_SUPPLICANT" = "NO" ]; then
        echo "2. Start tee-supplicant:"
        echo "   ssh root@192.168.1.114 'systemctl start tee-supplicant'"
        echo "   OR"
        echo "   ssh root@192.168.1.114 '/usr/sbin/tee-supplicant &'"
        echo ""
    fi
    echo "3. Verify:"
    echo "   ssh root@192.168.1.114 'ls -l /dev/tee*'"
    echo "   ssh root@192.168.1.114 'pgrep tee-supplicant'"
    exit 1
fi

