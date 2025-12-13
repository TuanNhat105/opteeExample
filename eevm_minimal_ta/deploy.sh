#!/bin/bash
# Deploy Minimal EVM TA to Raspberry Pi 5

set -e

PI_IP="${1:-192.168.1.203}"
PI_USER="root"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Deploying to Raspberry Pi 5${NC}"
echo -e "${GREEN}Target: ${PI_USER}@${PI_IP}${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check if files exist
if [ ! -f "ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta" ]; then
    echo -e "${YELLOW}TA not found, building first...${NC}"
    ./build.sh
fi

echo -e "${YELLOW}[1/2] Deploying TA...${NC}"
scp -O ta/*.ta ${PI_USER}@${PI_IP}:/lib/optee_armtz/
echo -e "${GREEN}✓ TA deployed${NC}"

echo ""
echo -e "${YELLOW}[2/2] Deploying Host App...${NC}"
scp -O host/minimal_evm_host ${PI_USER}@${PI_IP}:/usr/bin/
echo -e "${GREEN}✓ Host app deployed${NC}"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Deployment Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Run on Pi 5:"
echo "  ssh ${PI_USER}@${PI_IP}"
echo "  minimal_evm_host"
