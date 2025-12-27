#!/bin/bash
# Build script for Minimal EVM TA

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Building Minimal EVM TA${NC}"
echo -e "${GREEN}Custom Vector & Map Implementation${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Configuration
export TA_DEV_KIT_DIR="/home/abc/nhat/pi6_build/bsp/tee/out/arm-plat-cix/export-ta_arm64"
export CROSS_COMPILE="aarch64-none-linux-gnu-"
export TEEC_EXPORT="/home/abc/optee_client/out/export/usr"
export PATH="/home/abc/arm-toolchain/bin:$PATH"
echo "Configuration:"
echo "  TA_DEV_KIT_DIR = $TA_DEV_KIT_DIR"
echo "  CROSS_COMPILE  = $CROSS_COMPILE"
echo "  TEEC_EXPORT    = $TEEC_EXPORT"
echo ""

# Build TA
echo -e "${YELLOW}[1/3] Building Trusted Application...${NC}"
cd ta

make clean 2>/dev/null || true

# Suppress harmless warnings about overriding recipe and NULL redefinition
# These are expected and safe to ignore
# Build TA and filter out harmless warnings
# - "overriding recipe": intentional override to fix libstdc++ linking
# - "NULL redefined": harmless conflict between musl and gcc headers
BUILD_OUTPUT=$(mktemp)
if make CROSS_COMPILE="$CROSS_COMPILE" TA_DEV_KIT_DIR="$TA_DEV_KIT_DIR" 2>&1 | tee "$BUILD_OUTPUT" | \
    grep -vE "warning: (overriding recipe|ignoring old recipe|.*NULL.*redefined|MALLOC_INITIAL_POOL_MIN_SIZE.*not defined|unrecognized command-line option)"; then
    true
fi

# Check if make actually succeeded by checking for .ta file
if [ -f *.ta ]; then
    echo -e "${GREEN}✓ TA build successful!${NC}"
    ls -lh *.ta
    rm -f "$BUILD_OUTPUT"
else
    echo -e "${RED}✗ TA build failed${NC}"
    echo "Last 20 lines of build output:"
    tail -20 "$BUILD_OUTPUT"
    rm -f "$BUILD_OUTPUT"
    exit 1
fi

cd ..

# Build Host
echo ""
echo -e "${YELLOW}[2/3] Building Host Application...${NC}"
cd host

make clean 2>/dev/null || true

if make CROSS_COMPILE="$CROSS_COMPILE" TEEC_EXPORT="$TEEC_EXPORT"; then
    echo -e "${GREEN}✓ Host build successful!${NC}"
    ls -lh minimal_evm_host
else
    echo -e "${RED}✗ Host build failed${NC}"
    exit 1
fi

cd ..

# Summary
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Files built:"
ls -lh ta/*.ta host/minimal_evm_host
echo ""
echo "Deploy to Pi 5:"
echo "  scp -O ta/*.ta root@192.168.1.203:/lib/optee_armtz/"
echo "  scp -O host/minimal_evm_host root@192.168.1.203:/usr/bin/"
echo ""
echo "Run on Pi 5:"
echo "  minimal_evm_host"

