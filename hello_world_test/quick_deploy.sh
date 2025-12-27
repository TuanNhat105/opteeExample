#!/bin/bash

# Quick deploy script for orangepi user
# Copies to /tmp first, then uses sudo to move to system directories

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

DEVICE_USER="${DEVICE_USER:-orangepi}"
DEVICE_HOST="${DEVICE_HOST:-192.168.1.160}"

if [ -n "$1" ]; then
    DEVICE_HOST="$1"
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo -e "${GREEN}=== Quick Deploy for Orange Pi 6 Plus ===${NC}"
echo "Device: ${DEVICE_USER}@${DEVICE_HOST}"
echo ""

# Check files
if [ ! -f "ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta" ]; then
    echo -e "${RED}Error: TA not found. Run ./build.sh first${NC}"
    exit 1
fi

if [ ! -f "host/optee_example_hello_world" ]; then
    echo -e "${RED}Error: Host binary not found. Run ./build.sh first${NC}"
    exit 1
fi

# Step 1: Copy to /tmp
echo -e "${YELLOW}[1/3] Copying files to /tmp...${NC}"
scp -O ta/*.ta "${DEVICE_USER}@${DEVICE_HOST}:/tmp/" || {
    echo -e "${RED}Failed to copy TA${NC}"
    exit 1
}
scp -O host/optee_example_hello_world "${DEVICE_USER}@${DEVICE_HOST}:/tmp/" || {
    echo -e "${RED}Failed to copy host binary${NC}"
    exit 1
}
echo -e "${GREEN}✓ Files copied to /tmp${NC}"

# Step 2: Move files with sudo
echo -e "${YELLOW}[2/3] Moving files to system directories (requires sudo)...${NC}"
ssh "${DEVICE_USER}@${DEVICE_HOST}" 'bash -s' << 'EOF'
set -e

# Create directories
sudo mkdir -p /lib/optee_armtz

# Move TA
sudo cp /tmp/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta /lib/optee_armtz/
sudo chmod 644 /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta

# Move host binary
sudo cp /tmp/optee_example_hello_world /usr/bin/
sudo chmod +x /usr/bin/optee_example_hello_world

# Cleanup
rm /tmp/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta
rm /tmp/optee_example_hello_world

echo "Files moved successfully"
EOF

echo -e "${GREEN}✓ Files deployed${NC}"

# Step 3: Verify setup
echo -e "${YELLOW}[3/3] Verifying OP-TEE setup...${NC}"
ssh "${DEVICE_USER}@${DEVICE_HOST}" 'bash -s' << 'EOF'
echo ""
echo "=== OP-TEE Status ==="

# Check /dev/tee*
if [ -c /dev/tee0 ]; then
    echo "✓ /dev/tee0 exists"
    ls -l /dev/tee*
else
    echo "✗ /dev/tee0 not found"
    echo "  Run: sudo modprobe optee"
fi

# Check permissions
echo ""
echo "Device permissions:"
ls -l /dev/tee* 2>/dev/null || echo "No /dev/tee* found"

# Check if user can access
echo ""
if [ -r /dev/tee0 ] && [ -w /dev/tee0 ]; then
    echo "✓ Current user can read/write /dev/tee0"
else
    echo "✗ Current user cannot access /dev/tee0"
    echo "  Solution: sudo chmod 666 /dev/tee0"
    echo "  Or add user to tee group: sudo usermod -aG tee $USER"
fi

# Check tee-supplicant
echo ""
if pgrep tee-supplicant > /dev/null; then
    echo "✓ tee-supplicant is running"
    ps aux | grep tee-supplicant | grep -v grep
else
    echo "✗ tee-supplicant not running"
    echo "  Start with: sudo tee-supplicant &"
fi

# Check TA
echo ""
if [ -f /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta ]; then
    echo "✓ TA file exists"
    ls -lh /lib/optee_armtz/*.ta
else
    echo "✗ TA not found"
fi
EOF

echo ""
echo -e "${GREEN}=== Deployment complete ===${NC}"
echo ""
echo "To test, run on device:"
echo "  ssh ${DEVICE_USER}@${DEVICE_HOST}"
echo "  optee_example_hello_world"
echo ""
echo "If you see TEEC_InitializeContext error:"
echo "  1. Check /dev/tee0 permissions: ls -l /dev/tee0"
echo "  2. Add user to tee group: sudo usermod -aG tee orangepi"
echo "  3. Fix permissions: sudo chmod 666 /dev/tee0"
echo "  4. Start tee-supplicant: sudo tee-supplicant &"

