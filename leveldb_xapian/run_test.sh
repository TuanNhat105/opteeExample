#!/bin/bash
# Quick test script for eEVM on Raspberry Pi 5

echo "=========================================="
echo "eEVM OP-TEE Test Script"
echo "=========================================="
echo ""

# Check if we're on Pi 5 or need to SSH
if [ -f /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta ]; then
    # Running on Pi 5
    echo "Running on Pi 5..."
    ./host/eevm_host
else
    # Running on host, need to SSH
    echo "Connecting to Pi 5 (192.168.1.74)..."
    ssh root@192.168.1.74 "cd ~ && ./eevm_host"
fi

echo ""
echo "=========================================="
echo "Test completed!"
echo "=========================================="
