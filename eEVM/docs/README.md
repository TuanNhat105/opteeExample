# eEVM OP-TEE Documentation

This directory contains comprehensive documentation for the eEVM OP-TEE project.

## 📚 Available Documentation

### 1. OCALL Logging

**OCALL (Outside Call) Logging** - Mechanism to transfer debug logs from Secure World (TA) to Normal World (CA)

| Document | Description | Target Audience |
|----------|-------------|-----------------|
| **[OCALL_LOGGING_EXPLAINED.md](./OCALL_LOGGING_EXPLAINED.md)** | 📖 **Complete Guide** - Deep dive vào cơ chế OCALL logging với chi tiết triển khai, architecture, và best practices | Developers muốn hiểu sâu |
| **[OCALL_LOGGING_QUICKREF.md](./OCALL_LOGGING_QUICKREF.md)** | ⚡ **Quick Reference** - TL;DR version với examples và cheatsheet | Developers cần reference nhanh |
| **[OCALL_LOGGING_DIAGRAMS.md](./OCALL_LOGGING_DIAGRAMS.md)** | 🎨 **Visual Diagrams** - ASCII diagrams minh họa data flow, memory layout, và lifecycle | Visual learners |

### Quick Navigation

```
Bạn là ai?          →  Đọc tài liệu nào?
═══════════════════════════════════════════════════════════
Người mới bắt đầu   →  QUICKREF (quick start)
                    →  DIAGRAMS (visual learning)
                    →  EXPLAINED (full understanding)

Developer           →  QUICKREF (daily reference)
                    →  EXPLAINED (architecture decisions)

Reviewer/Auditor    →  EXPLAINED (complete picture)
                    →  DIAGRAMS (system overview)
```

## 🎯 What is OCALL Logging?

In OP-TEE, Trusted Applications (TAs) run in **Secure World** and cannot directly print to console. OCALL Logging solves this by:

1. **TA** accumulates logs in internal buffer
2. **TA** copies buffer to shared memory before returning
3. **CA** (Client Application) reads and displays logs

**Benefits:**
- ✅ Real-time logs on console (no need for SSH + dmesg)
- ✅ Easy debugging during development
- ✅ Production-ready logging infrastructure
- ✅ Secure (no sensitive data leaks to kernel logs)

## 🚀 Quick Start

### TA Code (Secure World)

```cpp
#include "ocall_logger.h"

static TEE_Result my_command(uint32_t param_types, TEE_Param params[4]) {
    // Init
    ocall_log_init();
    
    // Log (like printf)
    OCALL_LOG("Starting operation...");
    OCALL_LOG("Processing value: %d", 42);
    
    // Flush before return
    if (params[2].memref.buffer) {
        ocall_log_flush_to_params(
            params[2].memref.buffer, 
            &params[2].memref.size);
    }
    
    return TEE_SUCCESS;
}
```

### CA Code (Normal World)

```cpp
// Allocate buffer
char log_buffer[8192] = {0};

// Setup params
op.params[2].tmpref.buffer = log_buffer;
op.params[2].tmpref.size = sizeof(log_buffer);

// Invoke TA
TEEC_InvokeCommand(&sess, MY_CMD, &op, &err_origin);

// Print logs
std::cout << log_buffer;  // Shows all TA logs!
```

## 📁 Project Structure

```
eEVM/
├── docs/                          ⬅️ You are here
│   ├── README.md                  (This file)
│   ├── OCALL_LOGGING_EXPLAINED.md (Complete guide)
│   ├── OCALL_LOGGING_QUICKREF.md  (Quick reference)
│   └── OCALL_LOGGING_DIAGRAMS.md  (Visual diagrams)
│
├── optee/
│   ├── ta/                        ⬅️ Trusted Application
│   │   ├── include/
│   │   │   └── ocall_logger.h     (OCALL API header)
│   │   ├── ocall_logger.cpp       (OCALL implementation)
│   │   └── eevm_ta_main.cpp       (TA main code with OCALL usage)
│   │
│   └── host/                      ⬅️ Client Application
│       └── main.cpp               (CA code that receives logs)
│
├── src/                           ⬅️ eEVM source
│   ├── processor.cpp              (EVM processor - fmt free!)
│   ├── stack.cpp                  (EVM stack - fmt free!)
│   └── ...
│
└── include/                       ⬅️ eEVM headers
    └── eEVM/
        ├── processor.h
        ├── stack.h
        └── ...
```

## 🔧 Implementation Files

### Core OCALL Files

| File | Lines | Purpose |
|------|-------|---------|
| `ta/include/ocall_logger.h` | ~40 | API definitions, OCALL_LOG macro |
| `ta/ocall_logger.cpp` | ~60 | Implementation (buffer mgmt, flush) |
| `ta/eevm_ta_main.cpp` | ~300 | Usage examples (Hello World, Stack tests) |
| `host/main.cpp` | ~120 | CA code to receive and print logs |

### Key Functions

| Function | Purpose | Where |
|----------|---------|-------|
| `ocall_log_init()` | Reset buffer | Start of each command |
| `OCALL_LOG(fmt, ...)` | Write log | Anywhere in TA |
| `ocall_log_flush_to_params()` | Copy to CA | Before every return |

## 📊 Statistics

### eEVM Project Stats

```
Total Source Files:        50+
OP-TEE Compatible:         ✅ Yes
fmt Library Removed:       ✅ Yes (100%)
OCALL Logging:            ✅ Implemented
TA Binary Size:           1.3 MB
Maximum Log Buffer:       8 KB
```

### Components Built

- ✅ `processor.cpp` - EVM bytecode processor
- ✅ `stack.cpp` - EVM stack operations
- ✅ `transaction.cpp` - Transaction handling
- ✅ `util.cpp` - Utilities (hex, keccak)
- ✅ `disassembler.cpp` - Bytecode disassembler
- ✅ `simple/*` - Account, Storage, GlobalState
- ✅ `keccak` - Hash functions
- ✅ `intx` - 256-bit arithmetic

**All fmt dependencies removed!** 🎉

## 🧪 Testing

### Test Cases Available

1. **Stack Test** (`TA_EEVM_CMD_TEST_STACK`)
   - Push/Pop operations
   - Swap functionality
   - Dup (duplicate) operations
   - All with OCALL logging

2. **Hello World Test** (`TA_EEVM_CMD_HELLO_WORLD`)
   - EVM bytecode execution
   - Full processor pipeline
   - Result verification
   - Detailed step-by-step OCALL logs

### Run Tests

```bash
cd /home/abc/nhat/optee_examples/eEVM/optee

# Build
./build.sh

# Run on Raspberry Pi 5
ssh root@192.168.1.74
./eevm_host
```

**Expected Output:**
```
=== OCALL Logs from eEVM Hello World ===
[TA] === Starting eEVM Hello World Test ===
[TA] [Step 1] Creating addresses...
[TA] [Step 2] Creating global state...
[TA] [Step 3] Creating bytecode...
[TA] [INFO] Bytecode size: 135 bytes
[TA] [Step 4] Deploying contract...
[TA] [Step 5] Creating transaction...
[TA] [Step 6] Creating processor...
[TA] [Step 7] Executing EVM bytecode...
[TA] [INFO] Execution completed!
[TA] [INFO] Exit reason: 0
[TA] [INFO] Output size: 28 bytes
[TA] [Step 8] Verifying result...
[TA] [INFO] Expected: 'Hello from eEVM in OP-TEE!'
[TA] [INFO] Got:      'Hello from eEVM in OP-TEE!'
[TA] [PASS] ✓ eEVM execution successful!
[TA] === eEVM Hello World Test PASSED ===
=== End of OCALL Logs ===

✓ eEVM Hello World test completed successfully!
```

## 🐛 Troubleshooting

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| No logs appear | Forgot to flush | Call `ocall_log_flush_to_params()` before return |
| Logs truncated | Buffer too small | Reduce log count or increase `OCALL_BUFFER_SIZE` |
| TA crashes | Memory corruption | Check buffer bounds, validate params |
| "Bad parameters" | Wrong param types | Ensure `params[2]` is `MEMREF_OUTPUT` |

### Debug Checklist

```
☐ Called ocall_log_init()?
☐ Called ocall_log_flush_to_params()?
☐ params[2] is MEMREF_OUTPUT?
☐ CA allocated log_buffer?
☐ CA printed log_buffer?
☐ Flushed before ALL returns (including errors)?
```

## 🎓 Learning Path

### Beginner Track

1. Read [OCALL_LOGGING_QUICKREF.md](./OCALL_LOGGING_QUICKREF.md)
2. Look at [OCALL_LOGGING_DIAGRAMS.md](./OCALL_LOGGING_DIAGRAMS.md)
3. Study `host/main.cpp` (CA side)
4. Study `ta/eevm_ta_main.cpp` (TA side)
5. Modify and test!

### Advanced Track

1. Read [OCALL_LOGGING_EXPLAINED.md](./OCALL_LOGGING_EXPLAINED.md) completely
2. Understand memory layout and security implications
3. Review `ocall_logger.cpp` implementation
4. Study OP-TEE shared memory mechanisms
5. Explore optimization opportunities

## 📖 Additional Resources

### OP-TEE Documentation

- [OP-TEE Documentation](https://optee.readthedocs.io/)
- [GlobalPlatform TEE Client API](https://globalplatform.org/specs-library/tee-client-api-specification/)
- [ARM TrustZone Technology](https://developer.arm.com/ip-products/security-ip/trustzone)

### eEVM Resources

- [Microsoft eEVM GitHub](https://github.com/microsoft/eEVM)
- [Ethereum Yellow Paper](https://ethereum.github.io/yellowpaper/paper.pdf)

### Related Topics

- Secure Enclaves (SGX, SEV, TrustZone)
- EVM Opcodes and Gas Metering
- Trusted Execution Environments

## 🤝 Contributing

Contributions welcome! Areas for improvement:

- 📝 More documentation
- 🧪 Additional test cases
- 🔧 Performance optimizations
- 🛡️ Security hardening
- 📊 Benchmarking tools

## 📜 License

This project follows the licenses of its components:
- eEVM: MIT License
- OP-TEE: BSD 2-Clause License

## 📞 Contact

For questions or issues:
1. Check documentation in this directory
2. Review source code comments
3. Search OP-TEE documentation
4. Open an issue on GitHub

---

**Last Updated**: December 20, 2025  
**Version**: 1.0  
**Status**: ✅ Production Ready
