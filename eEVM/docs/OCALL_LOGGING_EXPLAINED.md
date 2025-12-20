# OCALL Logging - Đưa Log từ Secure World ra Normal World

## 📋 Mục lục
1. [Tổng quan](#tổng-quan)
2. [Vấn đề cần giải quyết](#vấn-đề-cần-giải-quyết)
3. [Kiến trúc OP-TEE](#kiến-trúc-op-tee)
4. [Cơ chế OCALL Logging](#cơ-chế-ocall-logging)
5. [Chi tiết triển khai](#chi-tiết-triển-khai)
6. [Workflow hoàn chỉnh](#workflow-hoàn-chỉnh)
7. [So sánh với các phương pháp khác](#so-sánh-với-các-phương-pháp-khác)
8. [Best Practices](#best-practices)

---

## 🎯 Tổng quan

**OCALL (Outside Call)** là một kỹ thuật cho phép code chạy trong **Secure World** (Trusted Execution Environment - TEE) gọi ra **Normal World** để thực hiện các tác vụ như logging, I/O, hoặc các operations không thể thực hiện trực tiếp trong TEE.

Trong project này, chúng ta sử dụng OCALL để **đưa debug logs từ TA (Trusted Application) trong Secure World ra Host Application trong Normal World** để có thể xem và debug dễ dàng.

---

## ❓ Vấn đề cần giải quyết

### Tại sao không dùng printf() trực tiếp?

Khi code chạy trong **Secure World (OP-TEE TA)**:

```cpp
// ❌ KHÔNG HOẠT ĐỘNG như mong đợi
printf("Debug info: %d\n", value);  
std::cout << "Debug info" << std::endl;
```

**Vấn đề:**
1. **Không có stdout/stderr**: Secure World không có console trực tiếp
2. **Logs bị ẩn**: Output chỉ xuất hiện trong kernel logs (`dmesg`), không hiển thị cho user
3. **Khó debug**: Phải SSH vào device, chạy `dmesg | grep TA` để xem logs
4. **Không real-time**: Logs bị buffer, không thấy ngay lập tức
5. **Format khó đọc**: Logs kernel logs lẫn với hàng nghìn dòng khác

### Giải pháp: OCALL Logging

Thay vì printf, ta dùng:

```cpp
// ✅ HOẠT ĐỘNG HOÀN HẢO
OCALL_LOG("Debug info: %d", value);
```

**Lợi ích:**
- ✅ Logs hiển thị **real-time** trên console của Host Application
- ✅ **Không cần SSH** hay `dmesg`, chạy ngay trên máy dev
- ✅ Format **đẹp, dễ đọc**, có thể customize
- ✅ **An toàn**, không leak thông tin nhạy cảm qua kernel logs

---

## 🏗️ Kiến trúc OP-TEE

### ARM TrustZone Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Normal World (REE)                    │
│  ┌────────────────────────────────────────────────────┐ │
│  │          Host Application (CA)                     │ │
│  │  - User code (C/C++)                               │ │
│  │  - Gọi TEEC_InvokeCommand()                        │ │
│  │  - Nhận logs từ TA qua params                      │ │
│  └────────────────────────────────────────────────────┘ │
│                         ⬇️ ⬆️                             │
│  ┌────────────────────────────────────────────────────┐ │
│  │          Linux Kernel + TEE Driver                 │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
                          ⬇️ ⬆️  (SMC - Secure Monitor Call)
┌─────────────────────────────────────────────────────────┐
│                    Secure World (TEE)                    │
│  ┌────────────────────────────────────────────────────┐ │
│  │              OP-TEE OS                             │ │
│  └────────────────────────────────────────────────────┘ │
│                         ⬇️ ⬆️                             │
│  ┌────────────────────────────────────────────────────┐ │
│  │    Trusted Application (TA) - eEVM                 │ │
│  │  - Secure code (C/C++)                             │ │
│  │  - OCALL_LOG() → ghi vào buffer                    │ │
│  │  - Return buffer qua params về CA                  │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Ranh giới bảo mật

- **Secure World**: Code chạy với highest privilege, bảo vệ khỏi Normal World
- **Normal World**: Linux, applications, có thể bị tấn công
- **Communication**: Chỉ qua **shared memory** được OP-TEE OS quản lý

---

## 🔧 Cơ chế OCALL Logging

### 1. Khái niệm cơ bản

```
┌──────────────────────────────────────────────────────────┐
│ OCALL = Outside Call                                     │
│                                                          │
│ TA (Secure) cần truyền data về CA (Normal)              │
│ → Dùng shared memory qua TEE params                      │
└──────────────────────────────────────────────────────────┘
```

### 2. Tại sao gọi là "OCALL"?

Thuật ngữ **OCALL** xuất phát từ **SGX (Intel Software Guard Extensions)**:

- **ECALL (Enclave Call)**: Normal World → Secure World
- **OCALL (Outside Call)**: Secure World → Normal World

Trong OP-TEE:
- **ECALL** ≈ `TEEC_InvokeCommand()` (CA gọi vào TA)
- **OCALL** ≈ **TA return data qua params về CA**

### 3. Shared Memory Communication

```
┌─────────────────────────────────────────────────────────┐
│              TEEC_Operation (Shared Memory)             │
├─────────────────────────────────────────────────────────┤
│  param[0]:  INPUT/OUTPUT - Value hoặc MemRef           │
│  param[1]:  INPUT/OUTPUT - Value hoặc MemRef           │
│  param[2]:  OUTPUT       - MemRef cho OCALL logs  ⭐    │
│  param[3]:  INPUT/OUTPUT - Value hoặc MemRef           │
└─────────────────────────────────────────────────────────┘
         ⬆️                                    ⬇️
    CA allocates                        TA writes
    buffer và pass                      logs vào buffer
    vào TA                              và return về CA
```

---

## 💻 Chi tiết triển khai

### Bước 1: Cấu trúc dữ liệu (ocall_logger.cpp)

```cpp
// Buffer tĩnh 8KB trong TA để lưu logs tạm thời
#define OCALL_BUFFER_SIZE 8192
static char g_ocall_buffer[OCALL_BUFFER_SIZE];
static size_t g_ocall_pos = 0;  // Vị trí hiện tại
```

**Giải thích:**
- `g_ocall_buffer`: Buffer **nội bộ** trong TA, không thể truy cập từ Normal World
- `g_ocall_pos`: Track vị trí ghi tiếp theo (như con trỏ)
- **Tĩnh (static)**: Tồn tại suốt lifetime của TA, không bị mất khi return

### Bước 2: Khởi tạo (ocall_log_init)

```cpp
void ocall_log_init(void) {
    g_ocall_pos = 0;
    g_ocall_buffer[0] = '\0';
}
```

**Khi nào gọi?**
- Đầu mỗi **Command** (mỗi lần CA invoke TA)
- Xóa logs cũ, chuẩn bị cho logs mới

**Tại sao cần?**
- Tránh logs từ command trước bị lẫn với command hiện tại
- Reset state về clean slate

### Bước 3: Ghi log (ocall_log_print)

```cpp
void ocall_log_print(const char* fmt, ...) {
    char tmp[512];              // ⬅️ Buffer tạm cho 1 dòng log
    va_list args;
    va_start(args, fmt);
    
    // Format string với prefix [TA]
    int header_len = snprintf(tmp, sizeof(tmp), "[TA] ");
    int msg_len = vsnprintf(tmp + header_len, sizeof(tmp) - header_len - 2, fmt, args);
    va_end(args);

    if (msg_len > 0) {
        strcat(tmp, "\n");      // ⬅️ Tự động xuống dòng
        size_t total_len = strlen(tmp);
        
        // Append vào g_ocall_buffer nếu còn chỗ
        if (g_ocall_pos + total_len < OCALL_BUFFER_SIZE) {
            memcpy(g_ocall_buffer + g_ocall_pos, tmp, total_len);
            g_ocall_pos += total_len;
            g_ocall_buffer[g_ocall_pos] = '\0';  // ⬅️ Null-terminate
        }
        // Nếu buffer đầy → logs bị drop (silent fail)
    }
}
```

**Flow:**
1. Format message vào buffer tạm `tmp` (như sprintf)
2. Thêm prefix `[TA]` để dễ phân biệt
3. Thêm newline `\n` tự động
4. **Append** vào `g_ocall_buffer` (không ghi đè)
5. Update `g_ocall_pos` để track vị trí

**Tại sao dùng variadic args (...)?**
- Để support format string như printf: `OCALL_LOG("Value: %d", x)`

### Bước 4: Macro tiện lợi (ocall_logger.h)

```cpp
#define OCALL_LOG(fmt, ...) ocall_log_print(fmt, ##__VA_ARGS__)
```

**Tại sao cần macro?**
- Syntax đẹp hơn: `OCALL_LOG("msg")` thay vì `ocall_log_print("msg")`
- `##__VA_ARGS__`: GCC extension để handle trường hợp không có args

**Sử dụng:**
```cpp
OCALL_LOG("Starting test");              // ⬅️ Không có args
OCALL_LOG("Value: %d", 42);             // ⬅️ Có args
OCALL_LOG("Sum: %d + %d = %d", a, b, c); // ⬅️ Nhiều args
```

### Bước 5: Flush logs về CA (ocall_log_flush_to_params)

```cpp
void ocall_log_flush_to_params(void* dest_buffer, size_t* dest_size) {
    if (!dest_buffer || !dest_size || *dest_size == 0) return;

    size_t copy_size = g_ocall_pos;
    
    // Truncate nếu buffer CA nhỏ hơn logs
    if (copy_size >= *dest_size) {
        copy_size = *dest_size - 1;  // ⬅️ Để chỗ cho null terminator
    }
    
    // Copy từ TA buffer → Shared memory
    memcpy(dest_buffer, g_ocall_buffer, copy_size);
    ((char*)dest_buffer)[copy_size] = '\0';  // ⬅️ Null-terminate
    *dest_size = copy_size + 1;              // ⬅️ Update actual size
}
```

**Flow:**
1. Kiểm tra validity của buffer CA
2. Tính size cần copy (min của buffer size và logs size)
3. **memcpy** từ `g_ocall_buffer` → `dest_buffer` (shared memory)
4. Null-terminate để CA có thể dùng như C string
5. Update `dest_size` để CA biết có bao nhiêu bytes

**Quan trọng:**
- `dest_buffer` là **shared memory** được OP-TEE OS map vào address space của cả CA và TA
- Sau khi return từ TA, CA có thể đọc data từ `dest_buffer`

### Bước 6: Sử dụng trong TA (eevm_ta_main.cpp)

```cpp
static TEE_Result eevm_hello_world(uint32_t param_types, TEE_Param params[4])
{
    // 1️⃣ Khởi tạo logging
    ocall_log_init();
    OCALL_LOG("=== Starting eEVM Hello World Test ===");

    // 2️⃣ Validate parameters
    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_OUTPUT,    // params[0]: exit reason
        TEE_PARAM_TYPE_VALUE_OUTPUT,    // params[1]: result size
        TEE_PARAM_TYPE_MEMREF_OUTPUT,   // params[2]: OCALL log buffer ⭐
        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        // 3️⃣ Flush logs ngay cả khi error
        if (params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(
                params[2].memref.buffer, 
                &params[2].memref.size);
        }
        return TEE_ERROR_BAD_PARAMETERS;
    }

    try {
        // 4️⃣ Logic chính với nhiều logs
        OCALL_LOG("[Step 1] Creating addresses...");
        // ... code ...
        
        OCALL_LOG("[Step 2] Creating global state...");
        // ... code ...
        
        OCALL_LOG("[Step 7] Executing EVM bytecode...");
        const eevm::ExecResult e = p.run(tx, sender, contract, {}, 0, nullptr);
        
        OCALL_LOG("[INFO] Execution completed!");
        OCALL_LOG("[INFO] Exit reason: %d", (int)e.er);
        
        // 5️⃣ Kiểm tra kết quả
        if (response == hello_world) {
            OCALL_LOG("[PASS] ✓ eEVM execution successful!");
        } else {
            OCALL_LOG("[FAIL] Result mismatch!");
        }

        // 6️⃣ Flush tất cả logs về CA trước khi return
        if (params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(
                params[2].memref.buffer, 
                &params[2].memref.size);
        }

        return TEE_SUCCESS;
        
    } catch (const std::exception& ex) {
        // 7️⃣ Catch exception và log
        OCALL_LOG("[FATAL] C++ exception: %s", ex.what());
        
        // 8️⃣ Flush logs ngay cả khi có exception
        if (params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(
                params[2].memref.buffer, 
                &params[2].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}
```

**Key points:**
- `params[2]` luôn là MEMREF_OUTPUT cho logs
- Gọi `ocall_log_flush_to_params()` **trước mọi return statement**
- Logs được giữ trong buffer cho đến khi flush

### Bước 7: Host Application nhận logs (main.cpp)

```cpp
int main() {
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    
    // 1️⃣ Allocate buffer cho logs (8KB)
    char log_buffer[8192] = {0};
    
    // 2️⃣ Setup params cho InvokeCommand
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_OUTPUT,            // params[0]
        TEEC_VALUE_OUTPUT,            // params[1]
        TEEC_MEMREF_TEMP_OUTPUT,      // params[2]: Log buffer ⭐
        TEEC_NONE);
    
    op.params[2].tmpref.buffer = log_buffer;     // ⬅️ Pointer
    op.params[2].tmpref.size = sizeof(log_buffer); // ⬅️ Max size
    
    // 3️⃣ Invoke TA command
    std::cout << "Invoking TA_EEVM_CMD_HELLO_WORLD..." << std::endl;
    TEEC_Result res = TEEC_InvokeCommand(
        &sess, 
        TA_EEVM_CMD_HELLO_WORLD, 
        &op, 
        &err_origin);
    
    // 4️⃣ TA đã return, logs đã được copy vào log_buffer
    // Print logs ra console
    if (log_buffer[0] != '\0') {
        std::cout << "\n=== OCALL Logs from eEVM Hello World ===" << std::endl;
        std::cout << log_buffer;  // ⬅️ Print toàn bộ logs
        std::cout << "=== End of OCALL Logs ===" << std::endl;
    }
    
    // 5️⃣ Kiểm tra result
    if (res == TEEC_SUCCESS) {
        std::cout << "✓ Test passed!" << std::endl;
        std::cout << "Exit reason: " << op.params[0].value.a << std::endl;
        std::cout << "Result size: " << op.params[1].value.a << std::endl;
    } else {
        std::cerr << "❌ Test failed: 0x" << std::hex << res << std::endl;
    }
    
    return 0;
}
```

**Flow:**
1. CA allocate buffer 8KB trên stack
2. Pass pointer và size vào `op.params[2]`
3. Gọi `TEEC_InvokeCommand()` → **OP-TEE OS map buffer vào TA**
4. TA ghi logs vào buffer
5. TA return
6. **OP-TEE OS unmap buffer**
7. CA đọc logs từ buffer và print ra console

---

## 🔄 Workflow hoàn chỉnh

### Timeline chi tiết

```
Time │ Normal World (CA)           │ Secure World (TA)
─────┼─────────────────────────────┼──────────────────────────────
  1  │ Allocate log_buffer[8KB]    │
  2  │ Setup op.params[2]          │
  3  │ TEEC_InvokeCommand() ─────→ │
     │                             │
  4  │                             │ ← TA Entry Point
  5  │                             │   ocall_log_init()
  6  │                             │   OCALL_LOG("Step 1") → g_ocall_buffer
  7  │                             │   OCALL_LOG("Step 2") → g_ocall_buffer
  8  │                             │   ... (nhiều logs) ...
  9  │                             │   OCALL_LOG("Step N") → g_ocall_buffer
 10  │                             │   
 11  │                             │   ocall_log_flush_to_params()
 12  │                             │     memcpy(params[2].buffer, g_ocall_buffer)
 13  │                             │   
 14  │                             │   return TEE_SUCCESS
 15  │ ←───────────────────────────┤
 16  │ Read log_buffer             │
 17  │ Print logs to console       │
 18  │ Done                        │
```

### Memory Flow

```
┌──────────────────────────────────────────────────────────────┐
│                      Normal World                            │
│                                                              │
│  Stack:                                                      │
│    char log_buffer[8192] = "........logs từ TA........"     │
│                              ⬆️                              │
│                              │ Copy data                     │
└──────────────────────────────┼──────────────────────────────┘
                               │
                    OP-TEE OS maps shared memory
                               │
┌──────────────────────────────┼──────────────────────────────┐
│                              ⬇️                              │
│                      Secure World                            │
│                                                              │
│  params[2].memref.buffer ──→ (shared memory mapped here)    │
│                              ⬆️                              │
│                              │ memcpy                        │
│  Static:                     │                               │
│    g_ocall_buffer[8192] = "[TA] Step 1\n[TA] Step 2\n..."   │
│                              ⬆️                              │
│                              │ Append logs                   │
│    OCALL_LOG() ──────────────┘                               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 📊 So sánh với các phương pháp khác

### 1. DMSG (OP-TEE Debug Messages)

```cpp
// TA code
DMSG("Debug message: %d", value);
```

**Ưu điểm:**
- ✅ Built-in OP-TEE, không cần code thêm
- ✅ Đơn giản, dễ dùng

**Nhược điểm:**
- ❌ Logs vào **kernel log** (`dmesg`), không hiện trên console CA
- ❌ Cần SSH vào device và chạy `dmesg | grep TA`
- ❌ Bị buffer, không real-time
- ❌ Khó đọc, lẫn với hàng nghìn dòng kernel logs khác
- ❌ Production builds thường disable DMSG

**Khi nào dùng:** Debug OP-TEE kernel-level issues

### 2. OCALL Logging (Implementation này)

```cpp
// TA code
OCALL_LOG("Debug message: %d", value);

// CA code - Ngay lập tức thấy logs
std::cout << log_buffer;
```

**Ưu điểm:**
- ✅ **Real-time logs** trên console CA
- ✅ Không cần SSH hay dmesg
- ✅ Format đẹp, dễ customize
- ✅ Có thể save logs vào file
- ✅ Works trên mọi build config

**Nhược điểm:**
- ⚠️ Phải implement code (nhưng đã có sẵn)
- ⚠️ Overhead nhỏ (memcpy logs)
- ⚠️ Buffer size giới hạn (8KB)

**Khi nào dùng:** Production logging, user-facing diagnostics

### 3. Return values

```cpp
// TA code
params[0].value.a = error_code;
return TEE_SUCCESS;

// CA code
if (op.params[0].value.a != 0) {
    std::cerr << "Error: " << op.params[0].value.a << std::endl;
}
```

**Ưu điểm:**
- ✅ Hiệu quả nhất (chỉ 4 bytes)
- ✅ Standard TEE API

**Nhược điểm:**
- ❌ Chỉ return được **số**, không có context
- ❌ Không có detailed logs
- ❌ Khó debug issues phức tạp

**Khi nào dùng:** Simple error codes, performance critical

### 4. Shared memory file logging

```cpp
// TA code - Write logs to shared memory file
fwrite(log, size, 1, fp);

// CA code - Read file
FILE* fp = fopen("/tmp/ta_log.txt", "r");
```

**Ưu điểm:**
- ✅ Unlimited size
- ✅ Persistent logs

**Nhược điểm:**
- ❌ **Không thể dùng trong TA** (no file system access)
- ❌ Security risk (sensitive data leakage)
- ❌ Performance overhead

**Khi nào dùng:** Không nên dùng trong TA

---

## ✨ Best Practices

### 1. Buffer Management

```cpp
// ❌ BAD - Có thể overflow
for (int i = 0; i < 1000000; i++) {
    OCALL_LOG("Iteration %d", i);  // Buffer chỉ 8KB!
}

// ✅ GOOD - Log summary
OCALL_LOG("Starting 1M iterations...");
for (int i = 0; i < 1000000; i++) {
    // ... work ...
}
OCALL_LOG("Completed 1M iterations");
```

### 2. Error Handling

```cpp
// ✅ GOOD - Log errors trước khi return
if (error) {
    OCALL_LOG("[ERROR] Operation failed: %s", error_msg);
    
    // Flush ngay để không mất logs
    if (params[2].memref.buffer && params[2].memref.size > 0) {
        ocall_log_flush_to_params(
            params[2].memref.buffer, 
            &params[2].memref.size);
    }
    
    return TEE_ERROR_GENERIC;
}
```

### 3. Structured Logging

```cpp
// ✅ GOOD - Consistent format
OCALL_LOG("=== Starting Test ===");
OCALL_LOG("[Step 1] Initialization");
OCALL_LOG("[Step 2] Processing");
OCALL_LOG("[INFO] Result: %d", result);
OCALL_LOG("[PASS] Test completed");
OCALL_LOG("=== End Test ===");
```

### 4. Performance Considerations

```cpp
// ❌ BAD - Log trong tight loop
for (int i = 0; i < 1000000; i++) {
    OCALL_LOG("Processing %d", i);  // Chậm!
}

// ✅ GOOD - Log theo batch
for (int i = 0; i < 1000000; i++) {
    // ... work ...
    if (i % 100000 == 0) {
        OCALL_LOG("Progress: %d%%", i * 100 / 1000000);
    }
}
```

### 5. Security

```cpp
// ❌ BAD - Log sensitive data
OCALL_LOG("Private key: %s", private_key);  // NEVER!
OCALL_LOG("Password: %s", password);        // NEVER!

// ✅ GOOD - Log safe info only
OCALL_LOG("Key derivation successful");
OCALL_LOG("Authentication result: %s", success ? "OK" : "FAIL");
```

### 6. Multi-Command Logging

```cpp
// ✅ GOOD - Init ở đầu mỗi command
TEE_Result TA_InvokeCommandEntryPoint(...) {
    switch (cmd_id) {
        case CMD_TEST_1:
            ocall_log_init();  // ⬅️ Reset logs
            return test_1(param_types, params);
            
        case CMD_TEST_2:
            ocall_log_init();  // ⬅️ Reset logs
            return test_2(param_types, params);
    }
}
```

---

## 🎓 Tổng kết

### OCALL Logging là gì?

Một kỹ thuật cho phép **Trusted Application (TA)** trong Secure World **gửi logs về Client Application (CA)** trong Normal World thông qua **shared memory**.

### Tại sao cần?

- Printf trong TA không hiển thị trên console
- Kernel logs (dmesg) khó truy cập và đọc
- Cần real-time debugging cho development

### Cách hoạt động?

1. TA ghi logs vào **buffer nội bộ** (`g_ocall_buffer`)
2. Trước khi return, TA **copy buffer → shared memory** (`params[2]`)
3. CA đọc shared memory và **print ra console**

### So với printf?

| Feature          | printf/DMSG | OCALL Logging |
|------------------|-------------|---------------|
| Hiển thị console | ❌           | ✅             |
| Real-time        | ❌           | ✅             |
| Dễ đọc           | ❌           | ✅             |
| Production ready | ❌           | ✅             |

### Khi nào dùng?

- ✅ Development & debugging
- ✅ Production diagnostics  
- ✅ User-facing error messages
- ✅ Performance profiling

---

## 📚 Tài liệu tham khảo

1. [OP-TEE Documentation](https://optee.readthedocs.io/)
2. [TEE Client API Specification](https://globalplatform.org/specs-library/tee-client-api-specification/)
3. [ARM TrustZone Technology](https://developer.arm.com/ip-products/security-ip/trustzone)
4. [Intel SGX OCALL](https://software.intel.com/content/www/us/en/develop/documentation/sgx-developer-guide/top.html)

---

**Author**: eEVM OP-TEE Project  
**Date**: December 20, 2025  
**Version**: 1.0
