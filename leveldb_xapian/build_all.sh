#!/bin/bash

# Build script for LevelDB with Ring Buffer on OP-TEE
# Usage: ./build_all.sh

set -e  # Exit on error

echo "=========================================="
echo " Building LevelDB Ring Buffer for OP-TEE"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check environment variables
if [ -z "$TA_DEV_KIT_DIR" ]; then
    echo -e "${RED}ERROR: TA_DEV_KIT_DIR is not set!${NC}"
    echo "Please set it to your OP-TEE OS export path, e.g.:"
    echo "export TA_DEV_KIT_DIR=/path/to/optee_os/out/arm/export-ta_arm64"
    exit 1
fi

echo -e "${GREEN}✓ TA_DEV_KIT_DIR: $TA_DEV_KIT_DIR${NC}"

# Check LevelDB
LEVELDB_ROOT="/home/abc/nhat/leveldb"
if [ ! -d "$LEVELDB_ROOT" ]; then
    echo -e "${RED}ERROR: LevelDB not found at $LEVELDB_ROOT${NC}"
    echo "Please clone and build LevelDB first:"
    echo "  cd /home/abc/nhat"
    echo "  git clone https://github.com/google/leveldb.git"
    echo "  cd leveldb && mkdir build && cd build"
    echo "  cmake -DCMAKE_BUILD_TYPE=Release .."
    echo "  make"
    exit 1
fi

echo -e "${GREEN}✓ LevelDB found at $LEVELDB_ROOT${NC}"
echo ""

# Build TA
echo "=========================================="
echo " Building Trusted Application (TA)"
echo "=========================================="

cd ta

echo "Cleaning previous build..."
make clean 2>/dev/null || true

echo "Building TA..."
if make; then
    echo -e "${GREEN}✓ TA built successfully!${NC}"
    TA_FILE=$(ls *.ta 2>/dev/null | head -1)
    if [ -n "$TA_FILE" ]; then
        echo -e "${GREEN}  TA file: $TA_FILE${NC}"
    fi
else
    echo -e "${RED}✗ TA build failed!${NC}"
    exit 1
fi

cd ..
echo ""

# Build Host
echo "=========================================="
echo " Building Host Application (CA)"
echo "=========================================="

cd host

echo "Cleaning previous build..."
make clean 2>/dev/null || true

echo "Building host..."
if make; then
    echo -e "${GREEN}✓ Host application built successfully!${NC}"
    echo -e "${GREEN}  Binary: leveldb_host${NC}"
else
    echo -e "${RED}✗ Host build failed!${NC}"
    exit 1
fi

cd ..
echo ""

# Summary
echo "=========================================="
echo " Build Summary"
echo "=========================================="
echo -e "${GREEN}✓ All components built successfully!${NC}"
echo ""
echo "Files ready to deploy:"
echo "  TA:   ta/*.ta"
echo "  Host: host/leveldb_host"
echo ""
echo "To deploy to target device:"
echo "  scp ta/*.ta root@<target-ip>:/lib/optee_armtz/"
echo "  scp host/leveldb_host root@<target-ip>:/root/"
echo ""
echo "To test locally (if on ARM64 with OP-TEE):"
echo "  sudo cp ta/*.ta /lib/optee_armtz/"
echo "  sudo ./host/leveldb_host"
echo ""
