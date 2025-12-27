#!/bin/bash
# Deploy và test hello_world example từ optee_examples

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="/mnt/Data/TEE"
cd "$WORKSPACE"

# Device info
DEVICE_IP="192.168.1.146"
DEVICE_USER="orangepi"
DEVICE_PASS="orangepi"
DEVICE_TA_DIR="/lib/optee_armtz"
DEVICE_HOST_DIR="/tmp"

# Paths
HELLO_WORLD_DIR="$WORKSPACE/optee_examples/hello_world"
TA_FILE=$(find "$HELLO_WORLD_DIR/ta" -name "*.ta" -type f | head -1)
# Host application có thể là hello_world hoặc optee_example_hello_world
HOST_FILE="$HELLO_WORLD_DIR/host/hello_world"
if [ ! -f "$HOST_FILE" ]; then
    HOST_FILE="$HELLO_WORLD_DIR/host/optee_example_hello_world"
fi

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo "=========================================="
echo "Deploy và Test Hello World Example"
echo "Từ optee_examples (Theo README)"
echo "=========================================="
echo ""

# Kiểm tra files
if [ ! -f "$TA_FILE" ]; then
    echo -e "${RED}❌ TA file not found${NC}"
    echo "Chạy ./setup.sh trước"
    exit 1
fi

if [ ! -f "$HOST_FILE" ]; then
    echo -e "${RED}❌ Host application not found${NC}"
    echo "Chạy ./setup.sh trước"
    exit 1
fi

TA_UUID=$(basename "$TA_FILE" .ta)
echo -e "${GREEN}✓ TA file: $TA_FILE${NC}"
echo -e "${GREEN}✓ Host file: $HOST_FILE${NC}"
echo -e "${GREEN}✓ TA UUID: $TA_UUID${NC}"
echo ""

# Kiểm tra sshpass
if ! command -v sshpass &> /dev/null; then
    echo -e "${YELLOW}⚠ sshpass not found, installing...${NC}"
    sudo apt-get update && sudo apt-get install -y sshpass || {
        echo -e "${RED}❌ Cannot install sshpass${NC}"
        exit 1
    }
fi

# Fix SSH host key nếu cần
if ! sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$DEVICE_USER@$DEVICE_IP" "echo 'test'" 2>/dev/null; then
    echo -e "${YELLOW}⚠ SSH host key changed, fixing...${NC}"
    ssh-keygen -f ~/.ssh/known_hosts -R "$DEVICE_IP" 2>/dev/null || true
    ssh-keyscan -H "$DEVICE_IP" >> ~/.ssh/known_hosts 2>/dev/null || true
fi

# Deploy TA
echo "=== Deploying TA to Device ==="
echo "Device: $DEVICE_USER@$DEVICE_IP"

sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
    "echo '$DEVICE_PASS' | sudo -S mkdir -p $DEVICE_TA_DIR" 2>/dev/null || true

echo "Copying TA to device..."
sshpass -p "$DEVICE_PASS" scp -o StrictHostKeyChecking=no \
    "$TA_FILE" \
    "$DEVICE_USER@$DEVICE_IP:/tmp/$TA_UUID.ta"

sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
    "echo '$DEVICE_PASS' | sudo -S cp /tmp/$TA_UUID.ta $DEVICE_TA_DIR/$TA_UUID.ta && \
     echo '$DEVICE_PASS' | sudo -S chmod 644 $DEVICE_TA_DIR/$TA_UUID.ta && \
     echo '$DEVICE_PASS' | sudo -S chown root:root $DEVICE_TA_DIR/$TA_UUID.ta" 2>/dev/null

echo -e "${GREEN}✓ TA deployed${NC}"
echo ""

# Deploy Host Application
echo "=== Deploying Host Application ==="
HOST_FILE_NAME=$(basename "$HOST_FILE")
sshpass -p "$DEVICE_PASS" scp -o StrictHostKeyChecking=no \
    "$HOST_FILE" \
    "$DEVICE_USER@$DEVICE_IP:$DEVICE_HOST_DIR/"

sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
    "chmod +x $DEVICE_HOST_DIR/$HOST_FILE_NAME"

# Deploy libteec.so nếu cần
TEEC_LIB="$WORKSPACE/optee_client/out/export/usr/lib/libteec.so.2.0.0"
if [ -f "$TEEC_LIB" ]; then
    echo "Deploying libteec.so..."
    sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
        "echo '$DEVICE_PASS' | sudo -S mkdir -p /usr/lib" 2>/dev/null || true
    
    sshpass -p "$DEVICE_PASS" scp -o StrictHostKeyChecking=no \
        "$TEEC_LIB" \
        "$DEVICE_USER@$DEVICE_IP:/tmp/libteec.so.2.0.0"
    
    sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
        "echo '$DEVICE_PASS' | sudo -S cp /tmp/libteec.so.2.0.0 /usr/lib/ && \
         echo '$DEVICE_PASS' | sudo -S chmod 755 /usr/lib/libteec.so.2.0.0 && \
         echo '$DEVICE_PASS' | sudo -S chown root:root /usr/lib/libteec.so.2.0.0 && \
         cd /usr/lib && \
         echo '$DEVICE_PASS' | sudo -S ln -sf libteec.so.2.0.0 libteec.so.2 && \
         echo '$DEVICE_PASS' | sudo -S ln -sf libteec.so.2.0.0 libteec.so && \
         echo '$DEVICE_PASS' | sudo -S ldconfig" 2>/dev/null
    echo -e "${GREEN}✓ libteec.so deployed${NC}"
fi

echo -e "${GREEN}✓ Host application deployed${NC}"
echo ""

# Fix /dev/tee0 permissions
echo "=== Fixing /dev/tee0 Permissions ==="
sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
    "echo '$DEVICE_PASS' | sudo -S chmod 666 /dev/tee0" 2>/dev/null || true

# Restart tee-supplicant
echo "=== Restarting tee-supplicant ==="
sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
    "echo '$DEVICE_PASS' | sudo -S systemctl restart tee-supplicant" 2>/dev/null || \
    sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
        "pkill -HUP tee-supplicant" 2>/dev/null || true

sleep 2

# Test
echo "=== Running Test ==="
echo ""

# Set LD_LIBRARY_PATH để tìm libteec.so
sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
    "export LD_LIBRARY_PATH=/usr/lib:\$LD_LIBRARY_PATH && $DEVICE_HOST_DIR/$HOST_FILE_NAME"

TEST_RESULT=$?

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}=========================================="
    echo "✓✓✓ TEST THÀNH CÔNG!"
    echo "==========================================${NC}"
    echo ""
    echo "Hello World example từ optee_examples hoạt động!"
    echo ""
else
    echo -e "${RED}=========================================="
    echo "❌ TEST THẤT BẠI"
    echo "==========================================${NC}"
    echo ""
    echo "Kiểm tra logs:"
    sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
        "dmesg | tail -20 | grep -i tee || journalctl -u tee-supplicant --no-pager | tail -10"
    echo ""
    exit 1
fi
