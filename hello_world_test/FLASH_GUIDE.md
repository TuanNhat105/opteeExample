# Hướng dẫn Flash OP-TEE OS lên Orange Pi 6 Plus

## Tình trạng hiện tại

✅ **OP-TEE OS đã build thành công** với default key:
- File: `/home/abc/nhat/orangepi6_secure/optee_os/out/tee.bin` (1.1MB)
- Key: `/home/abc/nhat/orangepi6_secure/optee_os/keys/default_ta.pem`

❌ **ARM Trusted Firmware chưa build được** do thiếu file platform:
- Thiếu: `plat/cix/common/aarch64/cix_bl2_mem_params_desc.c`
- Cần liên hệ vendor (CIX Computing) để lấy code base đầy đủ

## Vấn đề chính

Orange Pi 6 Plus sử dụng boot chain:
```
BL1 (ROM) → BL2 → BL31 (ATF) → BL32 (OP-TEE) → BL33 (U-Boot) → Linux
```

OP-TEE (BL32) được đóng gói trong **FIP (Firmware Image Package)** cùng với ATF.
Không thể flash riêng OP-TEE mà phải flash cả FIP.

## Giải pháp tạm thời

### Giải pháp 1: Sử dụng TA với key cũ (khuyến nghị)

Nếu OP-TEE OS trên device đã có key cũ, bạn có thể:

1. **Tìm key cũ** (nếu có backup):
```bash
# Key có thể ở:
# - Backup của build trước
# - Firmware package từ vendor
# - Git history
```

2. **Build TA với key cũ**:
```bash
cd /home/abc/nhat/optee_examples/hello_world_test
export TA_SIGN_KEY=/path/to/old_key.pem
./build.sh
./quick_deploy.sh 192.168.1.160
```

### Giải pháp 2: Disable signature verification (chỉ để test)

**CẢNH BÁO:** Không bảo mật, chỉ dùng để development!

Rebuild OP-TEE OS với signature verification disabled:
```bash
cd /home/abc/nhat/orangepi6_secure/optee_os
make PLATFORM=cix PLATFORM_FLAVOR=sky1 \
     CFG_ARM64_core=y \
     CFG_USER_TA_TARGETS=ta_arm64 \
     CFG_TA_ASLR=n \
     CFG_ENCRYPT_TA=n \
     DEBUG=1 \
     LDFLAGS="--no-warn-rwx-segments" \
     -j4
```

Sau đó cần flash (xem bên dưới).

### Giải pháp 3: Lấy firmware package đầy đủ từ vendor

Liên hệ CIX Computing hoặc Orange Pi để lấy:
- Full ATF source code
- Hoặc pre-built firmware với OP-TEE mới
- Hoặc flash tool chính thức

## Cách flash OP-TEE (khi có FIP hoàn chỉnh)

### Bước 1: Xác định partition layout

SSH vào Orange Pi 6 Plus:
```bash
ssh orangepi@192.168.1.160

# Kiểm tra partition table
sudo fdisk -l | grep -i "mmcblk\|sd"

# Hoặc
cat /proc/mtd

# Hoặc check device tree
ls -l /sys/firmware/devicetree/base/

# Tìm partition chứa ATF/TEE
# Thường tên: trust, tee, atf, fip, hoặc boot
sudo blkid | grep -i "trust\|tee\|atf\|fip"
```

Ví dụ output:
```
/dev/mmcblk0p1: LABEL="boot"
/dev/mmcblk0p2: LABEL="trust"    ← Có thể là partition này
/dev/mmcblk0p3: LABEL="rootfs"
```

### Bước 2: Backup partition hiện tại

**QUAN TRỌNG:** Backup trước khi flash!

```bash
# Giả sử partition là /dev/mmcblk0p2
sudo dd if=/dev/mmcblk0p2 of=/tmp/trust_backup.img bs=1M
scp orangepi@192.168.1.160:/tmp/trust_backup.img ~/orangepi_trust_backup.img
```

### Bước 3: Flash firmware mới

**Cách 1: Flash từ running Linux** (nếu biết đúng partition)

```bash
# Copy FIP lên device
scp /home/abc/nhat/orangepi6_secure/arm-trusted-firmware/build/sky1/debug/fip.bin \
    orangepi@192.168.1.160:/tmp/

# SSH và flash
ssh orangepi@192.168.1.160
sudo dd if=/tmp/fip.bin of=/dev/mmcblk0p2 bs=1M conv=fsync
sudo sync
sudo reboot
```

**Cách 2: Flash qua U-Boot** (an toàn hơn)

```bash
# 1. Setup TFTP server trên máy build
sudo apt install tftpd-hpa
sudo cp build/sky1/debug/fip.bin /srv/tftp/

# 2. Reboot Orange Pi và nhấn phím để vào U-Boot prompt

# 3. Trong U-Boot:
setenv serverip 192.168.1.100  # IP máy build
setenv ipaddr 192.168.1.160    # IP Orange Pi
tftp 0x40000000 fip.bin
mmc dev 0
mmc write 0x40000000 <partition_offset> <size_in_blocks>
# Ví dụ: mmc write 0x40000000 0x4000 0x2000
reset
```

**Cách 3: Flash qua SD Card**

```bash
# 1. Tạo SD card với firmware mới
sudo dd if=build/sky1/debug/fip.bin of=/dev/sdb2 bs=1M
sudo sync

# 2. Boot Orange Pi từ SD card
# 3. Copy từ SD sang eMMC (nếu cần)
```

### Bước 4: Verify sau khi flash

```bash
ssh orangepi@192.168.1.160

# Check OP-TEE logs
sudo dmesg | grep -i optee

# Nên thấy:
# [    0.123456] optee: probing for conduit method.
# [    0.123789] optee: revision 3.x detected
# [    0.124000] optee: initialized driver

# Check devices
ls -l /dev/tee*
# Phải thấy: /dev/tee0 và /dev/teepriv0

# Check version
cat /sys/devices/platform/optee/version 2>/dev/null
```

## Script tự động (khi biết partition)

```bash
#!/bin/bash
# flash_optee.sh

DEVICE_IP="192.168.1.160"
DEVICE_USER="orangepi"
FIP_FILE="/home/abc/nhat/orangepi6_secure/arm-trusted-firmware/build/sky1/debug/fip.bin"
PARTITION="/dev/mmcblk0p2"  # ĐIỀU CHỈNH CHO ĐÚNG!

# Backup
echo "Backing up partition..."
ssh ${DEVICE_USER}@${DEVICE_IP} "sudo dd if=${PARTITION} of=/tmp/backup.img bs=1M"
scp ${DEVICE_USER}@${DEVICE_IP}:/tmp/backup.img ~/orangepi_backup_$(date +%Y%m%d_%H%M%S).img

# Flash
echo "Flashing new firmware..."
scp ${FIP_FILE} ${DEVICE_USER}@${DEVICE_IP}:/tmp/fip.bin
ssh ${DEVICE_USER}@${DEVICE_IP} "sudo dd if=/tmp/fip.bin of=${PARTITION} bs=1M conv=fsync && sudo sync"

echo "Rebooting..."
ssh ${DEVICE_USER}@${DEVICE_IP} "sudo reboot"

echo "Wait 30s for reboot..."
sleep 30

echo "Verifying..."
ssh ${DEVICE_USER}@${DEVICE_IP} "sudo dmesg | grep -i optee"
```

## Lưu ý quan trọng

1. **Xác định đúng partition**: Flash sai có thể brick device!
2. **Luôn backup trước**: Để có thể restore nếu có vấn đề
3. **Serial console**: Nên có serial console để debug nếu boot fail
4. **Recovery method**: Chuẩn bị USB/SD card recovery trước khi flash

## Liên hệ hỗ trợ

- **Orange Pi Forum**: http://www.orangepi.org/orangepibbsen/
- **CIX Computing**: abel.shuai@cixcomputing.com (từ build script)
- **OP-TEE Forum**: https://github.com/OP-TEE/optee_os/discussions

## Tóm tắt

Hiện tại:
- ✅ OP-TEE OS đã build xong với key mới
- ❌ ATF chưa build được do thiếu file
- ⚠️ Cần firmware package đầy đủ từ vendor để flash

Khuyến nghị:
1. Liên hệ vendor để lấy full source code hoặc firmware tool
2. Hoặc tìm key cũ để build TA tương thích với OP-TEE hiện tại trên device
3. Hoặc sử dụng official firmware update tool từ Orange Pi

