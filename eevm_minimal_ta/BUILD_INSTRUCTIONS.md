# Hướng dẫn Build

## Cách 1: Build toàn bộ (Khuyến nghị)

```bash
cd /home/abc/nhat/optee_examples/eevm_minimal_ta
./build.sh
```

Script này sẽ tự động:
1. ✅ Build OpenEnclave libcxx + musl runtime (nếu chưa có)
2. ✅ Build Trusted Application (TA)
3. ✅ Build Host Application

**Output:**
- `ta/*.ta` - Trusted Application binary
- `host/minimal_evm_host` - Host application

---

## Cách 2: Build từng bước

### Bước 1: Build OpenEnclave libraries

```bash
cd /home/abc/nhat/optee_examples/eevm_minimal_ta
./build_openenclave_libs.sh
```

**Output:**
- `build_oe_libs/combined/libcxx_runtime.a` - Combined C++ runtime library
- `build_oe_libs/musl/include/` - Musl headers
- `build_oe_libs/libcxx/include/` - Libcxx headers

### Bước 2: Build Trusted Application

```bash
cd ta
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu- \
     TA_DEV_KIT_DIR=/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64
```

**Output:** `*.ta` file

### Bước 3: Build Host Application

```bash
cd ../host
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu- \
     TEEC_EXPORT=/home/abc/optee_client/out/export/usr
```

**Output:** `minimal_evm_host`

---

## Yêu cầu

1. **Toolchain:**
   - `aarch64-none-linux-gnu-gcc` trong PATH
   - Thường ở: `/home/abc/arm-toolchain/bin/`

2. **OP-TEE:**
   - `TA_DEV_KIT_DIR` - OP-TEE TA development kit
   - `TEEC_EXPORT` - OP-TEE client library

3. **OpenEnclave sources:**
   - `external/openenclave/3rdparty/musl/`
   - `external/openenclave/3rdparty/libcxx/`
   - `external/openenclave/3rdparty/libcxxrt/`

---

## Clean Build

```bash
# Clean tất cả
rm -rf build_oe_libs
cd ta && make clean && cd ..
cd host && make clean && cd ..

# Hoặc build lại từ đầu
./build.sh
```

---

## Troubleshooting

### Lỗi: "build_openenclave_libs.sh not found"
```bash
chmod +x build_openenclave_libs.sh
```

### Lỗi: "TA_DEV_KIT_DIR not found"
Kiểm tra đường dẫn OP-TEE:
```bash
export TA_DEV_KIT_DIR=/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64
```

### Lỗi: "CROSS_COMPILE not found"
Kiểm tra toolchain:
```bash
export PATH=/home/abc/arm-toolchain/bin:$PATH
which aarch64-none-linux-gnu-gcc
```

---

## Deploy và Run

Sau khi build xong, deploy lên Raspberry Pi 5:

```bash
# Copy TA
scp -O ta/*.ta root@192.168.1.203:/lib/optee_armtz/

# Copy Host
scp -O host/minimal_evm_host root@192.168.1.203:/usr/bin/

# Chạy trên Pi 5
ssh root@192.168.1.203
minimal_evm_host
```

