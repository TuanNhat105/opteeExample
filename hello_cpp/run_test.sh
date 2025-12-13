#!/bin/bash

TARGET=${1:-pi@raspberrypi.local}

echo "======================================"
echo "  Running Hello C++ TA on Pi 5"
echo "======================================"
echo ""

ssh "$TARGET" << 'EOF'
    echo "Running host application..."
    sudo ./hello_cpp_host
    
    echo ""
    echo "======================================"
    echo "  TA Kernel Logs:"
    echo "======================================"
    sudo dmesg | grep -E "(TA_|C\+\+|Hello)" | tail -50
EOF

echo ""
echo "======================================"
echo "  Test Completed!"
echo "======================================"
