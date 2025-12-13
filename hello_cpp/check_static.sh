#!/bin/bash

# Check if TA is properly statically linked

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

TA_FILE=$1

if [ -z "$TA_FILE" ]; then
    echo -e "${RED}Usage: $0 <ta_file.ta>${NC}"
    echo "Example: $0 deploy/f4e750bb-1437-4fbf-8785-8d3580c34994.ta"
    exit 1
fi

if [ ! -f "$TA_FILE" ]; then
    echo -e "${RED}Error: File not found: $TA_FILE${NC}"
    exit 1
fi

echo -e "${BLUE}╔═══════════════════════════════════════════╗"
echo "║   Static Linking Checker                  ║"
echo "╚═══════════════════════════════════════════╝${NC}"
echo ""
echo -e "${YELLOW}Analyzing: $TA_FILE${NC}"
echo ""

# Find readelf
READELF=$(which aarch64-none-linux-gnu-readelf 2>/dev/null || which aarch64-linux-gnu-readelf 2>/dev/null || echo "")
NM=$(which aarch64-none-linux-gnu-nm 2>/dev/null || which aarch64-linux-gnu-nm 2>/dev/null || echo "")
OBJDUMP=$(which aarch64-none-linux-gnu-objdump 2>/dev/null || which aarch64-linux-gnu-objdump 2>/dev/null || echo "")

if [ -z "$READELF" ]; then
    echo -e "${RED}Error: ARM64 toolchain not found${NC}"
    exit 1
fi

# 1. Check dynamic dependencies
echo -e "${YELLOW}[1/6] Dynamic Dependencies:${NC}"
NEEDED=$($READELF -d "$TA_FILE" 2>/dev/null | grep NEEDED || true)
if [ -z "$NEEDED" ]; then
    echo -e "${GREEN}  ✅ No dynamic dependencies (fully static)${NC}"
else
    echo -e "${RED}  ❌ Found dynamic dependencies:${NC}"
    echo "$NEEDED"
fi

echo ""

# 2. Check C++ symbols
echo -e "${YELLOW}[2/6] C++ Standard Library Symbols:${NC}"
if [ -n "$NM" ]; then
    CPP_SYMBOLS=$($NM -C "$TA_FILE" 2>/dev/null | grep "std::" | head -10 || true)
    if [ -n "$CPP_SYMBOLS" ]; then
        echo -e "${GREEN}  ✅ Found C++ symbols (libstdc++ linked):${NC}"
        echo "$CPP_SYMBOLS" | sed 's/^/    /'
    else
        echo -e "${YELLOW}  ⚠️  No C++ STL symbols found${NC}"
        echo "    (Using minimal C++ or no STL)"
    fi
else
    echo -e "${YELLOW}  ⚠️  nm tool not found, skipping${NC}"
fi

echo ""

# 3. Check new/delete operators
echo -e "${YELLOW}[3/6] C++ Memory Operators:${NC}"
if [ -n "$NM" ]; then
    NEW_DELETE=$($NM "$TA_FILE" 2>/dev/null | grep -E "operator new|operator delete" | wc -l)
    if [ "$NEW_DELETE" -gt 0 ]; then
        echo -e "${GREEN}  ✅ Found $NEW_DELETE new/delete operators${NC}"
    else
        echo -e "${YELLOW}  ⚠️  No new/delete operators (no dynamic memory)${NC}"
    fi
else
    echo -e "${YELLOW}  ⚠️  nm tool not found, skipping${NC}"
fi

echo ""

# 4. File size
echo -e "${YELLOW}[4/6] Binary Size:${NC}"
SIZE=$(ls -lh "$TA_FILE" | awk '{print $5}')
SIZE_BYTES=$(stat -c%s "$TA_FILE" 2>/dev/null || stat -f%z "$TA_FILE" 2>/dev/null)
echo "  Size: $SIZE ($SIZE_BYTES bytes)"

if [ "$SIZE_BYTES" -lt 100000 ]; then
    echo -e "${GREEN}  ✅ Small size (minimal dependencies)${NC}"
elif [ "$SIZE_BYTES" -lt 500000 ]; then
    echo -e "${YELLOW}  ⚠️  Medium size (some static libraries)${NC}"
else
    echo -e "${BLUE}  ℹ️  Large size (full static linking)${NC}"
fi

echo ""

# 5. Sections
echo -e "${YELLOW}[5/6] Binary Sections:${NC}"
if [ -n "$READELF" ]; then
    echo "  Section     Size"
    echo "  ---------   --------"
    $READELF -S "$TA_FILE" 2>/dev/null | grep -E "\.text|\.data|\.rodata|\.bss" | awk '{printf "  %-11s %s\n", $2, $6}' || true
fi

echo ""

# 6. Entry points
echo -e "${YELLOW}[6/6] TA Entry Points:${NC}"
if [ -n "$NM" ]; then
    ENTRY_POINTS=$($NM "$TA_FILE" 2>/dev/null | grep -E "TA_CreateEntryPoint|TA_OpenSessionEntryPoint|TA_InvokeCommandEntryPoint" || true)
    if [ -n "$ENTRY_POINTS" ]; then
        echo -e "${GREEN}  ✅ Found TA entry points:${NC}"
        echo "$ENTRY_POINTS" | awk '{print "    " $3}'
    else
        echo -e "${RED}  ❌ No TA entry points found!${NC}"
    fi
else
    echo -e "${YELLOW}  ⚠️  nm tool not found, skipping${NC}"
fi

echo ""
echo -e "${BLUE}╔═══════════════════════════════════════════╗"
echo "║   Analysis Complete                       ║"
echo "╚═══════════════════════════════════════════╝${NC}"

# Summary
echo ""
echo -e "${YELLOW}Summary:${NC}"

if [ -z "$NEEDED" ]; then
    echo -e "${GREEN}  ✅ Binary is properly statically linked${NC}"
else
    echo -e "${RED}  ❌ Binary has dynamic dependencies${NC}"
fi

if [ -n "$CPP_SYMBOLS" ]; then
    echo -e "${GREEN}  ✅ C++ support enabled${NC}"
else
    echo -e "${YELLOW}  ⚠️  Minimal C++ (no STL)${NC}"
fi

echo ""
