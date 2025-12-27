#!/bin/bash
# Simple fix script - runs all commands in one SSH session
# Avoids SCP issues by using inline commands

echo "=========================================="
echo "OP-TEE Simple Fix Script"
echo "=========================================="
echo ""

# Create a single command string with all fixes
FIX_COMMANDS=$(cat << 'ENDOFCOMMANDS'
echo "=== OP-TEE Fix Script ==="
echo ""
echo "1. Loading OP-TEE kernel modules..."
modprobe optee 2>&1
modprobe optee_rpc 2>&1
echo ""
echo "2. Checking /dev/tee0..."
if [ -c /dev/tee0 ]; then
    echo "✓ /dev/tee0 exists"
    ls -l /dev/tee*
else
    echo "✗ /dev/tee0 NOT FOUND"
fi
echo ""
echo "3. Finding tee-supplicant..."
SUPPLICANT=""
for path in /usr/sbin/tee-supplicant /usr/bin/tee-supplicant /sbin/tee-supplicant /bin/tee-supplicant; do
    if [ -f "$path" ]; then
        SUPPLICANT="$path"
        echo "Found at: $path"
        break
    fi
done
echo ""
echo "4. Starting tee-supplicant..."
if [ -n "$SUPPLICANT" ]; then
    if ! pgrep -x tee-supplicant > /dev/null; then
        nohup "$SUPPLICANT" > /tmp/tee-supplicant.log 2>&1 &
        sleep 2
        if pgrep -x tee-supplicant > /dev/null; then
            echo "✓ tee-supplicant started (PID: $(pgrep -x tee-supplicant))"
        else
            echo "✗ Failed to start (check /tmp/tee-supplicant.log)"
        fi
    else
        echo "✓ tee-supplicant already running (PID: $(pgrep -x tee-supplicant))"
    fi
else
    echo "✗ tee-supplicant not found"
fi
echo ""
echo "5. Final status:"
if [ -c /dev/tee0 ] && pgrep -x tee-supplicant > /dev/null; then
    echo "✓ OP-TEE is ready!"
    echo "You can now test with: test_simple_shm"
else
    echo "✗ OP-TEE is not ready"
    [ ! -c /dev/tee0 ] && echo "  - /dev/tee0 missing"
    ! pgrep -x tee-supplicant > /dev/null && echo "  - tee-supplicant not running"
fi
ENDOFCOMMANDS
)

# Run commands on device
expect << EOF
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "bash -c '$FIX_COMMANDS'"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF

