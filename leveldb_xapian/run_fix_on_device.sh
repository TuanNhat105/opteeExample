#!/bin/bash
# Script to run fix_optee_on_device.sh directly on device via SSH
# This avoids SCP issues by encoding the script and running it inline

echo "=========================================="
echo "Running OP-TEE Fix on Device (via SSH)"
echo "=========================================="
echo ""

# Read the on-device script and encode it
SCRIPT_CONTENT=$(cat fix_optee_on_device.sh)

# Use expect to run the script directly on device
expect << EOF
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "bash -s"
expect {
    "password:" { send "0000\r"; exp_continue }
    "# " { }
    "$ " { }
}

# Send the script content
send "$SCRIPT_CONTENT\r"
send "exit\r"
expect eof
EOF

