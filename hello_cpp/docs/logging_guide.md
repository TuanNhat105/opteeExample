# OP-TEE TA Logging trong Buildroot

## Vấn Đề: DMSG() Không Hiển Thị

Khi chạy `hello_cpp_host` trên Raspberry Pi 5 với Buildroot, bạn thấy:
- ✅ Output từ host app (Normal World) 
- ❌ Không thấy DMSG() logs từ TA (Secure World)

## Tại Sao?

### Kiến Trúc OP-TEE Logging

```
┌─────────────────────────────────────┐
│  Secure World (EL3)                 │
│  ┌──────────────────────────────┐   │
│  │  Trusted Application (TA)    │   │
│  │  - DMSG("debug message")     │───┼──▶ Secure UART
│  │  - IMSG("info message")      │───┼──▶ (không ra Normal World!)
│  │  - EMSG("error message")     │───┘
│  └──────────────────────────────┘   │
└─────────────────────────────────────┘
           │
           │ (OP-TEE Core không forward logs mặc định)
           │
           ▼
┌─────────────────────────────────────┐
│  Normal World (EL1)                 │
│  ┌──────────────────────────────┐   │
│  │  Host Application            │   │
│  │  - printf("normal world")    │───▶ stdout ✅
│  │  - TEEC_InvokeCommand()      │   │
│  └──────────────────────────────┘   │
└─────────────────────────────────────┘
```

### Lý Do DMSG Không Hiển Thị

1. **Security Isolation:**
   - Secure World logs KHÔNG được tự động gửi tới Normal World
   - Đây là security feature để prevent information leakage

2. **Buildroot Minimal Config:**
   - Buildroot của bạn có minimal OP-TEE configuration
   - Log output không được enable hoặc forward

3. **No Debug Console:**
   - DMSG()/IMSG() output đi vào secure console
   - Secure console thường là UART debug port (hardware)
   - Không có software interface để đọc

## Cách Xem TA Logs

### ❌ Các Phương Pháp KHÔNG Hoạt Động (trên Buildroot minimal)

```bash
# Không có logs ở đây:
dmesg | grep optee          # Kernel log
cat /var/log/messages       # Syslog
journalctl                  # Systemd (không có trên Buildroot)
```

### ✅ Option 1: UART Debug Console (Hardware)

**Cần:**
- USB-to-UART cable (3.3V)
- Connect tới Pi 5 debug header
- Minicom/screen trên host PC

**Setup:**
```bash
# Trên development PC
sudo minicom -D /dev/ttyUSB0 -b 115200

# Sẽ thấy:
D/TC:? 0 TA_OpenSessionEntryPoint (C++)
D/TC:? 0 Hello from C++ Trusted Application!
D/TC:? 0 === Testing Basic C++ Features ===
```

**Ưu điểm:** Thấy đầy đủ DMSG/IMSG logs  
**Nhược điểm:** Cần hardware, không tiện

### ✅ Option 2: Return Debug Info qua Output Buffer (Recommended)

Modify TA để return debug info qua TEEC_Operation:

**TA Code:**
```cpp
TEE_Result test_basic_cpp() {
    // Thay vì chỉ DMSG:
    DMSG("=== Testing Basic C++ Features ===");
    
    // Có thể return status string (nếu cần)
    return TEE_SUCCESS;
}

// Better: Add debug output parameter
TEE_Result test_with_debug(char* debug_buf, size_t* debug_size) {
    const char* msg = "C++ test passed!\n";
    size_t len = strlen(msg);
    
    if (*debug_size >= len) {
        memcpy(debug_buf, msg, len);
        *debug_size = len;
    }
    
    DMSG("%s", msg);  // Vẫn log trong TA
    return TEE_SUCCESS;
}
```

**Host Code:**
```c
char debug_output[256] = {0};
TEEC_Operation op = {0};
op.params[1].tmpref.buffer = debug_output;
op.params[1].tmpref.size = sizeof(debug_output);
op.paramTypes = TEEC_PARAM_TYPES(
    TEEC_VALUE_INPUT,
    TEEC_MEMREF_TEMP_OUTPUT,  // Debug output
    TEEC_NONE, TEEC_NONE);

res = TEEC_InvokeCommand(&sess, cmd_id, &op, &err_origin);

// Print debug from TA
if (op.params[1].tmpref.size > 0) {
    printf("[TA DEBUG] %s", debug_output);
}
```

### ✅ Option 3: Shared Memory Logging

Tạo circular buffer trong shared memory cho TA logs:

```c
// Host allocates shared memory
TEEC_SharedMemory log_shm;
log_shm.size = 4096;
log_shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
TEEC_AllocateSharedMemory(&ctx, &log_shm);

// TA writes logs to shared memory
void ta_log(const char* msg) {
    // Write to shared memory circular buffer
    // (implementation needed)
}

// Host reads logs
printf("TA Logs:\n%s", (char*)log_shm.buffer);
```

### ✅ Option 4: Rebuild OP-TEE với Full Debug

**Rebuild OP-TEE Core:**
```bash
cd /home/abc/optee_os

# Edit configuration
make PLATFORM=rpi5 \
     CFG_TEE_CORE_LOG_LEVEL=4 \
     CFG_TEE_CORE_DEBUG=y \
     CFG_TEE_CORE_TA_TRACE=y
```

**Nhược điểm:** Phải rebuild toàn bộ OP-TEE

## Hiện Tại: TA Đang Hoạt Động Tốt!

Mặc dù không thấy DMSG logs, TA vẫn chạy perfectly:

```bash
root@opteepi:~# hello_cpp_host
====================================
  Hello C++ Trusted Application
  Testing C++ in OP-TEE
====================================

=== Test 1: Basic C++ Features ===
✅ Test PASSED: Basic C++ (string, concatenation)

=== Test 2: STL Containers ===
✅ Test PASSED: STL (vector, find, algorithms)

=== Test 3: C++ Class ===
✅ Test PASSED: C++ Class (OOP, constructor, methods)

=== Test 4: Data Processing ===
Input:  Hello from Normal World!
Output: HELLO FROM NORMAL WORLD!
✅ Test PASSED: Data processing (uppercase conversion)
```

**Chứng minh:**
- ✅ TA được load thành công
- ✅ Session được mở
- ✅ Commands được invoke 
- ✅ Data processing hoạt động đúng
- ✅ C++ classes, constructors, methods working!

## Khuyến Nghị

**Cho Development:**
- Sử dụng host app output để verify TA behavior
- Test results đủ để confirm TA logic
- DMSG() vẫn useful cho debugging khi có UART

**Cho Production:**
- TA không nên print sensitive logs anyway (security)
- Return error codes và status qua TEEC_Operation
- Use EMSG() chỉ cho critical errors

**Cho eEVM Integration:**
- Design API với clear input/output buffers
- Use shared memory cho large data transfers
- Implement proper error handling với return codes

## Quick Test Script

Chạy script này trên Pi để verify:

```bash
#!/bin/bash
# test_ta.sh

echo "Testing C++ TA..."
hello_cpp_host

echo ""
echo "TA Status:"
ls -lh /lib/optee_armtz/f4e750bb-1437-4fbf-8785-8d3580c34994.ta

echo ""
echo "TEE Supplicant:"
ps | grep tee-supplicant

echo ""
echo "Note: DMSG logs are in Secure World console"
echo "Application works correctly even without visible DMSG output!"
```

## Tóm Tắt

| Logging Method | Visibility | Use Case |
|----------------|------------|----------|
| DMSG() | ❌ Not visible (Secure UART only) | TA internal debugging |
| IMSG() | ❌ Not visible | TA info messages |
| EMSG() | ❌ Not visible | TA errors |
| printf() in Host | ✅ Visible | Normal World output |
| Output Buffer | ✅ Visible | Pass debug info from TA |
| Shared Memory | ✅ Visible | Large debug data |

**Kết luận:** TA của bạn hoạt động hoàn hảo! DMSG không hiển thị là behavior bình thường trên Buildroot với minimal OP-TEE config. Focus vào test results thay vì debug logs.
