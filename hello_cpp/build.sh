#!/bin/bash

set -e

echo "======================================"
echo "  Building Hello C++ TA"
echo "======================================"

# Configuration
OPTEE_OS_DIR="${OPTEE_OS_DIR:-/home/abc/optee_os}"
OPTEE_CLIENT_DIR="${OPTEE_CLIENT_DIR:-/home/abc/optee_client}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Check if OP-TEE directories exist
if [ ! -d "$OPTEE_OS_DIR" ]; then
    echo -e "${RED}Error: OP-TEE OS directory not found: $OPTEE_OS_DIR${NC}"
    exit 1
fi

# Export TA_DEV_KIT_DIR - try multiple paths
if [ -d "$OPTEE_OS_DIR/out/arm-plat-rpi5/export-ta_arm64" ]; then
    export TA_DEV_KIT_DIR="$OPTEE_OS_DIR/out/arm-plat-rpi5/export-ta_arm64"
elif [ -d "$OPTEE_OS_DIR/out/arm/export-ta_arm64" ]; then
    export TA_DEV_KIT_DIR="$OPTEE_OS_DIR/out/arm/export-ta_arm64"
else
    echo -e "${RED}Error: TA dev kit not found. Please build OP-TEE OS first.${NC}"
    echo "Searched paths:"
    echo "  - $OPTEE_OS_DIR/out/arm-plat-rpi5/export-ta_arm64"
    echo "  - $OPTEE_OS_DIR/out/arm/export-ta_arm64"
    exit 1
fi

# Export TEEC
export TEEC_EXPORT="$OPTEE_CLIENT_DIR/out/export/usr"

if [ ! -d "$TEEC_EXPORT" ]; then
    echo -e "${YELLOW}Warning: TEEC export not found at $TEEC_EXPORT${NC}"
    echo "Will try to use system-installed libteec"
    export TEEC_EXPORT="/usr"
fi

# Set cross-compiler
export CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"

echo ""
echo "Build Configuration:"
echo "  OPTEE_OS_DIR     = $OPTEE_OS_DIR"
echo "  TA_DEV_KIT_DIR   = $TA_DEV_KIT_DIR"
echo "  TEEC_EXPORT      = $TEEC_EXPORT"
echo "  CROSS_COMPILE    = $CROSS_COMPILE"
echo "  PROJECT_DIR      = $PROJECT_DIR"
echo ""

# Step 1: Build TA
echo -e "${YELLOW}[1/2] Building Trusted Application...${NC}"
cd "$PROJECT_DIR/ta"
make clean
make CROSS_COMPILE="$CROSS_COMPILE" \
     TA_DEV_KIT_DIR="$TA_DEV_KIT_DIR" \
     CFG_TEE_TA_LOG_LEVEL=4

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ TA build successful!${NC}"
    ls -lh *.ta
else
    echo -e "${RED}❌ TA build failed!${NC}"
    exit 1
fi

# Step 2: Build Host
echo ""
echo -e "${YELLOW}[2/2] Building Host Application...${NC}"
cd "$PROJECT_DIR/host"
make clean

# Build with explicit CC to use cross compiler
make CC="${CROSS_COMPILE}gcc" \
     TEEC_EXPORT="$TEEC_EXPORT"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Host build successful!${NC}"
    ls -lh hello_cpp_host
else
    echo -e "${YELLOW}⚠️  Host build failed (library mismatch)${NC}"
    echo -e "${YELLOW}    This is OK if you already have hello_cpp_host binary${NC}"
    echo -e "${YELLOW}    Host app needs to be built with ARM cross-compiler${NC}"
fi

# Create deployment directory
echo ""
echo -e "${YELLOW}Creating deployment package...${NC}"
DEPLOY_DIR="$PROJECT_DIR/deploy"
mkdir -p "$DEPLOY_DIR"

cp "$PROJECT_DIR/ta/"*.ta "$DEPLOY_DIR/"
cp "$PROJECT_DIR/host/hello_cpp_host" "$DEPLOY_DIR/"

echo ""
echo -e "${GREEN}======================================"
echo "  Build Completed Successfully!"
echo "======================================${NC}"
echo ""
echo "Deployment files in: $DEPLOY_DIR"
ls -lh "$DEPLOY_DIR"
echo ""
echo "To deploy to Raspberry Pi 5:"
echo "  ./deploy.sh <pi_user>@<pi_ip>"
echo ""
echo "Example:"
echo "  ./deploy.sh pi@192.168.1.100"
