#!/bin/bash
# Simple script to start tee-supplicant on device

echo "=========================================="
echo "Starting tee-supplicant on device"
echo "=========================================="
echo ""

# Check if already running (using ps instead of pgrep for compatibility)
echo "Checking if tee-supplicant is already running..."
ALREADY_RUNNING=$(expect << 'EOF' | grep -v "password:" | tail -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ps | grep '[t]ee-supplicant' > /dev/null && echo 'YES' || echo 'NO'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

if [ "$ALREADY_RUNNING" = "YES" ]; then
    echo "✓ tee-supplicant is already running"
    PID=$(expect << 'EOF' | grep -v "password:" | awk '{print $1}' | tail -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ps | grep '[t]ee-supplicant'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)
    echo "  PID: $PID"
    exit 0
fi

echo "tee-supplicant is not running. Starting..."
echo ""

# Start tee-supplicant with explicit device path
echo "Starting tee-supplicant with /dev/teepriv0..."
expect << 'EOF'
set timeout 30
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "/usr/sbin/tee-supplicant -d /dev/teepriv0 > /tmp/tee-supplicant.log 2>&1 &"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

sleep 3

# Check if started (using ps instead of pgrep)
echo "Checking if tee-supplicant started..."
STATUS=$(expect << 'EOF' | grep -v "password:" | tail -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ps | grep '[t]ee-supplicant' > /dev/null && echo 'RUNNING' || echo 'NOT_RUNNING'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)

if [ "$STATUS" = "RUNNING" ]; then
    PID=$(expect << 'EOF' | grep -v "password:" | awk '{print $1}' | tail -1
set timeout 10
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "ps | grep '[t]ee-supplicant'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
)
    echo "✓ tee-supplicant started successfully"
    echo "  PID: $PID"
    echo ""
    echo "You can now test with:"
    echo "  ssh root@192.168.1.114 'test_simple_shm'"
else
    echo "✗ Failed to start tee-supplicant"
    echo ""
    echo "Check logs on device:"
    echo "  ssh root@192.168.1.114 'cat /tmp/tee-supplicant.log'"
    echo ""
    echo "Or try manually:"
    echo "  ssh root@192.168.1.114"
    echo "  /usr/sbin/tee-supplicant &"
    exit 1
fi

