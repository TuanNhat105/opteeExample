# OCALL Logging - Quick Reference

## TL;DR (Too Long; Didn't Read)

**Problem:** `printf()` trong OP-TEE TA không hiển thị trên console  
**Solution:** OCALL Logging - Đưa logs từ Secure World về Normal World qua shared memory

## Quick Example

### TA Code (Secure World)
```cpp
#include "ocall_logger.h"

static TEE_Result my_function(uint32_t param_types, TEE_Param params[4]) {
    // 1. Init logging
    ocall_log_init();
    
    // 2. Write logs (như printf)
    OCALL_LOG("Starting operation...");
    OCALL_LOG("Value: %d", 42);
    OCALL_LOG("Done!");
    
    // 3. Flush logs về CA trước khi return
    if (params[2].memref.buffer && params[2].memref.size > 0) {
        ocall_log_flush_to_params(
            params[2].memref.buffer, 
            &params[2].memref.size);
    }
    
    return TEE_SUCCESS;
}
```

### CA Code (Normal World)
```cpp
// 1. Allocate buffer
char log_buffer[8192] = {0};

// 2. Setup params
op.paramTypes = TEEC_PARAM_TYPES(
    TEEC_NONE,
    TEEC_NONE,
    TEEC_MEMREF_TEMP_OUTPUT,  // ⬅️ Param 2 cho logs
    TEEC_NONE);

op.params[2].tmpref.buffer = log_buffer;
op.params[2].tmpref.size = sizeof(log_buffer);

// 3. Invoke TA
TEEC_InvokeCommand(&sess, MY_CMD, &op, &err_origin);

// 4. Print logs
std::cout << log_buffer;  // ⬅️ Logs từ TA!
```

## How It Works (5 bước)

```
┌────────────────────────────────────────────────┐
│ 1. CA allocate buffer (8KB)                   │
│    char log_buffer[8192];                      │
└───────────────────┬────────────────────────────┘
                    │ Pass vào TA
┌───────────────────▼────────────────────────────┐
│ 2. TA ghi logs vào internal buffer            │
│    OCALL_LOG("Debug: %d", x);                  │
│    → g_ocall_buffer[] += "[TA] Debug: 42\n"   │
└───────────────────┬────────────────────────────┘
                    │
┌───────────────────▼────────────────────────────┐
│ 3. TA flush: copy buffer → shared memory      │
│    memcpy(params[2].buffer, g_ocall_buffer)    │
└───────────────────┬────────────────────────────┘
                    │ Return
┌───────────────────▼────────────────────────────┐
│ 4. CA đọc log_buffer                           │
│    std::cout << log_buffer;                    │
└────────────────────────────────────────────────┘
```

## API Reference

### TA Functions

| Function | Purpose | When to call |
|----------|---------|-------------|
| `ocall_log_init()` | Xóa logs cũ | Đầu mỗi command |
| `OCALL_LOG(fmt, ...)` | Ghi log (như printf) | Bất cứ lúc nào |
| `ocall_log_flush_to_params()` | Copy logs → shared memory | Trước mọi return |

### Usage Pattern

```cpp
static TEE_Result cmd_handler(uint32_t param_types, TEE_Param params[4]) {
    ocall_log_init();  // ⬅️ Step 1
    
    OCALL_LOG("Log 1");  // ⬅️ Step 2
    OCALL_LOG("Log 2");
    
    if (error) {
        OCALL_LOG("Error!");
        if (params[2].memref.buffer) {  // ⬅️ Step 3
            ocall_log_flush_to_params(
                params[2].memref.buffer, 
                &params[2].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
    
    OCALL_LOG("Success");
    if (params[2].memref.buffer) {  // ⬅️ Step 3
        ocall_log_flush_to_params(
            params[2].memref.buffer, 
            &params[2].memref.size);
    }
    return TEE_SUCCESS;
}
```

## Parameter Convention

**TA side:**
```cpp
params[0] - INPUT/OUTPUT - Custom data
params[1] - INPUT/OUTPUT - Custom data  
params[2] - OUTPUT       - OCALL log buffer ⭐
params[3] - INPUT/OUTPUT - Custom data
```

**CA side:**
```cpp
op.paramTypes = TEEC_PARAM_TYPES(
    ...,                          // params[0]
    ...,                          // params[1]
    TEEC_MEMREF_TEMP_OUTPUT,      // params[2] ⭐
    ...);                         // params[3]

op.params[2].tmpref.buffer = log_buffer;
op.params[2].tmpref.size = sizeof(log_buffer);
```

## Why Not printf()?

| Method | Where logs go | Access | Real-time |
|--------|---------------|--------|-----------|
| `printf()` | Kernel log (dmesg) | SSH + dmesg | ❌ No |
| `DMSG()` | Kernel log (dmesg) | SSH + dmesg | ❌ No |
| **`OCALL_LOG()`** | **CA console** | **Direct** | **✅ Yes** |

## Limitations

- ⚠️ Buffer size: 8KB (configurable)
- ⚠️ Logs > 8KB will be truncated
- ⚠️ Must flush before EVERY return statement
- ⚠️ Don't log in tight loops (performance)

## Tips

```cpp
// ✅ GOOD - Structured logs
OCALL_LOG("=== Test Start ===");
OCALL_LOG("[Step 1] Init");
OCALL_LOG("[Step 2] Process");
OCALL_LOG("[PASS] Success");

// ❌ BAD - Too many logs
for (int i = 0; i < 1000000; i++) {
    OCALL_LOG("Loop %d", i);  // Buffer overflow!
}

// ✅ GOOD - Batch logging
for (int i = 0; i < 1000000; i++) {
    if (i % 100000 == 0) {
        OCALL_LOG("Progress: %d%%", i/10000);
    }
}

// ❌ BAD - Forget to flush
OCALL_LOG("Important log");
return TEE_SUCCESS;  // Logs lost!

// ✅ GOOD - Always flush
OCALL_LOG("Important log");
if (params[2].memref.buffer) {
    ocall_log_flush_to_params(...);
}
return TEE_SUCCESS;
```

## Troubleshooting

### Không thấy logs?

**Check list:**
1. ✅ Đã gọi `ocall_log_init()` chưa?
2. ✅ Đã gọi `ocall_log_flush_to_params()` chưa?
3. ✅ `params[2]` có phải `MEMREF_OUTPUT` không?
4. ✅ CA có allocate `log_buffer` chưa?
5. ✅ CA có print `log_buffer` chưa?

### Logs bị cắt?

- Buffer chỉ 8KB
- Giảm số lượng logs
- Hoặc tăng `OCALL_BUFFER_SIZE` trong `ocall_logger.cpp`

### Performance chậm?

- Đừng log trong tight loops
- Chỉ log critical steps
- Log theo batch (mỗi N iterations)

## Files

```
eEVM/optee/ta/
├── include/
│   └── ocall_logger.h       ⬅️ Header file
├── ocall_logger.cpp         ⬅️ Implementation
└── eevm_ta_main.cpp         ⬅️ Usage example

eEVM/optee/host/
└── main.cpp                 ⬅️ CA receives logs
```

## Full Documentation

Xem chi tiết: [OCALL_LOGGING_EXPLAINED.md](./OCALL_LOGGING_EXPLAINED.md)

---

**Last updated**: December 20, 2025
