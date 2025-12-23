#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# Build script for eEVM OP-TEE application

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color
export TA_DEV_KIT_DIR="/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64"
export CROSS_COMPILE="aarch64-none-linux-gnu-"
export TEEC_EXPORT="/home/abc/optee_client/out/export/usr"
export PATH="/home/abc/arm-toolchain/bin:$PATH"
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
# echo "  - host/leveldb_host (main application)"
echo "  - host/test_simple_shm (simple shared memory test - NO LevelDB)"
echo ""
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Testing Options${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""
echo "To deploy and run tests:"
echo "1. Copy the TA to your device: /lib/optee_armtz/"
scp -O ta/*.ta root@192.168.1.182:/lib/optee_armtz/
echo ""
echo "2. Choose a test:"
echo "   a) Simple Shared Memory Test (NO LevelDB):"
echo "      scp -O host/test_simple_shm root@192.168.1.182:/usr/bin/"
echo "      ssh root@192.168.1.182 \"test_simple_shm\""
echo ""
echo "   b) Full LevelDB Test:"
echo "      scp -O host/leveldb_host root@192.168.1.182:/usr/bin/"
echo "      ssh root@192.168.1.182 \"leveldb_host\""
echo ""
echo -e "${GREEN}Running Simple Shared Memory Test (recommended first)...${NC}"
scp -O host/test_simple_shm root@192.168.1.182:/usr/bin/
ssh root@192.168.1.182 "test_simple_shm"
echo ""
echo -e "${YELLOW}Test completed! Check output above for:${NC}"
echo "  - [DEBUG] Parameter types and values"
echo "  - [STEP] Step-by-step execution"
echo "  - [TEST] Memory access tests"
echo "  - [PERF] Performance timing"
echo "  - [INFO] Operation results"
echo ""
