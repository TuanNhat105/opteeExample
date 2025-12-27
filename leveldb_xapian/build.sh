#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# Build script for eEVM OP-TEE application

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color
export LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH"
export TA_DEV_KIT_DIR="/home/orangepi/tee-core/export-ta_arm64"
export TEEC_EXPORT="/home/orangepi/tee-core/usr"
CROSS_COMPILE="${CROSS_COMPILE:-}"
echo "Configuration:"
echo "  TA_DEV_KIT_DIR = $TA_DEV_KIT_DIR"
echo "  CROSS_COMPILE  = $CROSS_COMPILE"
echo "  TEEC_EXPORT    = $TEEC_EXPORT"
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}eEVM OP-TEE Build Script${NC}"
echo -e "${GREEN}========================================${NC}"

# Check required environment variables
if [ -z "$TA_DEV_KIT_DIR" ]; then
    echo -e "${RED}ERROR: TA_DEV_KIT_DIR is not set${NC}"
    echo "Please set TA_DEV_KIT_DIR to point to your OP-TEE TA dev kit"
    echo "Example: export TA_DEV_KIT_DIR=/path/to/optee_os/out/arm/export-ta_arm64"
    exit 1
fi

if [ -z "$TEEC_EXPORT" ]; then
    echo -e "${RED}ERROR: TEEC_EXPORT is not set${NC}"
    echo "Please set TEEC_EXPORT to point to your OP-TEE client export"
    echo "Example: export TEEC_EXPORT=/path/to/optee_client/out/export"
    exit 1
fi

if [ -z "$CROSS_COMPILE" ]; then
    echo -e "${YELLOW}WARNING: CROSS_COMPILE is not set, using native compiler${NC}"
fi

# Check if required libraries exist
EEVM_MINIMAL_TA="../eevm_minimal_ta"

echo -e "\n${YELLOW}Checking required libraries...${NC}"

if [ ! -f "$EEVM_MINIMAL_TA/build_oe_libs/libcxx/libc++.a" ]; then
    echo -e "${RED}ERROR: libcxx not found at $EEVM_MINIMAL_TA/build_oe_libs/libcxx/libc++.a${NC}"
    echo "Please build libcxx first using build_openenclave_libs.sh in eevm_minimal_ta"
    exit 1
fi

if [ ! -f "$EEVM_MINIMAL_TA/libc/liboelibc.a" ]; then
    echo -e "${RED}ERROR: liboelibc not found at $EEVM_MINIMAL_TA/libc/liboelibc.a${NC}"
    echo "Please build libc first using build_libc.sh in eevm_minimal_ta"
    exit 1
fi

if [ ! -f "$EEVM_MINIMAL_TA/build_oe_libs/libcxxrt/libcxxrt.a" ]; then
    echo -e "${RED}ERROR: libcxxrt not found${NC}"
    echo "Please build libcxxrt first using build_libcxxrt_complete.sh in eevm_minimal_ta"
    exit 1
fi

if [ ! -f "$EEVM_MINIMAL_TA/build_libunwind/libunwind.a" ]; then
    echo -e "${RED}ERROR: libunwind not found${NC}"
    echo "Please build libunwind first using build_libunwind_openenclave.sh in eevm_minimal_ta"
    exit 1
fi

echo -e "${GREEN}All required libraries found!${NC}"

# Build TA with DEBUG flags enabled for testing
echo -e "\n${YELLOW}Building Trusted Application (TA) with DEBUG enabled...${NC}"
echo -e "${YELLOW}Debug flags: DEBUG_ENABLED=1, DEBUG_PARAMS=1, DEBUG_STEPS=1, DEBUG_TEST_CODE=1${NC}"
cd ta
make clean
# Build with all debug flags enabled for testing
make DEBUG_ENABLED=1 DEBUG_PARAMS=1 DEBUG_STEPS=1 DEBUG_TEST_CODE=1 DEBUG_PERF=1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}TA built successfully with DEBUG enabled!${NC}"
    ls -lh *.ta
else
    echo -e "${RED}TA build failed!${NC}"
    exit 1
fi
cd ..

# Build Host application
echo -e "\n${YELLOW}Building Host Application...${NC}"
cd host
make clean
make -j$(nproc)
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Host application built successfully!${NC}"
    echo -e "${GREEN}Binaries:" 
    echo -e "  - leveldb_host (main application)"
    echo -e "  - test_simple_shm (simple shared memory test - NO LevelDB)${NC}"
    ls -lh leveldb_host test_simple_shm 2>/dev/null || true
else
    echo -e "${RED}Host build failed!${NC}"
    exit 1
fi
cd ..
# scp -O ta/*.ta root@192.168.1.74:/lib/optee_armtz/
# scp -O host/leveldb_host root@192.168.1.74:/usr/bin/

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "TA binary: ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"
echo "Host binaries:"

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
HOST_BINARY="host/test_simple_shm"
if [ -f "$HOST_BINARY" ]; then
    echo -e "${YELLOW}Copying host binary to /usr/bin/...${NC}"
    sudo cp "$HOST_BINARY" /usr/bin/
    sudo chmod 755 /usr/bin/test_simple_shm
    echo -e "${GREEN}✓ Host binary copied successfully${NC}"
else
    echo -e "${RED}Error: Host binary not found: $HOST_BINARY${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=== Files installed successfully ===${NC}"
test_simple_shm
