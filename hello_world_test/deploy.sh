#!/bin/bash

# Deploy script for hello_world_test
# Copies TA, host binary, and required libraries to device

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Default paths
OPTEED_CLIENT_DIR="${OPTEED_CLIENT_DIR:-/home/abc/optee_client}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Device connection (adjust as needed)
DEVICE_USER="${DEVICE_USER:-root}"
DEVICE_HOST="${DEVICE_HOST:-192.168.1.234}"
DEVICE_TA_PATH="/lib/optee_armtz"
DEVICE_BIN_PATH="/usr/bin"
DEVICE_LIB_PATH="/usr/lib"

# Check if device host is provided
if [ -z "$DEVICE_HOST" ]; then
    echo -e "${RED}Error: DEVICE_HOST not set${NC}"
    echo "Usage: DEVICE_HOST=<ip> ./deploy.sh"
    echo "   or: ./deploy.sh <device_ip>"
    exit 1
fi

# Allow device IP as argument
if [ -n "$1" ]; then
    DEVICE_HOST="$1"
fi

echo -e "${GREEN}=== Deploying hello_world_test to device ===${NC}"
echo "Device: ${DEVICE_USER}@${DEVICE_HOST}"
echo ""

# Check if files exist
TA_FILE="${SCRIPT_DIR}/ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
HOST_BINARY="${SCRIPT_DIR}/host/optee_example_hello_world"
LIBTEEC_SO="${OPTEED_CLIENT_DIR}/out/export/usr/lib/libteec.so.2.0.0"

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

if [ ! -f "$LIBTEEC_SO" ]; then
    echo -e "${YELLOW}Warning: libteec.so.2.0.0 not found: $LIBTEEC_SO${NC}"
    echo "Library will not be copied"
    LIBTEEC_SO=""
fi

# Create symlinks for libteec if needed
if [ -f "$LIBTEEC_SO" ]; then
    LIB_DIR="$(dirname "$LIBTEEC_SO")"
    cd "$LIB_DIR"
    if [ ! -f "libteec.so.2" ]; then
        ln -sf libteec.so.2.0.0 libteec.so.2 2>/dev/null || true
    fi
    if [ ! -f "libteec.so" ]; then
        ln -sf libteec.so.2.0.0 libteec.so 2>/dev/null || true
    fi
    cd "$SCRIPT_DIR"
fi

echo -e "${GREEN}Copying TA to device...${NC}"
scp "$TA_FILE" "${DEVICE_USER}@${DEVICE_HOST}:${DEVICE_TA_PATH}/" || {
    echo -e "${YELLOW}Note: If directory doesn't exist, create it first:${NC}"
    echo "  ssh ${DEVICE_USER}@${DEVICE_HOST} mkdir -p ${DEVICE_TA_PATH}"
    exit 1
}

echo -e "${GREEN}Copying host binary to device...${NC}"
scp "$HOST_BINARY" "${DEVICE_USER}@${DEVICE_HOST}:${DEVICE_BIN_PATH}/" || {
    echo -e "${YELLOW}Note: If directory doesn't exist, create it first:${NC}"
    echo "  ssh ${DEVICE_USER}@${DEVICE_HOST} mkdir -p ${DEVICE_BIN_PATH}"
    exit 1
}

if [ -n "$LIBTEEC_SO" ]; then
    echo -e "${GREEN}Copying libteec.so to device...${NC}"
    LIB_DIR="$(dirname "$LIBTEEC_SO")"
    
    # Copy library files
    scp "${LIB_DIR}/libteec.so.2.0.0" "${DEVICE_USER}@${DEVICE_HOST}:${DEVICE_LIB_PATH}/" || {
        echo -e "${YELLOW}Note: If directory doesn't exist, create it first:${NC}"
        echo "  ssh ${DEVICE_USER}@${DEVICE_HOST} mkdir -p ${DEVICE_LIB_PATH}"
        exit 1
    }
    
    # Create symlinks on device
    echo -e "${GREEN}Creating symlinks on device...${NC}"
    ssh "${DEVICE_USER}@${DEVICE_HOST}" "cd ${DEVICE_LIB_PATH} && \
        ln -sf libteec.so.2.0.0 libteec.so.2 2>/dev/null || true && \
        ln -sf libteec.so.2.0.0 libteec.so 2>/dev/null || true && \
        ldconfig" || {
        echo -e "${YELLOW}Warning: Could not create symlinks or run ldconfig${NC}"
        echo "You may need to run manually on device:"
        echo "  cd ${DEVICE_LIB_PATH}"
        echo "  ln -sf libteec.so.2.0.0 libteec.so.2"
        echo "  ln -sf libteec.so.2.0.0 libteec.so"
        echo "  ldconfig"
    }
fi

echo ""
echo -e "${GREEN}=== Deployment completed ===${NC}"
echo "TA: ${DEVICE_TA_PATH}/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
echo "Host: ${DEVICE_BIN_PATH}/optee_example_hello_world"
if [ -n "$LIBTEEC_SO" ]; then
    echo "Library: ${DEVICE_LIB_PATH}/libteec.so.2.0.0"
fi
echo ""
echo "To run on device:"
echo "  ssh ${DEVICE_USER}@${DEVICE_HOST}"
echo "  optee_example_hello_world"

