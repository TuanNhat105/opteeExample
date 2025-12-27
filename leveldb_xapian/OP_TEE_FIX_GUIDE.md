# Hướng dẫn khắc phục lỗi OP-TEE Driver

## Vấn đề

Lỗi `TEEC_InitializeContext failed with code 0xffff0008` xảy ra khi:
- OP-TEE driver (`/dev/tee0`) không tồn tại
- OP-TEE kernel modules chưa được load
- `tee-supplicant` không chạy

## Giải pháp

### Cách 1: Tự động (Khuyến nghị)

Chạy script tự động từ máy build:

```bash
cd /home/abc/nhat/optee_examples/leveldb_xapian
./fix_optee.sh
```

Script này sẽ:
1. Kiểm tra trạng thái OP-TEE trên device
2. Copy script diagnostic lên device
3. Tự động load modules và khởi động tee-supplicant
4. Báo cáo kết quả

### Cách 2: Chạy trực tiếp trên device

**Bước 1:** Copy script lên device:
```bash
scp fix_optee_on_device.sh root@192.168.1.114:/tmp/
```

**Bước 2:** SSH vào device và chạy:
```bash
ssh root@192.168.1.114
bash /tmp/fix_optee_on_device.sh
```

### Cách 3: Thủ công

SSH vào device và chạy từng lệnh:

```bash
ssh root@192.168.1.114

# 1. Load OP-TEE kernel modules
modprobe optee
modprobe optee_rpc

# 2. Kiểm tra /dev/tee0
ls -l /dev/tee*

# 3. Tìm và khởi động tee-supplicant
find /usr /sbin /bin -name tee-supplicant

# 4. Khởi động tee-supplicant (thay /path/to bằng đường dẫn thực tế)
/usr/sbin/tee-supplicant &
# HOẶC
/usr/bin/tee-supplicant &

# 5. Kiểm tra
pgrep tee-supplicant
ls -l /dev/tee*
```

## Kiểm tra OP-TEE đã sẵn sàng

Sau khi fix, kiểm tra:

```bash
# Trên device
ls -l /dev/tee*          # Phải thấy /dev/tee0 và /dev/teepriv0
pgrep tee-supplicant     # Phải có PID

# Test từ máy build
ssh root@192.168.1.114 "test_simple_shm"
```

## Lưu ý

1. **Nếu device không có `systemctl`**: 
   - Device có thể là buildroot/embedded Linux
   - Cần chạy `tee-supplicant` trực tiếp (không qua systemctl)

2. **Nếu `modprobe` không tìm thấy modules**:
   - OP-TEE có thể được build vào kernel (không phải modules)
   - Kiểm tra: `grep -i optee /proc/config.gz | gunzip`

3. **Nếu không tìm thấy `tee-supplicant`**:
   - Cần build và install OP-TEE client
   - Hoặc copy binary từ build system

## Scripts có sẵn

- `check_optee.sh` - Kiểm tra trạng thái OP-TEE
- `fix_optee.sh` - Tự động fix (từ máy build)
- `fix_optee_on_device.sh` - Script chạy trực tiếp trên device

