# OpenEnclave OCALL Logging - Cách Implement Đúng Chuẩn

## Overview

OpenEnclave SDK có **built-in OCALL logging mechanism** để log từ Enclave (Secure World) ra Host (Normal World). Đây là cách **chính thức và chuẩn** để implement logging trong TEE environment.

## Architecture

```
┌─────────────────────────────────────┐
│     Enclave (Secure World)          │
│  ┌───────────────────────────────┐ │
│  │  Application Code              │ │
│  │  OE_TRACE_INFO("msg")         │ │
│  └────────────┬──────────────────┘ │
│               ↓                     │
│  ┌────────────────────────────────┐│
│  │  oe_log(level, fmt, ...)       ││
│  │  - Format message              ││
│  │  - Add enclave filename        ││
│  │  - Call oe_log_ocall()         ││
│  └────────────┬───────────────────┘│
│               ↓ OCALL               │
└───────────────┼─────────────────────┘
                │
┌───────────────┼─────────────────────┐
│               ↓                      │
│  ┌────────────────────────────────┐ │
│  │  oe_log_ocall()                │ │
│  │  - Receive message from enclave│ │
│  │  - Call oe_log_message()       │ │
│  └────────────┬───────────────────┘ │
│               ↓                      │
│  ┌────────────────────────────────┐ │
│  │  oe_log_message()              │ │
│  │  - Print to stdout/stderr      │ │
│  │  - Or call custom callback     │ │
│  └────────────────────────────────┘ │
│     Host (Normal World)              │
└──────────────────────────────────────┘
```

## Implementation Details

### 1. EDL Definition (logging.edl)

```c
enclave
{
    trusted
    {
        public void oe_log_init_ecall(
            [in, string] const char* enclave_path,
            uint32_t log_level);
    };

    untrusted
    {
        // Check if logging is supported
        void oe_log_is_supported_ocall();

        // Main logging OCALL
        void oe_log_ocall(
            uint32_t log_level,
            [in, string] const char* message);

        // Direct console write
        void oe_write_ocall(
            int device,
            [in, string] const char* str,
            size_t maxlen);
    };
};
```

### 2. Enclave Side (tracee.c)

```c
// Global log level
static oe_log_level_t _active_log_level = OE_LOG_LEVEL_ERROR;

// Main logging function
oe_result_t oe_log(oe_log_level_t level, const char* fmt, ...)
{
    char _trace_buffer[OE_LOG_MESSAGE_LEN_MAX]; // 2048 bytes
    
    // Check if debug is allowed
    if (!oe_is_enclave_debug_allowed())
        return OE_OK;
    
    // Check log level
    if (level > _active_log_level)
        return OE_OK;
    
    // Format message with enclave filename prefix
    oe_snprintf(_trace_buffer, OE_LOG_MESSAGE_LEN_MAX, 
                "%s:", _enclave_filename);
    
    // Add formatted message
    oe_va_start(ap, fmt);
    oe_vsnprintf(&_trace_buffer[bytes_written], 
                 OE_LOG_MESSAGE_LEN_MAX - bytes_written,
                 fmt, ap);
    oe_va_end(ap);
    
    // Send to host via OCALL
    return oe_log_ocall(level, _trace_buffer);
}
```

### 3. Host Side (log.c)

```c
void oe_log_ocall(uint32_t log_level, const char* message)
{
    // Forward to logging system
    oe_log_message(true, (oe_log_level_t)log_level, message);
}

// In trace.c:
void oe_log_message(bool is_enclave, oe_log_level_t level, const char* message)
{
    // If custom callback is set, use it
    if (_log_callback)
    {
        _log_callback(_log_context, is_enclave, level, message);
        return;
    }
    
    // Otherwise, print to stdout/stderr
    FILE* stream = (level <= OE_LOG_LEVEL_ERROR) ? stderr : stdout;
    fprintf(stream, "[%s] %s: %s\n",
            is_enclave ? "Enclave" : "Host",
            oe_log_level_strings[level],
            message);
}
```

## Usage in Enclave Code

### Macros Available

```c
// From internal/trace.h
#define OE_TRACE_FATAL(fmt, ...)    oe_log(OE_LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define OE_TRACE_ERROR(fmt, ...)    oe_log(OE_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define OE_TRACE_WARNING(fmt, ...)  oe_log(OE_LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define OE_TRACE_INFO(fmt, ...)     oe_log(OE_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define OE_TRACE_VERBOSE(fmt, ...)  oe_log(OE_LOG_LEVEL_VERBOSE, fmt, ##__VA_ARGS__)
```

### Example in TA

```cpp
#include <openenclave/internal/trace.h>

void my_ta_function() {
    OE_TRACE_INFO("TA function started");
    
    int result = process_data();
    OE_TRACE_INFO("Processing result: %d", result);
    
    if (result < 0) {
        OE_TRACE_ERROR("Processing failed with code %d", result);
        return;
    }
    
    OE_TRACE_INFO("TA function completed successfully");
}
```

### Example Output on Host

```
[Enclave] INFO: my_ta.signed.so:TA function started
[Enclave] INFO: my_ta.signed.so:Processing result: 42
[Enclave] INFO: my_ta.signed.so:TA function completed successfully
```

## Log Levels

```c
typedef enum _oe_log_level
{
    OE_LOG_LEVEL_NONE = 0,
    OE_LOG_LEVEL_FATAL = 1,
    OE_LOG_LEVEL_ERROR = 2,
    OE_LOG_LEVEL_WARNING = 3,
    OE_LOG_LEVEL_INFO = 4,
    OE_LOG_LEVEL_VERBOSE = 5,
    OE_LOG_LEVEL_MAX = OE_LOG_LEVEL_VERBOSE
} oe_log_level_t;
```

## Host Configuration

### Set Log Level

```c
// In host app
#include <openenclave/trace.h>

int main() {
    // Set host log level
    oe_set_host_log_level(OE_LOG_LEVEL_VERBOSE);
    
    // Initialize enclave with log level
    oe_create_enclave("myenclave.signed.so", 
                      OE_ENCLAVE_TYPE_AUTO,
                      OE_ENCLAVE_FLAG_DEBUG,
                      NULL, NULL,
                      &enclave);
    
    // Enclave will inherit log level
    oe_log_init_ecall(enclave, "myenclave.signed.so", OE_LOG_LEVEL_INFO);
}
```

### Custom Log Callback

```c
void my_log_callback(
    void* context,
    bool is_enclave,
    oe_log_level_t level,
    const char* message)
{
    // Custom logging logic
    const char* source = is_enclave ? "ENCLAVE" : "HOST";
    printf("[%s][LEVEL %d] %s\n", source, level, message);
    
    // Could also:
    // - Write to file
    // - Send to syslog
    // - Send to remote logging service
}

int main() {
    // Set custom callback
    oe_log_set_callback(NULL, my_log_callback);
    
    // Now all logs go through your callback
}
```

## Key Features

### 1. Thread-Safe
- Uses mutex `_log_lock` to serialize logging

### 2. Enclave Identification
- Automatically prefixes logs with enclave filename

### 3. Format String Support
- Full printf-style formatting with variadic arguments

### 4. Level Filtering
- Only logs messages at or below active log level

### 5. Debug Gate
- Only works in debug enclaves (`oe_is_enclave_debug_allowed()`)

### 6. Buffer Size
- Fixed 2KB buffer (`OE_LOG_MESSAGE_LEN_MAX = 2048`)

## Advantages Over Manual OCALL

✅ **Standardized**: Industry-standard approach  
✅ **Printf-style**: Easy formatting with `%d`, `%s`, etc.  
✅ **Thread-safe**: Built-in mutex protection  
✅ **Level filtering**: Control verbosity at runtime  
✅ **Custom callbacks**: Flexible output handling  
✅ **No shared memory**: Uses marshalling instead  
✅ **Enclave identification**: Auto-prefixes with filename  

## Port to OP-TEE

Để port sang OP-TEE TA, cần:

1. ✅ Remove EDL dependency (OP-TEE không dùng EDL)
2. ✅ Replace `oe_log_ocall()` với shared memory approach (như ta đã làm)
3. ✅ Keep printf-style formatting
4. ✅ Keep thread-safety với OP-TEE mutex
5. ✅ Keep level filtering

## So Sánh Với Implementation Của Chúng Ta

| Feature | OpenEnclave | Our OCALL |
|---------|-------------|-----------|
| **Method** | EDL-based OCALL | Shared Memory |
| **Formatting** | Printf-style ✅ | Simple strings only |
| **Buffer** | 2KB stack | 4KB shared memory |
| **Thread-safe** | Mutex ✅ | Ring buffer |
| **Performance** | OCALL overhead | Async, no overhead |
| **OP-TEE compat** | ❌ Needs port | ✅ Native |

## Kết Luận

OpenEnclave sử dụng **EDL-based OCALL** với **printf-style formatting** để log từ enclave ra host. Approach của chúng ta (shared memory ring buffer) là **lightweight hơn** và **native cho OP-TEE**, nhưng OpenEnclave approach có **formatting tốt hơn**.

**Next Step**: Implement printf-style formatting trong OCALL của chúng ta!
