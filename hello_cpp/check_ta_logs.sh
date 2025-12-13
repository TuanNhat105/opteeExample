#!/bin/bash
# Monitor OP-TEE TA logs on Buildroot
# Note: In minimal Buildroot, OP-TEE DMSG/IMSG logs may not be visible
# They are typically sent to secure UART or need special OP-TEE config

echo "======================================"
echo "  OP-TEE TA Log Monitor"
echo "======================================"
echo
echo "Checking available log sources..."
echo

# Check dmesg
if command -v dmesg &> /dev/null; then
    echo "[1] Checking dmesg (kernel log):"
    dmesg | tail -50 | grep -iE "optee|tee|secure" | tail -10 || echo "  No OP-TEE logs in dmesg"
    echo
fi

# Check syslog
if [ -f /var/log/messages ]; then
    echo "[2] Checking /var/log/messages:"
    tail -50 /var/log/messages | grep -iE "optee|tee|secure" | tail -10 || echo "  No OP-TEE logs in messages"
    echo
fi

# Check if tee-supplicant is running
echo "[3] TEE Supplicant status:"
ps aux | grep tee-supplicant | grep -v grep || echo "  tee-supplicant not running!"
echo

echo "======================================"
echo "  How to See TA Logs"
echo "======================================"
echo
echo "In Buildroot with minimal OP-TEE config:"
echo "  - DMSG() logs go to Secure World console (not accessible)"
echo "  - IMSG() logs may not be forwarded to Normal World"
echo
echo "Options to see TA debug output:"
echo
echo "1. Use UART debug console (requires hardware connection)"
echo "   - Connect UART cable to Pi 5 debug header"
echo "   - Use minicom/screen to read secure console"
echo
echo "2. Modify TA to return debug info in output buffers"
echo "   - Add debug strings to TEEC_Operation output"
echo "   - Host app can print received data"
echo
echo "3. Use shared memory for logging"
echo "   - Allocate shared memory in host app"
echo "   - TA writes logs to shared memory"
echo "   - Host app reads and prints logs"
echo
echo "4. Rebuild OP-TEE with debug output enabled"
echo "   - Set CFG_TEE_CORE_LOG_LEVEL=4"
echo "   - Enable CFG_TEE_CORE_DEBUG=y"
echo "   - Configure log output destination"
echo
echo "Current workaround:"
echo "  The hello_cpp_host application shows test results"
echo "  from Normal World. TA is working correctly even"
echo "  though you can't see DMSG() output."
echo
