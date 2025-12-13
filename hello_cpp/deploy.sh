#!/bin/bash

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="$PROJECT_DIR/deploy"

# Check arguments
if [ $# -lt 1 ]; then
    echo -e "${RED}Usage: $0 <user>@<ip_address>${NC}"
    echo "Example: $0 pi@192.168.1.100"
    exit 1
fi

TARGET=$1

echo "======================================"
echo "  Deploying Hello C++ TA to Pi 5"
echo "======================================"
echo ""
echo "Target: $TARGET"
echo ""

# Check if deploy directory exists
if [ ! -d "$DEPLOY_DIR" ]; then
    echo -e "${RED}Error: Deploy directory not found. Please run ./build.sh first.${NC}"
    exit 1
fi

# Check if files exist
TA_FILE=$(find "$DEPLOY_DIR" -name "*.ta" -type f | head -n 1)
if [ -z "$TA_FILE" ]; then
    echo -e "${RED}Error: No .ta file found in deploy directory.${NC}"
    exit 1
fi

HOST_FILE="$DEPLOY_DIR/hello_cpp_host"
if [ ! -f "$HOST_FILE" ]; then
    echo -e "${RED}Error: Host application not found.${NC}"
    exit 1
fi

echo -e "${YELLOW}[1/3] Copying TA to /lib/optee_armtz/ ...${NC}"
scp -O "$TA_FILE" "$TARGET:/lib/optee_armtz/"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ TA deployed${NC}"
else
    echo -e "${RED}❌ Failed to deploy TA${NC}"
    exit 1
fi

echo ""
echo -e "${YELLOW}[2/3] Copying host application to /usr/bin/ ...${NC}"
scp -O "$HOST_FILE" "$TARGET:/usr/bin/"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Host application deployed${NC}"
else
    echo -e "${RED}❌ Failed to deploy host application${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}======================================"
echo "  Deployment Completed!"
echo "======================================${NC}"
echo ""
echo "To run on Pi 5:"
echo "  ssh $TARGET"
echo "  sudo ./hello_cpp_host"
echo ""
echo "To view TA logs:"
echo "  sudo dmesg | grep -A 20 \"TA_CreateEntryPoint\""
