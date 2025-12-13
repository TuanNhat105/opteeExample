#!/bin/bash
# ETL (Embedded Template Library) Integration Script
# Integrates STL-like containers for OP-TEE TA

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

echo "======================================"
echo "  ETL Integration for OP-TEE TA"
echo "======================================"
echo

# Step 1: Clone ETL
if [ -d "ta/external/etl" ]; then
    echo "✅ ETL already exists at ta/external/etl"
else
    echo "[1/4] Cloning ETL library..."
    mkdir -p ta/external
    cd ta/external
    git clone --depth 1 https://github.com/ETLCPP/etl.git
    cd ../..
    echo "✅ ETL cloned successfully"
fi

# Step 2: Verify ETL headers
echo "[2/4] Verifying ETL headers..."
if [ -f "ta/external/etl/include/etl/string.h" ]; then
    echo "✅ ETL headers found"
else
    echo "❌ ETL headers missing!"
    exit 1
fi

# Step 3: Test ETL compilation
echo "[3/4] Testing ETL compilation..."
cat > /tmp/test_etl.cpp << 'EOF'
#include <etl/string.h>
#include <etl/vector.h>
int main() {
    etl::string<10> s = "test";
    etl::vector<int, 5> v;
    return 0;
}
EOF

if aarch64-none-linux-gnu-g++ -std=c++17 \
    -I./ta/external/etl/include \
    -c /tmp/test_etl.cpp -o /tmp/test_etl.o 2>/dev/null; then
    echo "✅ ETL compiles successfully"
    rm -f /tmp/test_etl.cpp /tmp/test_etl.o
else
    echo "❌ ETL compilation failed"
    exit 1
fi

# Step 4: Update sub.mk
echo "[4/4] Updating ta/sub.mk..."
if grep -q "external/etl" ta/sub.mk; then
    echo "✅ sub.mk already includes ETL path"
else
    cat >> ta/sub.mk << 'EOF'

# ===================================================================
# ETL (Embedded Template Library) Support
# ===================================================================
# STL-like containers for embedded systems
# https://github.com/ETLCPP/etl
cppflags-y += -I./external/etl/include
EOF
    echo "✅ sub.mk updated"
fi

echo
echo "======================================"
echo "  ETL Integration Complete! ✅"
echo "======================================"
echo
echo "Available ETL Containers:"
echo "  - etl::string<SIZE>        (like std::string)"
echo "  - etl::vector<T, SIZE>     (like std::vector)"
echo "  - etl::map<K, V, SIZE>     (like std::map)"
echo "  - etl::array<T, SIZE>      (like std::array)"
echo "  - etl::queue<T, SIZE>      (like std::queue)"
echo "  - etl::list<T, SIZE>       (like std::list)"
echo
echo "Example Usage in TA:"
echo "  #include <etl/string.h>"
echo "  #include <etl/vector.h>"
echo "  "
echo "  etl::string<100> msg = \"Hello ETL!\";"
echo "  etl::vector<uint32_t, 50> numbers;"
echo "  numbers.push_back(42);"
echo
echo "Documentation:"
echo "  - ETL API docs: https://www.etlcpp.com/"
echo "  - Examples: ta/external/etl/examples/"
echo
echo "Next Steps:"
echo "  1. Create test TA with ETL containers"
echo "  2. Build: ./build.sh"
echo "  3. Test on Pi 5"
echo "  4. Analyze eEVM dependencies for porting"
echo
