#!/bin/bash

# Install ARM64 Toolchain for OP-TEE Development

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════╗"
echo "║   ARM64 Toolchain Installation            ║"
echo "╚═══════════════════════════════════════════╝${NC}"
echo ""

# Check if running on Ubuntu/Debian
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    echo -e "${YELLOW}Detected OS: $NAME${NC}"
else
    echo -e "${RED}Cannot detect OS${NC}"
    exit 1
fi

# Option 1: Use apt (recommended for Ubuntu/Debian)
if command -v apt &> /dev/null; then
    echo ""
    echo -e "${YELLOW}[Option 1] Installing via apt...${NC}"
    echo "This will install:"
    echo "  - gcc-aarch64-linux-gnu"
    echo "  - g++-aarch64-linux-gnu"
    echo ""
    read -p "Continue? (y/n) " -n 1 -r
    echo
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo apt update
        sudo apt install -y \
            gcc-aarch64-linux-gnu \
            g++-aarch64-linux-gnu \
            binutils-aarch64-linux-gnu
        
        echo ""
        echo -e "${GREEN}✅ Toolchain installed via apt${NC}"
        echo ""
        echo "Compiler version:"
        aarch64-linux-gnu-gcc --version | head -n1
        aarch64-linux-gnu-g++ --version | head -n1
        exit 0
    fi
fi

# Option 2: Download ARM toolchain (Linaro)
echo ""
echo -e "${YELLOW}[Option 2] Installing Linaro toolchain...${NC}"
echo "This will download and install ARM GCC toolchain"
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Installation cancelled"
    exit 1
fi

TOOLCHAIN_DIR="$HOME/.local/toolchains"
TOOLCHAIN_URL="https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-x86_64-aarch64-none-linux-gnu.tar.xz"
TOOLCHAIN_ARCHIVE="arm-toolchain.tar.xz"
TOOLCHAIN_NAME="arm-gnu-toolchain-13.2.rel1-x86_64-aarch64-none-linux-gnu"

mkdir -p "$TOOLCHAIN_DIR"
cd "$TOOLCHAIN_DIR"

echo ""
echo -e "${YELLOW}Downloading toolchain...${NC}"
wget -O "$TOOLCHAIN_ARCHIVE" "$TOOLCHAIN_URL"

echo ""
echo -e "${YELLOW}Extracting...${NC}"
tar -xf "$TOOLCHAIN_ARCHIVE"
rm "$TOOLCHAIN_ARCHIVE"

# Add to PATH
TOOLCHAIN_PATH="$TOOLCHAIN_DIR/$TOOLCHAIN_NAME/bin"

echo ""
echo -e "${GREEN}✅ Toolchain installed to: $TOOLCHAIN_PATH${NC}"
echo ""

# Update bashrc
if ! grep -q "aarch64-none-linux-gnu" ~/.bashrc; then
    echo "" >> ~/.bashrc
    echo "# ARM64 Toolchain for OP-TEE" >> ~/.bashrc
    echo "export PATH=\"$TOOLCHAIN_PATH:\$PATH\"" >> ~/.bashrc
    echo "export CROSS_COMPILE=aarch64-none-linux-gnu-" >> ~/.bashrc
    
    echo -e "${GREEN}✅ Added to ~/.bashrc${NC}"
fi

# Test
export PATH="$TOOLCHAIN_PATH:$PATH"

if command -v aarch64-none-linux-gnu-gcc &> /dev/null; then
    echo ""
    echo -e "${GREEN}Toolchain version:${NC}"
    aarch64-none-linux-gnu-gcc --version | head -n1
    aarch64-none-linux-gnu-g++ --version | head -n1
    echo ""
    echo -e "${YELLOW}⚠️  Please run: source ~/.bashrc${NC}"
    echo "Or open a new terminal"
else
    echo -e "${RED}Installation verification failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════╗"
echo "║   Installation Completed!                 ║"
echo "╚═══════════════════════════════════════════╝${NC}"
echo ""
echo "Run the following to activate:"
echo "  ${YELLOW}source ~/.bashrc${NC}"
echo ""
echo "Or use directly:"
echo "  ${YELLOW}export PATH=\"$TOOLCHAIN_PATH:\$PATH\"${NC}"
