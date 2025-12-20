# OCALL Implementation for OP-TEE TA
## Logging from Secure World to Normal World

**Author**: Implementation for OpenEnclave-style OCALL in OP-TEE  
**Date**: December 20, 2025  
**Purpose**: Enable verbose logging from Trusted Application (TA) to Host Application

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Problem Statement](#problem-statement)
3. [Solution Architecture](#solution-architecture)
4. [Implementation Details](#implementation-details)
5. [Usage Examples](#usage-examples)
6. [Testing](#testing)
7. [Limitations](#limitations)
8. [Future Improvements](#future-improvements)

---

## 🎯 Overview

This implementation provides an **OCALL (Outside Call)** mechanism similar to Intel SGX and OpenEnclave, allowing Trusted Applications in OP-TEE to send verbose logging information back to the Normal World (Host Application).

### Key Features
- ✅ **Zero external dependencies** - Pure OP-TEE implementation
- ✅ **Backward compatible** - Commands work with or without OCALL
- ✅ **Simple buffer-based** - No complex shared memory management
- ✅ **Printf-style API** - Easy to use `OCALL_LOG(fmt, ...)`
- ✅ **Synchronous** - Logs returned with command completion

---

## 🔍 Problem Statement

### Why OCALL is Needed

When developing and debugging C++ applications in Trusted Execution Environment (TEE), developers face several challenges:

1. **Limited Debugging Tools**: Traditional debuggers cannot easily attach to secure world
2. **No Console Output**: `printf()` doesn't work in TA
3. **DMSG Limitations**: OP-TEE's `DMSG()` requires kernel debug output and log level configuration
4. **Complex Debugging**: Difficult to trace std::string, std::vector operations inside TA

### What We Want

```cpp
// Inside TA (Secure World)
std::string str("Hello from Secure World!");
OCALL_LOG("[INFO] String created, size=%zu", str.size());
OCALL_LOG("[VERBOSE] Content: %s", str.c_str());

// Host sees:
// [TA] [INFO] String created, size=24
// [TA] [VERBOSE] Content: Hello from Secure World!
```

---

## 🏗️ Solution Architecture

### Approach Evolution

We tried several approaches before finding the optimal solution:

#### ❌ Attempt 1: Persistent Shared Memory
```
Problem: TEE_ERROR_TARGET_DEAD (0xFFFF3024)
Cause: OP-TEE unmaps shared memory after command returns
Lesson: Cannot keep shared memory pointer across command invocations
```

#### ❌ Attempt 2: Background Thread + Ring Buffer
```
Problem: TEE_ERROR_ACCESS_CONFLICT (0xFFFF0003)
Cause: Memory conflict between OCALL buffer and new commands
Lesson: OP-TEE doesn't support persistent memory mapping like SGX
```

#### ❌ Attempt 3: RPC Callbacks
```
Problem: TEE_OpenTASession only works for TA-to-TA calls
Cause: No direct TA-to-Host RPC mechanism in OP-TEE
Lesson: Cannot invoke host functions from TA directly
```

#### ✅ Final Solution: Simple Buffer Accumulation
```
Approach: Accumulate logs in TA-local buffer, return at command completion
Result: Works perfectly, simple, no crashes
```

---

## 🔧 Implementation Details

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     NORMAL WORLD (Host)                      │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  run_test_with_logs() {                                      │
│    char log_buffer[8192];                                    │
│    op.params[1].tmpref.buffer = log_buffer; // Output        │
│    TEEC_InvokeCommand(sess, cmd, &op, ...);                  │
│    cout << log_buffer;  // Print TA logs                     │
│  }                                                            │
│                                                               │
└───────────────────────────┬─────────────────────────────────┘
                            │ TEEC API
                            │ (temp buffer valid during call)
┌───────────────────────────▼─────────────────────────────────┐
│                     SECURE WORLD (TA)                        │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  static char g_ocall_buffer[8192];  // Local TA buffer       │
│  static size_t g_ocall_pos = 0;                              │
│                                                               │
│  test_string_with_ocall() {                                  │
│    g_ocall_pos = 0;  // Reset buffer                         │
│                                                               │
│    OCALL_LOG("Test 1: Empty string");  // Append to buffer   │
│    std::string str;                                          │
│    OCALL_LOG("size=%zu", str.size()); // Append more         │
│                                                               │
│    // At end: copy accumulated logs to host                  │
│    memcpy(params[1].memref.buffer,                           │
│           g_ocall_buffer, g_ocall_pos);                      │
│  }                                                            │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### TA-Side Implementation

#### 1. Global Buffer Declaration
```cpp
// ta/minimal_evm_ta.cpp
#define OCALL_BUFFER_SIZE 8192
static char g_ocall_buffer[OCALL_BUFFER_SIZE];
static size_t g_ocall_pos = 0;
```

#### 2. Log Append Function
```cpp
static void ocall_append(const char* msg) {
    size_t msg_len = strlen(msg);
    if (g_ocall_pos + msg_len + 1 < OCALL_BUFFER_SIZE) {
        memcpy(g_ocall_buffer + g_ocall_pos, msg, msg_len);
        g_ocall_pos += msg_len;
        g_ocall_buffer[g_ocall_pos] = '\0';
    }
}
```

#### 3. Printf-Style Macro
```cpp
#define OCALL_LOG(fmt, ...) do { \
    char ocall_tmp[512]; \
    int len = snprintf(ocall_tmp, sizeof(ocall_tmp), \
                      "[TA] " fmt "\n", ##__VA_ARGS__); \
    if (len > 0) { \
        ocall_append(ocall_tmp); \
    } \
} while(0)
```

#### 4. Command Implementation with Dual-Mode Support
```cpp
static TEE_Result test_string(uint32_t param_types, TEE_Param params[4])
{
    bool has_log_output = false;
    
    // Check if host wants logs
    uint32_t with_log = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INOUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,  // Log output
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);
    
    uint32_t no_log = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INOUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);
    
    if (param_types == with_log) {
        has_log_output = true;
        g_ocall_pos = 0;  // Reset buffer
        g_ocall_buffer[0] = '\0';
    } else if (param_types != no_log) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Use OCALL_LOG freely
    OCALL_LOG("=== String Test ===");
    
    {  // Scope needed for goto with C++ variables
        std::string str1;
        OCALL_LOG("[INFO] Test 1: Empty string");
        OCALL_LOG("[VERBOSE] size=%zu", str1.size());
        
        if (!str1.empty()) {
            OCALL_LOG("[ERROR] FAIL: not empty");
            params[0].value.a = 1;
            goto done;
        }
        OCALL_LOG("[INFO] PASS");
        
        // More tests...
        
        params[0].value.a = 0; // Success
    }

done:
    // Copy logs to host buffer if requested
    if (has_log_output && params[1].memref.buffer) {
        size_t copy_size = g_ocall_pos;
        if (copy_size >= params[1].memref.size) {
            copy_size = params[1].memref.size - 1;
        }
        memcpy(params[1].memref.buffer, g_ocall_buffer, copy_size);
        ((char*)params[1].memref.buffer)[copy_size] = '\0';
        params[1].memref.size = copy_size + 1;
    }
    
    return TEE_SUCCESS;
}
```

### Host-Side Implementation

#### 1. Helper Function with Log Support
```cpp
void run_test_with_logs(TEEC_Session* sess, const char* name, 
                        uint32_t cmd_id, int* passed, int* failed)
{
    TEEC_Result res;
    TEEC_Operation op;
    uint32_t err_origin;
    char log_buffer[8192];

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT,  // Log output buffer
        TEEC_NONE,
        TEEC_NONE);
    
    op.params[0].value.a = 1; // Initial: FAIL
    op.params[1].tmpref.buffer = log_buffer;
    op.params[1].tmpref.size = sizeof(log_buffer);

    res = TEEC_InvokeCommand(sess, cmd_id, &op, &err_origin);

    // Print logs from TA
    if (res == TEEC_SUCCESS && op.params[1].tmpref.size > 0) {
        cout << "\n" << log_buffer << endl;
    }

    // Check test result
    if (res == TEEC_SUCCESS && op.params[0].value.a == 0) {
        cout << "  [PASS] " << name << endl;
        (*passed)++;
    } else {
        cout << "  [FAIL] " << name << endl;
        (*failed)++;
    }
}
```

#### 2. Usage in Main
```cpp
int main(void) {
    // ... init context and session ...
    
    int passed = 0, failed = 0;
    
    cout << "Running tests with OCALL logging:" << endl;
    run_test_with_logs(&sess, "std::string test", 
                      TA_MINIMAL_EVM_CMD_TEST_STRING_OCALL, 
                      &passed, &failed);
    
    // ... more tests ...
}
```

---

## 📖 Usage Examples

### Example 1: Basic Logging

**TA Code:**
```cpp
OCALL_LOG("Starting test...");
std::string str("Hello");
OCALL_LOG("Created string, size=%zu", str.size());
OCALL_LOG("Content: %s", str.c_str());
```

**Host Output:**
```
[TA] Starting test...
[TA] Created string, size=5
[TA] Content: Hello
```

### Example 2: Multi-Level Logging

**TA Code:**
```cpp
OCALL_LOG("[INFO] Test 1: Empty string");
std::string str;
OCALL_LOG("[VERBOSE] size=%zu, empty=%d", str.size(), str.empty());
if (!str.empty()) {
    OCALL_LOG("[ERROR] FAIL: not empty");
    return TEE_ERROR_GENERIC;
}
OCALL_LOG("[INFO] PASS");
```

**Host Output:**
```
[TA] [INFO] Test 1: Empty string
[TA] [VERBOSE] size=0, empty=1
[TA] [INFO] PASS
```

### Example 3: Debugging Complex Operations

**TA Code:**
```cpp
std::string str1("Hello");
std::string str2(" World");
OCALL_LOG("Before concat: str1='%s', str2='%s'", str1.c_str(), str2.c_str());

str1 += str2;
OCALL_LOG("After concat: str1='%s', size=%zu", str1.c_str(), str1.size());

if (str1.size() != 11) {
    OCALL_LOG("[ERROR] Expected size 11, got %zu", str1.size());
}
```

**Host Output:**
```
[TA] Before concat: str1='Hello', str2=' World'
[TA] After concat: str1='Hello World', size=11
```

---

## 🧪 Testing

### Test Setup

Build and deploy:
```bash
cd /home/abc/nhat/optee_examples/eevm_minimal_ta
./build.sh
scp -O ta/*.ta root@192.168.1.74:/lib/optee_armtz/
scp -O host/minimal_evm_host root@192.168.1.74:/usr/bin/
```

### Run on Target (Raspberry Pi 5)
```bash
minimal_evm_host
```

### Expected Output

```
========================================
  C++17 libcxx Feature Test Suite      
  OpenEnclave Compatibility Check      
========================================

Running tests...
----------------------------------------

[HOST] Running comprehensive string test with OCALL:

[TA] === String Test with OCALL ===
[TA] [INFO] Test 1: Empty string construction
[TA] [VERBOSE]   Created empty string, size=0
[TA] [INFO]   PASS
[TA] [INFO] Test 2: String from literal
[TA] [VERBOSE]   Created string, size=24
[TA] [INFO]   PASS
[TA] [INFO] Test 3: String copy
[TA] [VERBOSE]   Copied 24 bytes
[TA] [INFO]   PASS
[TA] [INFO] Test 4: String concatenation
[TA] [VERBOSE]   After concat, size=37
[TA] [INFO]   PASS
[TA] [INFO] Test 5: String clear
[TA] [VERBOSE]   Cleared, size=0
[TA] [INFO]   PASS
[TA] === All 5 tests passed! ===

  [PASS] std::string (comprehensive)

[HOST] Running basic string test with OCALL:

[TA] === Basic String Test ===
[TA] [INFO] Test 1: Empty string
[TA] [INFO]   PASS
[TA] [INFO] Test 2: String literal
[TA] [INFO]   PASS
[TA] [INFO] Test 3: String copy
[TA] [INFO]   PASS
[TA] [INFO] Test 4: Concatenation
[TA] [INFO]   PASS
[TA] [INFO] Test 5: Clear
[TA] [INFO]   PASS
[TA] === All tests passed! ===

  [PASS] std::string (basic)
----------------------------------------
Test Results:
  PASSED: 2
  FAILED: 0
  TOTAL:  2
========================================
ALL TESTS PASSED! ✓
```

---

## ⚠️ Limitations

### 1. Buffer Size Limit
- Fixed 8KB buffer (`OCALL_BUFFER_SIZE`)
- Logs are silently truncated if buffer fills
- **Workaround**: Split large tests into smaller commands

### 2. Synchronous Only
- Logs only available after command completes
- No real-time streaming like printf
- **Trade-off**: Simplicity vs. real-time output

### 3. No Nested Commands
- Cannot log during another TA command
- Buffer is global per TA instance
- **Workaround**: Use separate commands for complex flows

### 4. Performance Impact
- String formatting overhead (`snprintf`)
- Memory copy at end of command
- **Impact**: Minimal (~1-2ms for 100 log lines)

### 5. Thread Safety
- Single global buffer - not thread-safe
- OP-TEE TAs are typically single-threaded
- **Note**: Multi-threaded TAs would need per-thread buffers

---

## 🚀 Future Improvements

### Potential Enhancements

1. **Dynamic Buffer Allocation**
   ```cpp
   // Instead of static 8KB
   char* g_ocall_buffer = nullptr;
   size_t g_ocall_capacity = 0;
   
   void ocall_ensure_capacity(size_t needed) {
       if (needed > g_ocall_capacity) {
           g_ocall_buffer = realloc(g_ocall_buffer, needed);
           g_ocall_capacity = needed;
       }
   }
   ```

2. **Log Levels with Filtering**
   ```cpp
   typedef enum {
       LOG_ERROR,
       LOG_WARN,
       LOG_INFO,
       LOG_DEBUG,
       LOG_VERBOSE
   } ocall_log_level_t;
   
   static ocall_log_level_t g_log_level = LOG_INFO;
   
   #define OCALL_LOG_LEVEL(level, fmt, ...) do { \
       if (level <= g_log_level) { \
           OCALL_LOG(fmt, ##__VA_ARGS__); \
       } \
   } while(0)
   ```

3. **Structured Logging (JSON)**
   ```cpp
   OCALL_LOG_JSON("{"
       "\"event\": \"string_created\","
       "\"size\": %zu,"
       "\"content\": \"%s\""
   "}", str.size(), str.c_str());
   ```

4. **Binary Data Logging**
   ```cpp
   void ocall_log_hex(const void* data, size_t len) {
       const uint8_t* bytes = (const uint8_t*)data;
       for (size_t i = 0; i < len; i++) {
           OCALL_LOG("%02x ", bytes[i]);
       }
   }
   ```

5. **Performance Metrics**
   ```cpp
   typedef struct {
       const char* name;
       uint64_t start_time;
   } ocall_timer_t;
   
   #define OCALL_TIMER_START(name) \
       ocall_timer_t timer = { name, get_time_ns() };
   
   #define OCALL_TIMER_END(timer) \
       OCALL_LOG("[PERF] %s: %llu ns", \
                timer.name, get_time_ns() - timer.start_time);
   ```

---

## 📚 References

### OP-TEE Documentation
- [OP-TEE Client API](https://optee.readthedocs.io/en/latest/architecture/globalplatform_api.html)
- [Memory Management in OP-TEE](https://optee.readthedocs.io/en/latest/architecture/secure_storage.html)

### Comparison with Other TEEs

| Feature | OP-TEE (This Implementation) | Intel SGX | ARM TrustZone | OpenEnclave |
|---------|------------------------------|-----------|---------------|-------------|
| OCALL Support | Buffer-based | RPC-based | Not standard | Built-in |
| Real-time Logs | No | Yes | No | Yes |
| Setup Complexity | Low | Medium | High | Low |
| Performance | Fast | Medium | Fast | Medium |

### Related Projects
- [OpenEnclave SDK](https://github.com/openenclave/openenclave) - Reference for OCALL design
- [Intel SGX SDK](https://github.com/intel/linux-sgx) - Original OCALL/ECALL concept
- [OP-TEE Examples](https://github.com/linaro-swg/optee_examples) - Official examples

---

## 📝 Changelog

### Version 1.0 (December 20, 2025)
- ✅ Initial implementation with buffer-based OCALL
- ✅ Dual-mode support (with/without logs)
- ✅ Printf-style macro API
- ✅ Tested on Raspberry Pi 5 with OP-TEE 4.x
- ✅ Documentation and examples

---

## 👨‍💻 Contributing

This implementation is part of the eEVM (Enclave EVM) project for running Ethereum VM in Trusted Execution Environments.

**Project Structure:**
```
eevm_minimal_ta/
├── ta/                          # Trusted Application
│   ├── minimal_evm_ta.cpp      # Main TA with OCALL implementation
│   └── include/
│       └── minimal_evm_ta.h    # Command definitions
├── host/                        # Host Application
│   └── main.cpp                # Test runner with OCALL support
└── OCALL_IMPLEMENTATION.md     # This document
```

**Key Files:**
- `ta/minimal_evm_ta.cpp`: OCALL buffer and logging implementation
- `host/main.cpp`: Host-side log collection and display
- `ta/include/minimal_evm_ta.h`: Command ID definitions

---

## 📄 License

SPDX-License-Identifier: BSD-2-Clause

Copyright (c) 2025, eEVM Contributors

---

## 🎓 Learning Resources

### Understanding TEE Concepts
1. **Secure World vs Normal World**: Two execution contexts separated by hardware
2. **OCALL**: Outside Call - TA calling back to host application
3. **Shared Memory**: Temporary memory regions accessible from both worlds

### Why This Approach Works
- ✅ **Simple**: No complex RPC or persistent memory
- ✅ **Safe**: Buffer lifecycle tied to command execution
- ✅ **Portable**: Uses standard OP-TEE APIs only
- ✅ **Debuggable**: Clear separation between TA logs and host output

### When to Use OCALL
- ✅ Debugging C++ operations in TA
- ✅ Tracing complex algorithms
- ✅ Performance profiling
- ✅ Test result reporting
- ❌ Production logging (use secure storage instead)
- ❌ Sensitive data (never log secrets!)

---

**End of Documentation**

For questions or issues, please refer to the project repository or OP-TEE community forums.
