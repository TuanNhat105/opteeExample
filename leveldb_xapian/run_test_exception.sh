#!/bin/bash
# Run test_exception on device using compressed base64

echo "Copying test_exception to device (compressed)..."

B64_DATA=$(cat host/test_exception | gzip | base64 -w 0)

expect << EOF
set timeout 60
spawn ssh -o StrictHostKeyChecking=no root@192.168.1.114 "echo '$B64_DATA' | base64 -d | gunzip > /tmp/test_exception && chmod +x /tmp/test_exception && /tmp/test_exception"
expect {
    "password:" { send "0000\r"; exp_continue }
    eof
}
EOF
