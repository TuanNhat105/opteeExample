#!/bin/bash

# Build script for hello_world_test TA and host application
# Uses TA_DEV_KIT_DIR from OP-TEE OS build and optee_client

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"


# TA_DEV_KIT_DIR from OP-TEE OS build
TA_DEV_KIT_DIR="${TA_DEV_KIT_DIR:-/home/orangepi/tee-core/export-ta_arm64}"

# TEEC_EXPORT from optee_client build
TEEC_EXPORT="${TEEC_EXPORT:-/home/orangepi/tee-core/usr}"
# Fix OpenSSL library path for cryptography (use Debian OpenSSL instead of cix-openssl)
export LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH"
# Cross compiler - use aarch64-none-linux-gnu- if available, otherwise aarch64-linux-gnu-
if command -v aarch64-none-linux-gnu-gcc >/dev/null 2>&1; then
    CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
elif command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
else
    echo "Error: No cross-compiler found"
    exit 1
fi

echo -e "${GREEN}=== Building hello_world_test ===${NC}"
echo "TA_DEV_KIT_DIR: $TA_DEV_KIT_DIR"
echo "TEEC_EXPORT: $TEEC_EXPORT"
echo "CROSS_COMPILE: $CROSS_COMPILE"
echo ""

# Check if TA_DEV_KIT_DIR exists
if [ ! -d "$TA_DEV_KIT_DIR" ]; then
    echo -e "${RED}Error: TA_DEV_KIT_DIR not found at: $TA_DEV_KIT_DIR${NC}"
    echo "Please build OP-TEE OS first or set TA_DEV_KIT_DIR environment variable"
    exit 1
fi

# Check if TA_DEV_KIT_DIR/mk/ta_dev_kit.mk exists
if [ ! -f "$TA_DEV_KIT_DIR/mk/ta_dev_kit.mk" ]; then
    echo -e "${RED}Error: ta_dev_kit.mk not found at: $TA_DEV_KIT_DIR/mk/ta_dev_kit.mk${NC}"
    exit 1
fi

# Check if TEEC_EXPORT exists
if [ ! -d "$TEEC_EXPORT" ]; then
    echo -e "${RED}Error: TEEC_EXPORT not found at: $TEEC_EXPORT${NC}"
    echo "Please build optee_client first or set TEEC_EXPORT environment variable"
    exit 1
fi

# Check if key exists
TA_KEY="${TA_DEV_KIT_DIR}/keys/default_ta.pem"
if [ ! -f "$TA_KEY" ]; then
    echo -e "${YELLOW}Warning: TA signing key not found at: $TA_KEY${NC}"
    echo "TA will not be signed properly"
fi

echo -e "${GREEN}Building TA...${NC}"
cd ta
make clean 2>/dev/null || true
make TA_DEV_KIT_DIR="$TA_DEV_KIT_DIR" CROSS_COMPILE="$CROSS_COMPILE"
cd ..

echo -e "${GREEN}Building host application...${NC}"
cd host
make clean 2>/dev/null || true
export CROSS_COMPILE="$CROSS_COMPILE"
export CC="${CROSS_COMPILE}gcc"
export LD="${CROSS_COMPILE}ld"
# Default to static build to avoid library dependency issues on device
# Set STATIC_BUILD=0 to use dynamic linking (requires libteec.so.2 on device)
STATIC_BUILD="${STATIC_BUILD:-1}"
if [ "$STATIC_BUILD" = "1" ]; then
    echo -e "${YELLOW}Using static linking (no library needed on device)${NC}"
else
    echo -e "${YELLOW}Using dynamic linking (requires libteec.so.2 on device)${NC}"
fi
make TEEC_EXPORT="$TEEC_EXPORT" CROSS_COMPILE="$CROSS_COMPILE" CC="${CROSS_COMPILE}gcc" STATIC_BUILD="$STATIC_BUILD"
cd ..

echo ""
echo -e "${GREEN}=== Build completed successfully ===${NC}"
echo "TA binary: ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
echo "Host binary: host/optee_example_hello_world"
if [ "$STATIC_BUILD" = "1" ]; then
    echo -e "${GREEN}(Static build - no library needed on device)${NC}"
else
    echo -e "${YELLOW}(Dynamic build - requires libteec.so.2 on device)${NC}"
fi
echo ""

# Copy files to system directories
TA_FILE="ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
HOST_BINARY="host/optee_example_hello_world"

echo -e "${GREEN}Copying files to system directories...${NC}"

# Check if TA file exists
if [ ! -f "$TA_FILE" ]; then
    echo -e "${RED}Error: TA file not found: $TA_FILE${NC}"
    exit 1
fi

# Check if host binary exists
if [ ! -f "$HOST_BINARY" ]; then
    echo -e "${RED}Error: Host binary not found: $HOST_BINARY${NC}"
    exit 1
fi

# Copy TA to /lib/optee_armtz/
echo -e "${YELLOW}Copying TA to /lib/optee_armtz/...${NC}"
sudo cp "$TA_FILE" /lib/optee_armtz/
sudo chmod 755 /lib/optee_armtz/$(basename "$TA_FILE")
echo -e "${GREEN}✓ TA copied successfully${NC}"

# Copy host binary to /usr/bin/
echo -e "${YELLOW}Copying host binary to /usr/bin/...${NC}"
sudo cp "$HOST_BINARY" /usr/bin/
sudo chmod 755 /usr/bin/$(basename "$HOST_BINARY")
echo -e "${GREEN}✓ Host binary copied successfully${NC}"

echo ""
echo -e "${GREEN}=== Files installed successfully ===${NC}"
echo "TA installed: /lib/optee_armtz/$(basename "$TA_FILE")"
echo "Host binary installed: /usr/bin/$(basename "$HOST_BINARY")"

