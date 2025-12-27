#!/bin/bash
# Deploy and run exception test

DEVICE="root@192.168.1.114"
PASSWORD="0000"

echo "Deploying TA..."
expect << EOF
set timeout 30
spawn scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ta/*.ta $DEVICE:/lib/optee_armtz/
expect {
    "password:" { send "$PASSWORD\r"; exp_continue }
    eof
}
EOF

echo ""
echo "Deploying test_exception binary..."
# Use a simpler method: split into chunks
cat host/test_exception | base64 | split -b 10000 - /tmp/test_exception_part_

expect << EOF
set timeout 30
spawn ssh -o StrictHostKeyChecking=no $DEVICE "cat > /tmp/test_exception_b64 << 'REMOTEEOF'
$(cat host/test_exception | base64)
REMOTEEOF
base64 -d /tmp/test_exception_b64 > /tmp/test_exception && chmod +x /tmp/test_exception && /tmp/test_exception"
expect {
    "password:" { send "$PASSWORD\r"; exp_continue }
    eof
}
EOF

