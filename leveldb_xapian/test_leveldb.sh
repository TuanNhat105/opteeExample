#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# LevelDB Test Script

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

DEVICE_IP="${DEVICE_IP:-192.168.1.182}"
TA_PATH="/lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta"
HOST_BINARY="leveldb_host"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}LevelDB Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if binaries exist
if [ ! -f "ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta" ]; then
    echo -e "${RED}ERROR: TA binary not found!${NC}"
    echo "Please run ./build.sh first"
    exit 1
fi

if [ ! -f "host/leveldb_host" ]; then
    echo -e "${RED}ERROR: Host binary not found!${NC}"
    echo "Please run ./build.sh first"
    exit 1
fi

# Deploy to device
echo -e "${YELLOW}Deploying to device ${DEVICE_IP}...${NC}"
echo "1. Copying TA..."
scp -O ta/8aaaf200-2450-11e4-abe2-0002a5d5c53d.ta root@${DEVICE_IP}:${TA_PATH}
echo "2. Copying host application..."
scp -O host/leveldb_host root@${DEVICE_IP}:/usr/bin/
echo -e "${GREEN}Deployment completed!${NC}"
echo ""

# Test 1: Basic initialization
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test 1: LevelDB Initialization${NC}"
echo -e "${BLUE}========================================${NC}"
echo "Testing LevelDB initialization with ring buffer..."
ssh root@${DEVICE_IP} "leveldb_host" 2>&1 | tee test_output.log

# Check for success indicators
if grep -q "LevelDB opened successfully" test_output.log; then
    echo -e "${GREEN}✓ Test 1 PASSED: LevelDB initialized successfully${NC}"
else
    echo -e "${RED}✗ Test 1 FAILED: LevelDB initialization failed${NC}"
    echo "Check test_output.log for details"
    exit 1
fi

echo ""

# Test 2: PUT operation
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test 2: PUT Operation${NC}"
echo -e "${BLUE}========================================${NC}"
echo "Testing PUT operation..."
# This would require modifying the host app to support command-line args
# For now, we just verify the initialization worked

# Test 3: Check debug output
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test 3: Debug Output Verification${NC}"
echo -e "${BLUE}========================================${NC}"

if grep -q "\[DEBUG\]" test_output.log; then
    echo -e "${GREEN}✓ Debug logs present${NC}"
else
    echo -e "${YELLOW}⚠ No debug logs found (may be disabled)${NC}"
fi

if grep -q "\[STEP" test_output.log; then
    echo -e "${GREEN}✓ Step-by-step logs present${NC}"
else
    echo -e "${YELLOW}⚠ No step logs found${NC}"
fi

if grep -q "\[TEST\]" test_output.log; then
    echo -e "${GREEN}✓ Test code executed${NC}"
else
    echo -e "${YELLOW}⚠ No test code output found${NC}"
fi

if grep -q "\[PERF\]" test_output.log; then
    echo -e "${GREEN}✓ Performance timing present${NC}"
else
    echo -e "${YELLOW}⚠ No performance timing found${NC}"
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Test Summary${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Test output saved to: test_output.log"
echo ""
echo "To view full output:"
echo "  cat test_output.log"
echo ""
echo "To run tests again:"
echo "  ./test_leveldb.sh"
echo ""

