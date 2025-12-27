#!/bin/bash

# Deploy script that uses sudo on device for non-root users

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

if [ -n "$2" ]; then
    DEVICE_USER="$2"
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${GREEN}=== Deploying hello_world_test to device ===${NC}"
echo "Device: ${DEVICE_USER}@${DEVICE_HOST}"
echo ""

# Check if files exist
TA_FILE="${SCRIPT_DIR}/ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
HOST_BINARY="${SCRIPT_DIR}/host/optee_example_hello_world"

if [ ! -f "$TA_FILE" ]; then
    echo -e "${RED}Error: TA file not found: $TA_FILE${NC}"
    echo "Please build first: ./build.sh"
    exit 1
fi

if [ ! -f "$HOST_BINARY" ]; then
    echo -e "${RED}Error: Host binary not found: $HOST_BINARY${NC}"
    echo "Please build first: ./build.sh"
    exit 1
fi

# Method 1: Copy to /tmp first, then use sudo to move
echo -e "${GREEN}Copying files to /tmp on device...${NC}"
scp -O "$TA_FILE" "${DEVICE_USER}@${DEVICE_HOST}:/tmp/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta" || {
    echo -e "${RED}Failed to copy TA${NC}"
    exit 1
}

scp -O "$HOST_BINARY" "${DEVICE_USER}@${DEVICE_HOST}:/tmp/optee_example_hello_world" || {
    echo -e "${RED}Failed to copy host binary${NC}"
    exit 1
}

echo -e "${GREEN}Moving files to system directories (requires sudo)...${NC}"

# Move TA to /lib/optee_armtz/
ssh -o StrictHostKeyChecking=no "${DEVICE_USER}@${DEVICE_HOST}" \
    "sudo mkdir -p /lib/optee_armtz && \
     sudo cp /tmp/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta /lib/optee_armtz/ && \
     sudo chmod 644 /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta && \
     rm /tmp/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta" || {
    echo -e "${RED}Failed to move TA to /lib/optee_armtz/${NC}"
    exit 1
}

# Move host binary to /usr/bin/
ssh -o StrictHostKeyChecking=no "${DEVICE_USER}@${DEVICE_HOST}" \
    "sudo cp /tmp/optee_example_hello_world /usr/bin/ && \
     sudo chmod +x /usr/bin/optee_example_hello_world && \
     rm /tmp/optee_example_hello_world" || {
    echo -e "${RED}Failed to move host binary to /usr/bin/${NC}"
    exit 1
}

echo ""
echo -e "${GREEN}=== Deployment completed successfully ===${NC}"
echo "TA: /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
echo "Host: /usr/bin/optee_example_hello_world"
echo ""
echo "To run on device:"
echo "  ssh ${DEVICE_USER}@${DEVICE_HOST}"
echo "  optee_example_hello_world"

