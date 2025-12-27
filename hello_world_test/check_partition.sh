#!/bin/bash

# Script to check partition layout on Orange Pi 6 Plus
# Helps identify where to flash OP-TEE/ATF firmware

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

DEVICE_USER="${DEVICE_USER:-orangepi}"
DEVICE_HOST="${DEVICE_HOST:-192.168.1.160}"

if [ -n "$1" ]; then
    DEVICE_HOST="$1"
fi

if [ -n "$2" ]; then
    DEVICE_USER="$2"
fi

echo -e "${GREEN}=== Checking Orange Pi 6 Plus Partition Layout ===${NC}"
echo "Device: ${DEVICE_USER}@${DEVICE_HOST}"
echo ""

# Check if device is reachable
if ! ping -c 1 -W 2 ${DEVICE_HOST} > /dev/null 2>&1; then
    echo -e "${RED}Error: Device ${DEVICE_HOST} is not reachable${NC}"
    exit 1
fi

echo -e "${YELLOW}[1/6] Block devices:${NC}"
ssh ${DEVICE_USER}@${DEVICE_HOST} "lsblk -o NAME,SIZE,TYPE,MOUNTPOINT" || true

echo ""
echo -e "${YELLOW}[2/6] Partition table (fdisk):${NC}"
ssh ${DEVICE_USER}@${DEVICE_HOST} "sudo fdisk -l 2>/dev/null | grep -E 'Disk /dev|Device|mmcblk|sd'" || true

echo ""
echo -e "${YELLOW}[3/6] Partition labels (blkid):${NC}"
ssh ${DEVICE_USER}@${DEVICE_HOST} "sudo blkid 2>/dev/null | grep -v loop" || true

echo ""
echo -e "${YELLOW}[4/6] MTD partitions (if using NAND/NOR):${NC}"
ssh ${DEVICE_USER}@${DEVICE_HOST} "cat /proc/mtd 2>/dev/null" || echo "No MTD devices found"

echo ""
echo -e "${YELLOW}[5/6] Device tree partitions:${NC}"
ssh ${DEVICE_USER}@${DEVICE_HOST} "ls -l /sys/firmware/devicetree/base/ 2>/dev/null | grep -i 'partition\|flash'" || echo "No DT partition info"

echo ""
echo -e "${YELLOW}[6/6] Searching for trust/tee/atf partitions:${NC}"
ssh ${DEVICE_USER}@${DEVICE_HOST} 'bash -s' << 'EOF'
# Search for partition names containing trust, tee, atf, fip
echo "Partition names:"
for dev in /dev/mmcblk* /dev/sd*; do
    if [ -b "$dev" ]; then
        label=$(sudo blkid -s LABEL -o value "$dev" 2>/dev/null)
        partlabel=$(sudo blkid -s PARTLABEL -o value "$dev" 2>/dev/null)
        if [ -n "$label" ]; then
            echo "  $dev: LABEL=$label"
        fi
        if [ -n "$partlabel" ]; then
            echo "  $dev: PARTLABEL=$partlabel"
        fi
    fi
done | grep -iE "trust|tee|atf|fip|boot|uboot" || echo "  No trust/tee/atf partitions found by name"

echo ""
echo "Checking common partition offsets:"
# Common offsets for Rockchip/Allwinner boards
# trust partition usually at offset 0x4000 (8MB) or 0x6000 (12MB)
for offset in 0x4000 0x6000 0x8000; do
    echo "  Offset $offset ($(($offset * 512 / 1024 / 1024))MB):"
    sudo dd if=/dev/mmcblk0 bs=512 skip=$offset count=1 2>/dev/null | strings | head -1 || true
done
EOF

echo ""
echo -e "${GREEN}=== Analysis ===${NC}"
echo ""
echo -e "${BLUE}Common partition layouts for Orange Pi:${NC}"
echo ""
echo "Rockchip RK3588 (Orange Pi 5/5+):"
echo "  /dev/mmcblk0p1 - uboot (loader)"
echo "  /dev/mmcblk0p2 - trust (ATF + OP-TEE)"
echo "  /dev/mmcblk0p3 - boot"
echo "  /dev/mmcblk0p4 - rootfs"
echo ""
echo "Allwinner H6/H616:"
echo "  Offset 8KB   - SPL"
echo "  Offset 128KB - U-Boot"
echo "  Offset 1MB   - ATF/OP-TEE"
echo "  Partition 1  - boot"
echo "  Partition 2  - rootfs"
echo ""
echo -e "${YELLOW}To flash firmware:${NC}"
echo "1. Identify the correct partition from output above"
echo "2. Backup: sudo dd if=/dev/mmcblkXpY of=/tmp/backup.img bs=1M"
echo "3. Flash: sudo dd if=fip.bin of=/dev/mmcblkXpY bs=1M conv=fsync"
echo "4. Sync and reboot: sudo sync && sudo reboot"
echo ""
echo -e "${RED}WARNING: Flashing wrong partition can brick your device!${NC}"
echo -e "${RED}Always backup before flashing!${NC}"

