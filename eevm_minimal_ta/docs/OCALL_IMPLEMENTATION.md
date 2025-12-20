# OCALL Implementation for OP-TEE
# Log từ Secure World sang Normal World Real-time

## Kỹ thuật: Shared Memory Buffer

OP-TEE không hỗ trợ OCALL trực tiếp như SGX, nhưng có thể implement bằng:
1. **Shared Memory**: TA write log vào buffer
2. **Polling/Signal**: Host check buffer định kỳ hoặc TA signal
3. **Two-way RPC**: TA pause và gọi host command (phức tạp)

## Giải pháp Đơn giản: Shared Memory Ring Buffer

### Architecture:

```
┌─────────────────────────────────────────┐
│         Normal World (Host)             │
│  ┌────────────────────────────────┐    │
│  │  Monitor Thread                 │    │
│  │  - Poll shared memory           │    │
│  │  - Print logs to stdout         │    │
│  └────────────────────────────────┘    │
│              ↑                           │
│              │ Read                      │
│  ┌───────────┴──────────────────────┐  │
│  │   Shared Memory Ring Buffer      │  │
│  │   struct {                       │  │
│  │     uint32_t write_ptr;          │  │
│  │     uint32_t read_ptr;           │  │
│  │     char buffer[LOG_SIZE];       │  │
│  │   }                              │  │
│  └───────────┬──────────────────────┘  │
│              │ Write                     │
│              ↓                           │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│      Secure World (TA)                  │
│  ┌────────────────────────────────┐    │
│  │  OCALL_LOG() macro              │    │
│  │  - Format message               │    │
│  │  - Write to ring buffer         │    │
│  │  - Update write_ptr             │    │
│  └────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

## Implementation

### 1. Shared Header (minimal_evm_ta.h)

```cpp
// OCALL shared memory structure
#define OCALL_LOG_BUFFER_SIZE 4096

typedef struct {
    volatile uint32_t write_ptr;  // TA writes here
    volatile uint32_t read_ptr;   // Host reads here
    volatile uint32_t flags;      // Control flags
    char buffer[OCALL_LOG_BUFFER_SIZE];
} ocall_log_buffer_t;

#define OCALL_FLAG_ACTIVE 0x1
```

### 2. TA Code (minimal_evm_ta.cpp)

```cpp
// Global shared buffer pointer (set by host)
static ocall_log_buffer_t* g_ocall_buffer = nullptr;

// Initialize OCALL buffer
void ocall_init(ocall_log_buffer_t* buf) {
    g_ocall_buffer = buf;
    if (buf) {
        buf->write_ptr = 0;
        buf->read_ptr = 0;
        buf->flags = OCALL_FLAG_ACTIVE;
    }
}

// Write log to shared buffer
static void ocall_write(const char* msg) {
    if (!g_ocall_buffer || !msg) return;
    
    uint32_t len = strlen(msg);
    if (len >= OCALL_LOG_BUFFER_SIZE - 1) {
        len = OCALL_LOG_BUFFER_SIZE - 2;
    }
    
    uint32_t wp = g_ocall_buffer->write_ptr;
    
    for (uint32_t i = 0; i < len; i++) {
        g_ocall_buffer->buffer[wp] = msg[i];
        wp = (wp + 1) % OCALL_LOG_BUFFER_SIZE;
    }
    
    // Null terminator
    g_ocall_buffer->buffer[wp] = '\\0';
    wp = (wp + 1) % OCALL_LOG_BUFFER_SIZE;
    
    // Memory barrier
    __sync_synchronize();
    
    // Update write pointer
    g_ocall_buffer->write_ptr = wp;
}

// OCALL macro
#define OCALL_LOG(fmt, ...) do { \\
    char buf[256]; \\
    snprintf(buf, sizeof(buf), "[TA] " fmt "\\n", ##__VA_ARGS__); \\
    ocall_write(buf); \\
    DMSG("OCALL: %s", buf); \\
} while(0)

// New command to set OCALL buffer
#define TA_MINIMAL_EVM_CMD_SET_OCALL_BUFFER 100

static TEE_Result set_ocall_buffer(uint32_t param_types, TEE_Param params[4]) {
    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INOUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    if (params[0].memref.size != sizeof(ocall_log_buffer_t))
        return TEE_ERROR_BAD_PARAMETERS;

    ocall_init((ocall_log_buffer_t*)params[0].memref.buffer);
    
    OCALL_LOG("OCALL buffer initialized at %p", g_ocall_buffer);
    
    return TEE_SUCCESS;
}
```

### 3. Host Code (main.cpp)

```cpp
#include <thread>
#include <atomic>

// Global OCALL buffer
static ocall_log_buffer_t g_ocall_buffer;
static std::atomic<bool> g_monitor_running(false);

// Monitor thread function
void ocall_monitor_thread() {
    uint32_t last_read = g_ocall_buffer.read_ptr;
    
    while (g_monitor_running) {
        uint32_t wp = g_ocall_buffer.write_ptr;
        uint32_t rp = last_read;
        
        while (rp != wp) {
            char c = g_ocall_buffer.buffer[rp];
            if (c == '\\0') {
                std::cout << std::flush;
            } else {
                std::cout << c;
            }
            rp = (rp + 1) % OCALL_LOG_BUFFER_SIZE;
        }
        
        last_read = rp;
        g_ocall_buffer.read_ptr = rp;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Initialize OCALL in host
void setup_ocall(TEEC_Session* sess) {
    TEEC_Operation op;
    TEEC_Result res;
    uint32_t err_origin;
    
    // Clear buffer
    memset(&g_ocall_buffer, 0, sizeof(g_ocall_buffer));
    
    // Setup operation
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INOUT,
        TEEC_NONE,
        TEEC_NONE,
        TEEC_NONE);
    
    op.params[0].tmpref.buffer = &g_ocall_buffer;
    op.params[0].tmpref.size = sizeof(g_ocall_buffer);
    
    // Send buffer to TA
    res = TEEC_InvokeCommand(sess, TA_MINIMAL_EVM_CMD_SET_OCALL_BUFFER,
                             &op, &err_origin);
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "Failed to setup OCALL: 0x" << std::hex << res << std::endl;
        return;
    }
    
    // Start monitor thread
    g_monitor_running = true;
    std::thread monitor(ocall_monitor_thread);
    monitor.detach();
    
    std::cout << "[HOST] OCALL monitor started" << std::endl;
}

// In main():
int main() {
    // ... TEEC_InitializeContext, TEEC_OpenSession ...
    
    // Setup OCALL
    setup_ocall(&sess);
    
    // Now all OCALL_LOG() in TA will appear here!
    
    // ... run tests ...
    
    // Cleanup
    g_monitor_running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // ... TEEC_CloseSession, TEEC_FinalizeContext ...
}
```

## Sử dụng

Trong TA code, thay vì `DMSG()`:

```cpp
OCALL_LOG("Processing transaction %d", tx_id);
OCALL_LOG("Account balance: %lu", balance);
OCALL_LOG("Execution result: %s", result.c_str());
```

Log sẽ xuất hiện **real-time** trong terminal của host app!

## Ưu điểm

✓ Real-time logging từ TA
✓ Không cần check dmesg
✓ Thread-safe (ring buffer + atomic operations)
✓ Low overhead
✓ Không block TA execution

## Nhược điểm

- Cần shared memory (4KB)
- Polling overhead (10ms interval)
- Buffer có thể overflow nếu log quá nhanh

## Tối ưu hóa

1. **Giảm polling interval** xuống 1ms nếu cần real-time hơn
2. **Tăng buffer size** nếu log nhiều
3. **Thêm timestamp** trong OCALL_LOG macro
4. **Lock-free ring buffer** cho thread-safety tốt hơn

---

**Note**: Đây là cách "fake OCALL" vì OP-TEE không hỗ trợ OCALL thật như SGX. 
Nhưng nó hoạt động tốt cho debugging và monitoring!
