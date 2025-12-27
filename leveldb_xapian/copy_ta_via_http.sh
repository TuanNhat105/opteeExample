#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# Script to copy TA to device using HTTP server

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

DEVICE_IP="${DEVICE_IP:-192.168.1.114}"
BUILD_IP=$(ip route get 8.8.8.8 2>/dev/null | awk '{print $7; exit}' || hostname -I | awk '{print $1}')
TA_FILE="ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"
TA_DEST="/lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"

if [ ! -f "$TA_FILE" ]; then
    echo -e "${RED}ERROR: TA file not found: $TA_FILE${NC}"
    exit 1
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Copy TA via HTTP Server${NC}"
echo -e "${GREEN}========================================${NC}\n"

# Find available port
PORT=8000
while lsof -Pi :${PORT} -sTCP:LISTEN -t >/dev/null 2>&1 ; do
    PORT=$((PORT + 1))
    if [ $PORT -gt 8100 ]; then
        echo -e "${RED}ERROR: Could not find available port${NC}"
        exit 1
    fi
done

echo -e "${YELLOW}Build machine IP: ${BUILD_IP}${NC}"
echo -e "${YELLOW}Device IP: ${DEVICE_IP}${NC}"
echo -e "${YELLOW}Port: ${PORT}${NC}\n"

# Start HTTP server in background
echo -e "${BLUE}Starting HTTP server on port ${PORT}...${NC}"
cd "$(dirname "$0")"
python3 -m http.server ${PORT} > /tmp/http_server.log 2>&1 &
HTTP_PID=$!

# Wait a bit for server to start
sleep 2

# Check if server is running
if ! kill -0 $HTTP_PID 2>/dev/null; then
    echo -e "${RED}ERROR: Failed to start HTTP server${NC}"
    cat /tmp/http_server.log
    exit 1
fi

echo -e "${GREEN}✓ HTTP server started (PID: $HTTP_PID)${NC}"
echo -e "${YELLOW}Server URL: http://${BUILD_IP}:${PORT}/${TA_FILE}${NC}\n"

# Download TA on device
echo -e "${BLUE}Downloading TA on device...${NC}"
echo -e "${YELLOW}This may take a while (file size: $(du -h "$TA_FILE" | cut -f1))...${NC}"
expect << EOF
set timeout 300
spawn ssh -o StrictHostKeyChecking=no root@${DEVICE_IP} "wget http://${BUILD_IP}:${PORT}/${TA_FILE} -O ${TA_DEST} && chmod 644 ${TA_DEST} && ls -lh ${TA_DEST} && echo 'TA downloaded successfully'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

DOWNLOAD_RESULT=$?

# Stop HTTP server
echo -e "\n${YELLOW}Stopping HTTP server...${NC}"
kill $HTTP_PID 2>/dev/null || true
wait $HTTP_PID 2>/dev/null || true

if [ $DOWNLOAD_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ TA copied successfully!${NC}"
    
    # Verify TA on device
    echo -e "\n${YELLOW}Verifying TA on device...${NC}"
    TA_SIZE=$(expect << 'EOF' | grep -E "^[0-9]+$" | head -1 || echo "0"
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "stat -c%s /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)
    
    if [ "$TA_SIZE" != "0" ] && [ -n "$TA_SIZE" ]; then
        echo -e "${GREEN}✓ TA verified on device (size: $TA_SIZE bytes)${NC}"
    else
        echo -e "${RED}⚠ WARNING: Could not verify TA size${NC}"
    fi
else
    echo -e "${RED}✗ Failed to download TA${NC}"
    exit 1
fi

echo -e "\n${GREEN}Done!${NC}"

