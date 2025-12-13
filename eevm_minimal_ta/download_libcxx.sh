#!/bin/bash
# Download and setup libcxx-10.0.1 from LLVM for OP-TEE
# This is what OpenEnclave uses

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
LIBCXX_DIR="$PROJECT_ROOT/external/openenclave/3rdparty/libcxx"
LIBCXX_VERSION="10.0.1"
LIBCXX_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LIBCXX_VERSION}/libcxx-${LIBCXX_VERSION}.src.tar.xz"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Download libcxx-${LIBCXX_VERSION} from LLVM${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

cd "$LIBCXX_DIR"

# Check if already downloaded
if [ -d "libcxx/include" ]; then
    echo -e "${GREEN}✓ libcxx already downloaded!${NC}"
    ls -la libcxx/include/ | head -10
    exit 0
fi

echo "Downloading libcxx-${LIBCXX_VERSION}..."
wget -q --show-progress "$LIBCXX_URL" -O libcxx.tar.xz

echo ""
echo "Extracting..."
tar -xf libcxx.tar.xz

echo "Renaming..."
mv "libcxx-${LIBCXX_VERSION}.src" libcxx

echo "Cleaning up..."
rm libcxx.tar.xz

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✓ libcxx downloaded successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Location: $LIBCXX_DIR/libcxx"
echo ""
echo "Headers: $LIBCXX_DIR/libcxx/include"
ls -la libcxx/include/ | head -15
echo ""
echo "Sources: $LIBCXX_DIR/libcxx/src"
ls -la libcxx/src/*.cpp | head -15
