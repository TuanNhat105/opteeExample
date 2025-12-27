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

# Fix OpenSSL library path for cryptography (use Debian OpenSSL instead of cix-openssl)
export LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH"
export TA_DEV_KIT_DIR="/home/orangepi/tee-core/export-ta_arm64"
export TEEC_EXPORT="/home/orangepi/tee-core/usr"
CROSS_COMPILE="${CROSS_COMPILE:-}"
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

# Copy files to system directories
echo -e "${GREEN}Copying files to system directories...${NC}"

# Copy TA files to /lib/optee_armtz/
TA_FILES=ta/*.ta
TA_COUNT=0
for TA_FILE in $TA_FILES; do
    if [ -f "$TA_FILE" ]; then
        TA_NAME=$(basename "$TA_FILE")
        echo -e "${YELLOW}Copying TA to /lib/optee_armtz/: $TA_NAME${NC}"
        sudo cp "$TA_FILE" /lib/optee_armtz/
        sudo chmod 755 /lib/optee_armtz/"$TA_NAME"
        echo -e "${GREEN}✓ $TA_NAME copied successfully${NC}"
        TA_COUNT=$((TA_COUNT + 1))
    fi
done

if [ $TA_COUNT -eq 0 ]; then
    echo -e "${RED}Error: No TA files found in ta/ directory${NC}"
    exit 1
fi

# Copy host binary to /usr/bin/
HOST_BINARY="host/minimal_evm_host"
if [ -f "$HOST_BINARY" ]; then
    echo -e "${YELLOW}Copying host binary to /usr/bin/...${NC}"
    sudo cp "$HOST_BINARY" /usr/bin/
    sudo chmod 755 /usr/bin/minimal_evm_host
    echo -e "${GREEN}✓ Host binary copied successfully${NC}"
else
    echo -e "${RED}Error: Host binary not found: $HOST_BINARY${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=== Files installed successfully ===${NC}"
minimal_evm_host


