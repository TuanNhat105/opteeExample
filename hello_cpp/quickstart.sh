#!/bin/bash

# Quick Start Guide for Hello C++ TA

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}"
cat << "EOF"
╔═══════════════════════════════════════════╗
║   Hello C++ - Quick Start Guide          ║
║   C++ in OP-TEE Secure World              ║
╚═══════════════════════════════════════════╝
EOF
echo -e "${NC}"

# Step 1: Check toolchain
echo -e "${YELLOW}[Step 1/5] Checking toolchain...${NC}"
if command -v aarch64-none-linux-gnu-g++ &> /dev/null; then
    TOOLCHAIN_VERSION=$(aarch64-none-linux-gnu-g++ --version | head -n1)
    echo -e "${GREEN}✅ Toolchain found: $TOOLCHAIN_VERSION${NC}"
    export CROSS_COMPILE="aarch64-none-linux-gnu-"
elif command -v aarch64-linux-gnu-g++ &> /dev/null; then
    TOOLCHAIN_VERSION=$(aarch64-linux-gnu-g++ --version | head -n1)
    echo -e "${GREEN}✅ Toolchain found: $TOOLCHAIN_VERSION${NC}"
    export CROSS_COMPILE="aarch64-linux-gnu-"
else
    echo -e "${RED}❌ No ARM64 toolchain found!${NC}"
    echo "Please install:"
    echo "  sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
    exit 1
fi

# Step 2: Check OP-TEE OS
echo ""
echo -e "${YELLOW}[Step 2/5] Checking OP-TEE OS...${NC}"
OPTEE_OS_DIR="${OPTEE_OS_DIR:-/home/abc/optee_os}"
if [ -d "$OPTEE_OS_DIR/out/arm-plat-rpi5/export-ta_arm64" ]; then
    echo -e "${GREEN}✅ OP-TEE OS found: $OPTEE_OS_DIR (RPi5)${NC}"
    export TA_DEV_KIT_DIR="$OPTEE_OS_DIR/out/arm-plat-rpi5/export-ta_arm64"
elif [ -d "$OPTEE_OS_DIR/out/arm/export-ta_arm64" ]; then
    echo -e "${GREEN}✅ OP-TEE OS found: $OPTEE_OS_DIR${NC}"
    export TA_DEV_KIT_DIR="$OPTEE_OS_DIR/out/arm/export-ta_arm64"
else
    echo -e "${RED}❌ OP-TEE OS not built!${NC}"
    echo "Please build OP-TEE OS first:"
    echo "  cd $OPTEE_OS_DIR"
    echo "  make PLATFORM=vexpress-qemu_armv8a CFG_ARM64_core=y -j\$(nproc)"
    exit 1
fi

# Step 3: Check OP-TEE Client
echo ""
echo -e "${YELLOW}[Step 3/5] Checking OP-TEE Client...${NC}"
if [ -d "/home/abc/optee_client/out/export" ]; then
    echo -e "${GREEN}✅ OP-TEE Client found${NC}"
    export TEEC_EXPORT="/home/abc/optee_client/out/export/usr"
elif pkg-config --exists tee-client-api; then
    echo -e "${GREEN}✅ System libteec found${NC}"
    export TEEC_EXPORT="/usr"
else
    echo -e "${YELLOW}⚠️  libteec not found, will use system default${NC}"
    export TEEC_EXPORT="/usr"
fi

# Step 4: Build
echo ""
echo -e "${YELLOW}[Step 4/5] Building Hello C++ TA...${NC}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

./build.sh

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Build failed!${NC}"
    exit 1
fi

# Step 5: Instructions
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════╗"
echo "║   Build Completed Successfully!           ║"
echo "╚═══════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}Next steps:${NC}"
echo ""
echo "1️⃣  Deploy to Raspberry Pi 5:"
echo "   ${YELLOW}./deploy.sh pi@<IP_ADDRESS>${NC}"
echo ""
echo "2️⃣  Run tests:"
echo "   ${YELLOW}./run_test.sh pi@<IP_ADDRESS>${NC}"
echo ""
echo "3️⃣  Or manually on Pi:"
echo "   ${YELLOW}ssh pi@<IP_ADDRESS>${NC}"
echo "   ${YELLOW}sudo ./hello_cpp_host${NC}"
echo ""
echo -e "${BLUE}Example:${NC}"
echo "   ./deploy.sh pi@192.168.1.100"
echo "   ./run_test.sh pi@192.168.1.100"
echo ""
