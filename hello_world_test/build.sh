#!/bin/bash

# Build script for hello_world_test TA and host application
# Uses TA_DEV_KIT_DIR from OP-TEE OS build and optee_client

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Default paths - adjust these if needed
OPTEED_CLIENT_DIR="${OPTEED_CLIENT_DIR:-/home/abc/optee_client}"

# TA_DEV_KIT_DIR from OP-TEE OS build
TA_DEV_KIT_DIR="${TA_DEV_KIT_DIR:-/home/abc/nhat/pi6_build/bsp/tee/out/arm-plat-cix/export-ta_arm64}"

# TEEC_EXPORT from optee_client build
TEEC_EXPORT="${TEEC_EXPORT:-${OPTEED_CLIENT_DIR}/out/export/usr}"

# Cross compiler - use aarch64-none-linux-gnu- if available, otherwise aarch64-linux-gnu-
if command -v aarch64-none-linux-gnu-gcc >/dev/null 2>&1; then
    CROSS_COMPILE="${CROSS_COMPILE:-aarch64-none-linux-gnu-}"
elif command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
else
    echo "Error: No cross-compiler found"
    exit 1
fi

echo -e "${GREEN}=== Building hello_world_test ===${NC}"
echo "TA_DEV_KIT_DIR: $TA_DEV_KIT_DIR"
echo "TEEC_EXPORT: $TEEC_EXPORT"
echo "CROSS_COMPILE: $CROSS_COMPILE"
echo ""

# Check if TA_DEV_KIT_DIR exists
if [ ! -d "$TA_DEV_KIT_DIR" ]; then
    echo -e "${RED}Error: TA_DEV_KIT_DIR not found at: $TA_DEV_KIT_DIR${NC}"
    echo "Please build OP-TEE OS first or set TA_DEV_KIT_DIR environment variable"
    exit 1
fi

# Check if TA_DEV_KIT_DIR/mk/ta_dev_kit.mk exists
if [ ! -f "$TA_DEV_KIT_DIR/mk/ta_dev_kit.mk" ]; then
    echo -e "${RED}Error: ta_dev_kit.mk not found at: $TA_DEV_KIT_DIR/mk/ta_dev_kit.mk${NC}"
    exit 1
fi

# Check if TEEC_EXPORT exists
if [ ! -d "$TEEC_EXPORT" ]; then
    echo -e "${RED}Error: TEEC_EXPORT not found at: $TEEC_EXPORT${NC}"
    echo "Please build optee_client first or set TEEC_EXPORT environment variable"
    exit 1
fi

# Check if key exists
TA_KEY="${TA_DEV_KIT_DIR}/keys/default_ta.pem"
if [ ! -f "$TA_KEY" ]; then
    echo -e "${YELLOW}Warning: TA signing key not found at: $TA_KEY${NC}"
    echo "TA will not be signed properly"
fi

echo -e "${GREEN}Building TA...${NC}"
cd ta
make clean 2>/dev/null || true
make TA_DEV_KIT_DIR="$TA_DEV_KIT_DIR" CROSS_COMPILE="$CROSS_COMPILE"
cd ..

echo -e "${GREEN}Building host application...${NC}"
cd host
make clean 2>/dev/null || true
export CROSS_COMPILE="$CROSS_COMPILE"
export CC="${CROSS_COMPILE}gcc"
export LD="${CROSS_COMPILE}ld"
# Default to static build to avoid library dependency issues on device
# Set STATIC_BUILD=0 to use dynamic linking (requires libteec.so.2 on device)
STATIC_BUILD="${STATIC_BUILD:-1}"
if [ "$STATIC_BUILD" = "1" ]; then
    echo -e "${YELLOW}Using static linking (no library needed on device)${NC}"
else
    echo -e "${YELLOW}Using dynamic linking (requires libteec.so.2 on device)${NC}"
fi
make TEEC_EXPORT="$TEEC_EXPORT" CROSS_COMPILE="$CROSS_COMPILE" CC="${CROSS_COMPILE}gcc" STATIC_BUILD="$STATIC_BUILD"
cd ..

echo ""
echo -e "${GREEN}=== Build completed successfully ===${NC}"
echo "TA binary: ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
echo "Host binary: host/optee_example_hello_world"
if [ "$STATIC_BUILD" = "1" ]; then
    echo -e "${GREEN}(Static build - no library needed on device)${NC}"
else
    echo -e "${YELLOW}(Dynamic build - requires libteec.so.2 on device)${NC}"
fi
echo ""

# Auto-deploy to device if enabled
AUTO_DEPLOY="${AUTO_DEPLOY:-1}"
if [ "$AUTO_DEPLOY" = "1" ]; then
    echo -e "${GREEN}=== Auto-deploying to OrangePi ===${NC}"
    
    # Device info
    DEVICE_IP="${DEVICE_IP:-192.168.1.146}"
    DEVICE_USER="${DEVICE_USER:-orangepi}"
    DEVICE_PASS="${DEVICE_PASS:-orangepi}"
    DEVICE_TA_DIR="/lib/optee_armtz"
    DEVICE_HOST_DIR="/tmp"
    
    # Files to deploy
    TA_FILE="ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta"
    HOST_FILE="host/optee_example_hello_world"
    HOST_FILE_NAME=$(basename "$HOST_FILE")
    TA_UUID="8aaaf200-2450-11e4-abe2-0002a5d5c51b"
    
    # Check if files exist
    if [ ! -f "$TA_FILE" ]; then
        echo -e "${RED}❌ TA file not found: $TA_FILE${NC}"
        echo -e "${YELLOW}Skipping deployment${NC}"
        AUTO_DEPLOY=0
    fi
    
    if [ ! -f "$HOST_FILE" ]; then
        echo -e "${RED}❌ Host file not found: $HOST_FILE${NC}"
        echo -e "${YELLOW}Skipping deployment${NC}"
        AUTO_DEPLOY=0
    fi
    
    # Check sshpass
    if [ "$AUTO_DEPLOY" = "1" ] && ! command -v sshpass &> /dev/null; then
        echo -e "${YELLOW}⚠ sshpass not found, installing...${NC}"
        sudo apt-get update && sudo apt-get install -y sshpass || {
            echo -e "${RED}❌ Cannot install sshpass, skipping deployment${NC}"
            AUTO_DEPLOY=0
        }
    fi
    if [ "$AUTO_DEPLOY" = "1" ]; then
        # Test SSH connection first
        echo -e "${YELLOW}Testing SSH connection to $DEVICE_USER@$DEVICE_IP...${NC}"
        if ! sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$DEVICE_USER@$DEVICE_IP" "echo 'Connection OK'" 2>&1; then
            echo -e "${RED}❌ Cannot connect to $DEVICE_USER@$DEVICE_IP${NC}"
            echo -e "${YELLOW}⚠ SSH host key might have changed, trying to fix...${NC}"
            ssh-keygen -f ~/.ssh/known_hosts -R "$DEVICE_IP" 2>/dev/null || true
            ssh-keyscan -H "$DEVICE_IP" >> ~/.ssh/known_hosts 2>/dev/null || true
            
            # Try again
            if ! sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$DEVICE_USER@$DEVICE_IP" "echo 'Connection OK'" 2>&1; then
                echo -e "${RED}❌ Still cannot connect. Please check:${NC}"
                echo "  - Device IP: $DEVICE_IP"
                echo "  - SSH service is running"
                echo "  - Network connectivity"
                AUTO_DEPLOY=0
            else
                echo -e "${GREEN}✓ SSH connection OK${NC}"
            fi
        else
            echo -e "${GREEN}✓ SSH connection OK${NC}"
        fi
        
        if [ "$AUTO_DEPLOY" = "1" ]; then
            # Deploy TA
            echo -e "${YELLOW}Deploying TA to $DEVICE_USER@$DEVICE_IP...${NC}"
            if sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
                "echo '$DEVICE_PASS' | sudo -S mkdir -p $DEVICE_TA_DIR" 2>&1; then
                echo -e "${GREEN}  ✓ Created TA directory${NC}"
            else
                echo -e "${RED}  ❌ Failed to create TA directory${NC}"
            fi
            
            # Check if TA file exists on device and remove it
            echo -e "${YELLOW}  Checking for existing TA file...${NC}"
            if sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
                "test -f $DEVICE_TA_DIR/$TA_UUID.ta" 2>/dev/null; then
                echo -e "${YELLOW}  ⚠ Old TA file found, removing...${NC}"
                sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
                    "echo '$DEVICE_PASS' | sudo -S rm -f $DEVICE_TA_DIR/$TA_UUID.ta" 2>&1
                echo -e "${GREEN}  ✓ Old TA file removed${NC}"
            else
                echo -e "${GREEN}  ✓ No existing TA file${NC}"
            fi
            
            # Copy new TA file
            if sshpass -p "$DEVICE_PASS" scp -o StrictHostKeyChecking=no \
                "$TA_FILE" \
                "$DEVICE_USER@$DEVICE_IP:/tmp/$TA_UUID.ta" 2>&1; then
                echo -e "${GREEN}  ✓ Copied new TA to /tmp${NC}"
            else
                echo -e "${RED}  ❌ Failed to copy TA file${NC}"
                AUTO_DEPLOY=0
            fi
            
            if [ "$AUTO_DEPLOY" = "1" ]; then
                # Install TA with force overwrite
                if sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
                    "echo '$DEVICE_PASS' | sudo -S cp -f /tmp/$TA_UUID.ta $DEVICE_TA_DIR/$TA_UUID.ta && \
                     echo '$DEVICE_PASS' | sudo -S chmod 644 $DEVICE_TA_DIR/$TA_UUID.ta && \
                     echo '$DEVICE_PASS' | sudo -S chown root:root $DEVICE_TA_DIR/$TA_UUID.ta && \
                     echo '$DEVICE_PASS' | sudo -S rm -f /tmp/$TA_UUID.ta" 2>&1; then
                    echo -e "${GREEN}✓ TA deployed to $DEVICE_TA_DIR${NC}"
                else
                    echo -e "${RED}❌ Failed to install TA${NC}"
                fi
            fi
            
            # Deploy Host Application
            if [ "$AUTO_DEPLOY" = "1" ]; then
                echo -e "${YELLOW}Deploying host application...${NC}"
                
                # Check if host file exists on device and remove it
                echo -e "${YELLOW}  Checking for existing host application...${NC}"
                if sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
                    "test -f $DEVICE_HOST_DIR/$HOST_FILE_NAME" 2>/dev/null; then
                    echo -e "${YELLOW}  ⚠ Old host app found, removing...${NC}"
                    sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
                        "rm -f $DEVICE_HOST_DIR/$HOST_FILE_NAME" 2>&1
                    echo -e "${GREEN}  ✓ Old host app removed${NC}"
                else
                    echo -e "${GREEN}  ✓ No existing host app${NC}"
                fi
                
                # Copy new host application with force overwrite
                if sshpass -p "$DEVICE_PASS" scp -o StrictHostKeyChecking=no \
                    "$HOST_FILE" \
                    "$DEVICE_USER@$DEVICE_IP:$DEVICE_HOST_DIR/" 2>&1; then
                    echo -e "${GREEN}  ✓ Copied new host app${NC}"
                else
                    echo -e "${RED}  ❌ Failed to copy host application${NC}"
                    AUTO_DEPLOY=0
                fi
                
                if [ "$AUTO_DEPLOY" = "1" ]; then
                    if sshpass -p "$DEVICE_PASS" ssh -o StrictHostKeyChecking=no "$DEVICE_USER@$DEVICE_IP" \
                        "chmod +x $DEVICE_HOST_DIR/$HOST_FILE_NAME" 2>&1; then
                        echo -e "${GREEN}✓ Host application deployed${NC}"
                    else
                        echo -e "${YELLOW}⚠ Failed to set executable permission${NC}"
                    fi
                fi
            fi
            
            # Fix /dev/tee0 permissions
            if [ "$AUTO_DEPLOY" = "1" ]; then
                echo ""
                echo -e "${GREEN}=== Deployment completed successfully ===${NC}"
                echo -e "Files deployed to: ${GREEN}$DEVICE_USER@$DEVICE_IP${NC}"
                echo ""
                echo "To run the test:"
                echo "  ssh $DEVICE_USER@$DEVICE_IP"
                if [ "$STATIC_BUILD" = "1" ]; then
                    echo "  $DEVICE_HOST_DIR/$HOST_FILE_NAME"
                else
                    echo "  export LD_LIBRARY_PATH=/usr/lib:\$LD_LIBRARY_PATH"
                    echo "  $DEVICE_HOST_DIR/$HOST_FILE_NAME"
                fi
            else
                echo -e "${RED}=== Deployment failed ===${NC}"
                echo "Please check the errors above and try again"
            fi
        fi
    fi
else
    echo "To test on device:"
    echo "  1. Copy TA: scp ta/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta root@<device>:/lib/optee_armtz/"
    echo "  2. Copy host: scp host/optee_example_hello_world root@<device>:/usr/bin/"
    if [ "$STATIC_BUILD" != "1" ]; then
        echo "  3. Copy library: scp ${TEEC_EXPORT}/lib/libteec.so.2.0.0 root@<device>:/usr/lib/"
        echo "  4. On device: cd /usr/lib && ln -sf libteec.so.2.0.0 libteec.so.2 && ldconfig"
    fi
fi

