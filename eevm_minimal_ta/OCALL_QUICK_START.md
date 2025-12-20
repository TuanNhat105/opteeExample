# OCALL Quick Start Guide

## 🎯 What is OCALL?

OCALL (Outside Call) allows your Trusted Application (TA) to send logs back to the Host Application in Normal World - similar to `printf()` debugging but from Secure World.

---

## ⚡ Quick Start

### 1. In Your TA Code

```cpp
// Add OCALL logging anywhere in your TA
OCALL_LOG("Starting test...");

std::string str("Hello Secure World");
OCALL_LOG("Created string, size=%zu", str.size());

if (str.size() != 18) {
    OCALL_LOG("[ERROR] Size mismatch!");
}
OCALL_LOG("[INFO] Test passed!");
```

### 2. Update Your Command Handler

```cpp
static TEE_Result your_command(uint32_t param_types, TEE_Param params[4])
{
    // Accept optional log output parameter
    bool has_logs = (param_types == TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INOUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,  // <-- Log output
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE));
    
    if (has_logs) {
        g_ocall_pos = 0;  // Reset buffer
    }
    
    // Your code with OCALL_LOG
    OCALL_LOG("Test started");
    // ... do work ...
    
    // Copy logs at end
    if (has_logs && params[1].memref.buffer) {
        memcpy(params[1].memref.buffer, g_ocall_buffer, g_ocall_pos);
        params[1].memref.size = g_ocall_pos;
    }
    
    return TEE_SUCCESS;
}
```

### 3. In Your Host Application

```cpp
void run_test_with_logs(TEEC_Session* sess, uint32_t cmd_id) {
    char log_buffer[8192];
    TEEC_Operation op;
    
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT,  // Log buffer
        TEEC_NONE, 
        TEEC_NONE);
    
    op.params[1].tmpref.buffer = log_buffer;
    op.params[1].tmpref.size = sizeof(log_buffer);
    
    TEEC_InvokeCommand(sess, cmd_id, &op, &err_origin);
    
    // Print logs
    if (op.params[1].tmpref.size > 0) {
        std::cout << log_buffer << std::endl;
    }
}
```

---

## 📝 Example Output

```bash
$ ./minimal_evm_host
Running tests...

[TA] === String Test ===
[TA] [INFO] Test 1: Empty string
[TA] [VERBOSE] size=0, empty=true
[TA] [INFO] PASS
[TA] [INFO] Test 2: String literal
[TA] [VERBOSE] Created string, size=24
[TA] [INFO] PASS
[TA] === All tests passed! ===

  [PASS] std::string test
```

---

## 🔑 Key Points

### ✅ DO
- Use OCALL for debugging and testing
- Reset buffer at start: `g_ocall_pos = 0`
- Copy buffer at end to `params[1].memref.buffer`
- Keep log messages under 512 bytes each
- Use format strings: `OCALL_LOG("val=%d", x)`

### ❌ DON'T
- Log sensitive data (keys, passwords, etc.)
- Use in production (performance overhead)
- Exceed 8KB total buffer size
- Forget to check `has_logs` flag
- Log in performance-critical loops

---

## 🐛 Troubleshooting

### No logs appearing?
✓ Check `g_ocall_pos` is reset to 0  
✓ Verify `params[1].memref.buffer` is not NULL  
✓ Ensure `has_logs` check is correct  

### Logs truncated?
✓ Increase `OCALL_BUFFER_SIZE` (default 8KB)  
✓ Reduce number of log messages  
✓ Split into multiple commands  

### TEE_ERROR_BAD_PARAMETERS?
✓ Check param_types match in TA and host  
✓ Verify using `TEEC_MEMREF_TEMP_OUTPUT` in host  
✓ Ensure TA checks for both modes (with/without logs)  

---

## 📚 Full Documentation

See [OCALL_IMPLEMENTATION.md](./OCALL_IMPLEMENTATION.md) for:
- Complete architecture details
- Implementation rationale
- Advanced usage examples
- Performance considerations
- Limitations and future work

---

## 🚀 Build & Test

```bash
# Build
cd /home/abc/nhat/optee_examples/eevm_minimal_ta
./build.sh

# Deploy to Raspberry Pi 5
scp -O ta/*.ta root@192.168.1.74:/lib/optee_armtz/
scp -O host/minimal_evm_host root@192.168.1.74:/usr/bin/

# Run
ssh root@192.168.1.74
minimal_evm_host
```

---

**That's it!** You now have printf-style debugging in your OP-TEE TA! 🎉
