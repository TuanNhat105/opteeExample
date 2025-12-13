#!/bin/bash
# Download musl-1.1.21 source code
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

MUSL_VERSION="1.1.21"
MUSL_URL="https://musl.libc.org/releases/musl-${MUSL_VERSION}.tar.gz"
DEST_DIR="external/openenclave/3rdparty/musl/musl"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Downloading musl-${MUSL_VERSION}${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check if already exists
if [ -d "$DEST_DIR/include" ] && [ -d "$DEST_DIR/src" ]; then
    echo -e "${YELLOW}musl already downloaded at: $DEST_DIR${NC}"
    ls -la "$DEST_DIR" | head -10
    exit 0
fi

# Download
echo -e "${YELLOW}Downloading from: $MUSL_URL${NC}"
wget -q --show-progress "$MUSL_URL" -O /tmp/musl.tar.gz

# Extract
echo -e "${YELLOW}Extracting to: $DEST_DIR${NC}"
rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"
tar -xzf /tmp/musl.tar.gz -C /tmp/
mv /tmp/musl-${MUSL_VERSION}/* "$DEST_DIR/"
rm -rf /tmp/musl-${MUSL_VERSION} /tmp/musl.tar.gz

echo ""
echo -e "${GREEN}✓ musl downloaded successfully!${NC}"
echo ""
echo "Location: $DEST_DIR"
echo "Headers: $DEST_DIR/include/"
echo "Sources: $DEST_DIR/src/"
echo ""
ls -lh "$DEST_DIR/include" | head -10
