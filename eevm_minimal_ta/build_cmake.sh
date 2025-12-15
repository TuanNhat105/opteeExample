#!/bin/bash
set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}==================================================${NC}"
echo -e "${GREEN}Minimal EVM TA - CMAKE Build${NC}"
echo -e "${GREEN}==================================================${NC}"

# ===================================================
# Environment Setup
# ===================================================
export TA_DEV_KIT_DIR="/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64"
export TEEC_EXPORT="/home/abc/optee_client/out/export/usr"
export PATH="/home/abc/arm-toolchain/bin:$PATH"
export CROSS_COMPILE="aarch64-none-linux-gnu-"

# Check required libraries
if [ ! -f "build_oe_libs/standalone/libc++_standalone.a" ]; then
    echo -e "${RED}Error: OpenEnclave libraries not built!${NC}"
    echo "Please run:"
    echo "  ./build_openenclave_libs.sh"
    echo "  ./create_standalone_libcxx.sh"
    exit 1
fi

# ===================================================
# CMAKE Build
# ===================================================
echo -e "${YELLOW}[1/4] Cleaning previous build...${NC}"
rm -rf build_cmake
mkdir -p build_cmake

echo -e "${YELLOW}[2/4] Running CMAKE configuration...${NC}"
cd build_cmake

# Use CMakeLists_new.txt as the main CMakeLists.txt
cp ../CMakeLists_new.txt ../CMakeLists.txt

cmake -DCMAKE_BUILD_TYPE=Release \
      -DCROSS_COMPILE=${CROSS_COMPILE} \
      -DTA_DEV_KIT_DIR=${TA_DEV_KIT_DIR} \
      -DTEEC_EXPORT=${TEEC_EXPORT} \
      -G "Unix Makefiles" \
      -DCMAKE_INSTALL_PREFIX=./install \
      ..

echo -e "${YELLOW}[3/4] Building TA and Host...${NC}"
make -j$(nproc) VERBOSE=1

echo -e "${YELLOW}[4/4] Installing binaries...${NC}"
make install DESTDIR=./install

# ===================================================
# Verify Build
# ===================================================
echo ""
echo -e "${GREEN}==================================================${NC}"
echo -e "${GREEN}Build Summary${NC}"
echo -e "${GREEN}==================================================${NC}"

TA_FILE="ta_cmake/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
HOST_FILE="host_cmake/minimal_evm_host"

if [ -f "$TA_FILE" ]; then
    SIZE=$(du -h "$TA_FILE" | cut -f1)
    echo -e "${GREEN}✓ TA Binary:${NC} $TA_FILE (${SIZE})"
else
    echo -e "${RED}✗ TA Binary: NOT FOUND${NC}"
    exit 1
fi

if [ -f "$HOST_FILE" ]; then
    SIZE=$(du -h "$HOST_FILE" | cut -f1)
    echo -e "${GREEN}✓ Host Binary:${NC} $HOST_FILE (${SIZE})"
else
    echo -e "${RED}✗ Host Binary: NOT FOUND${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}Build completed successfully!${NC}"
echo ""
echo "To deploy on Raspberry Pi 5:"
echo "  scp $TA_FILE pi@<ip>:/lib/optee_armtz/"
echo "  scp $HOST_FILE pi@<ip>:/tmp/"
echo "  ssh pi@<ip> '/tmp/minimal_evm_host'"
