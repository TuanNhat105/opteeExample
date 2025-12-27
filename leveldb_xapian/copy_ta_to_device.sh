#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# Script to copy TA to device using base64 (when scp doesn't work)

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

DEVICE_IP="${DEVICE_IP:-192.168.1.114}"
TA_FILE="ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"
TA_DEST="/lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"

if [ ! -f "$TA_FILE" ]; then
    echo "ERROR: TA file not found: $TA_FILE"
    exit 1
fi

echo -e "${GREEN}Copying TA to device using base64 (chunked)...${NC}"
echo "This may take a while for large files..."

# Split base64 into chunks of 10000 bytes
TEMP_DIR=$(mktemp -d)
cat "$TA_FILE" | base64 | split -b 10000 - "$TEMP_DIR/ta_part_"

# Count parts
PART_COUNT=$(ls -1 "$TEMP_DIR"/ta_part_* 2>/dev/null | wc -l)
echo "File split into $PART_COUNT parts"

# Copy first part and create file
FIRST_PART=$(ls -1 "$TEMP_DIR"/ta_part_* | head -1)
FIRST_CONTENT=$(cat "$FIRST_PART")

expect << EOF
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@${DEVICE_IP} "cat > /tmp/ta_b64 << 'REMOTEEOF'
${FIRST_CONTENT}
REMOTEEOF
echo 'Part 1/$PART_COUNT received'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Append remaining parts
PART_NUM=2
for part in $(ls -1 "$TEMP_DIR"/ta_part_* | tail -n +2); do
    PART_CONTENT=$(cat "$part")
    expect << EOF
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@${DEVICE_IP} "cat >> /tmp/ta_b64 << 'REMOTEEOF'
${PART_CONTENT}
REMOTEEOF
echo 'Part $PART_NUM/$PART_COUNT appended'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
    PART_NUM=$((PART_NUM + 1))
done

# Check if file exists before decoding
echo -e "${YELLOW}Verifying file on device...${NC}"
FILE_EXISTS=$(expect << 'EOF' | grep -q "exists" && echo "YES" || echo "NO"
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "test -f /tmp/ta_b64 && echo 'exists' || echo 'not found'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

if [ "$FILE_EXISTS" != "YES" ]; then
    echo "ERROR: File /tmp/ta_b64 not found on device. Copy may have failed."
    exit 1
fi

# Decode and install
echo -e "${YELLOW}Decoding and installing TA...${NC}"
expect << 'EOF'
set timeout 120
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "base64 -d /tmp/ta_b64 > /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta && chmod 644 /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta && rm /tmp/ta_b64 && ls -lh /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta && echo 'TA installed successfully'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

# Cleanup
rm -rf "$TEMP_DIR"

echo -e "${GREEN}TA copied successfully!${NC}"

