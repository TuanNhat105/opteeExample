# Troubleshooting Guide - Hello World TA

## Lỗi: TEEC_ERROR_SECURITY (0xFFFF000F)

### Nguyên nhân

Lỗi này có nghĩa là **TA signature không hợp lệ** hoặc **không được trust**.

Điều này xảy ra khi:
1. TA được ký với một key
2. OP-TEE OS trên device được build với public key khác
3. OP-TEE OS verify signature của TA → thất bại → từ chối load

### Minh họa vấn đề

```
┌─────────────────────────────────────┐
│   Build Machine                      │
│   1. Build OP-TEE OS với key A       │
│      → Public key A embedded vào OS  │
│   2. Build TA với key B              │
│      → TA signed với private key B   │
└─────────────────────────────────────┘
              ↓ Deploy
┌─────────────────────────────────────┐
│   Orange Pi 6 Plus Device           │
│   - Running OLD OP-TEE OS (key A)   │
│   - Try to load NEW TA (key B)      │
│   → Signature mismatch!             │
│   → TEEC_ERROR_SECURITY             │
└─────────────────────────────────────┘
```

### Giải pháp

#### Giải pháp 1: Flash lại OP-TEE OS mới (khuyến nghị)

OP-TEE OS hiện tại trên device có thể được build với key cũ. Cần flash lại OP-TEE OS mới vừa build:

```bash
# 1. Xác định OP-TEE OS mới
cd /home/abc/nhat/orangepi6_secure/optee_os
ls -lh out/tee.bin
# File: out/tee.bin (567KB)

# 2. Flash lại OP-TEE OS
# Cách flash phụ thuộc vào Orange Pi 6 Plus boot process
# Thường là:
#   - Update boot partition
#   - Hoặc flash qua U-Boot
#   - Hoặc update firmware package

# Ví dụ (điều chỉnh theo board của bạn):
# sudo dd if=out/tee.bin of=/dev/mmcblk0pX bs=1M  # X là partition số
```

#### Giải pháp 2: Kiểm tra key đang dùng

```bash
# Kiểm tra key trong OP-TEE OS build
cd /home/abc/nhat/orangepi6_secure/optee_os
ls -l keys/default_ta.pem
openssl rsa -in keys/default_ta.pem -text -noout | head -5

# Kiểm tra key trong TA_DEV_KIT_DIR
ls -l out/arm-plat-cix/export-ta_arm64/keys/
```

#### Giải pháp 3: Build TA với key đúng (nếu có key cũ)

Nếu bạn có key cũ mà OP-TEE OS trên device được build:

```bash
cd /home/abc/nhat/optee_examples/hello_world_test
export TA_SIGN_KEY=/path/to/old_key.pem
./build.sh
```

#### Giải pháp 4: Disable signature verification (KHÔNG khuyến nghị cho production)

Chỉ dùng để test. Build OP-TEE OS với:

```bash
cd /home/abc/nhat/orangepi6_secure/optee_os
make ... CFG_ENCRYPT_TA=n CFG_TA_ASLR=n
```

### Kiểm tra vấn đề

Trên device, check dmesg để xem OP-TEE logs:

```bash
ssh orangepi@192.168.1.160
sudo dmesg | grep -i "optee\|signature\|security" | tail -20
```

Thông báo lỗi có thể là:
- "TA signature verification failed"
- "Security error loading TA"

## Lỗi: TEEC_ERROR_ITEM_NOT_FOUND (0xFFFF0008)

### Nguyên nhân

1. **User không có quyền truy cập /dev/tee0**
2. TA không được copy đúng vị trí
3. tee-supplicant không chạy

### Giải pháp

#### Fix permissions /dev/tee0

```bash
ssh orangepi@192.168.1.160

# Check permissions
ls -l /dev/tee*
# Nếu thấy: crw------- (chỉ root)

# Fix:
sudo chmod 666 /dev/tee0
sudo chmod 666 /dev/teepriv0

# Hoặc add user vào group tee:
sudo usermod -aG tee orangepi
# (cần logout và login lại)
```

#### Verify TA location

```bash
ls -l /lib/optee_armtz/8aaaf200-2450-11e4-abe2-0002a5d5c51b.ta
# Phải thấy file và có quyền đọc
```

#### Start tee-supplicant

```bash
pgrep tee-supplicant || sudo tee-supplicant &
```

## So sánh các lỗi

| Error Code | Name | Meaning | Common Cause |
|------------|------|---------|--------------|
| 0xFFFF0008 | TEEC_ERROR_ITEM_NOT_FOUND | Không tìm thấy | `/dev/tee0` không mở được, TA không tìm thấy |
| 0xFFFF000F | TEEC_ERROR_SECURITY | Lỗi bảo mật | **TA signature không hợp lệ, key mismatch** |
| 0xFFFF3072 | TEE_ERROR_SIGNATURE_INVALID | Chữ ký không hợp lệ | TA corrupted hoặc key sai |

## Khuyến nghị

Vì bạn đang gặp `TEEC_ERROR_SECURITY (0xFFFF000F)`:

1. **Flash lại OP-TEE OS mới build** (có public key khớp với key ký TA)
2. Hoặc kiểm tra xem OP-TEE OS trên device có phải version mới nhất không
3. Kiểm tra dmesg logs để xác định nguyên nhân cụ thể

Lỗi này khác với `TEEC_ERROR_ITEM_NOT_FOUND` (0xFFFF0008) - đó là lỗi không tìm thấy, còn 0xFFFF000F là lỗi chữ ký/bảo mật.

