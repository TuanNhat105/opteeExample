# 🎯 Lộ trình tích hợp eEVM vào OP-TEE + Go RPC

## Giai đoạn 1: Test C++ trong TEE ✅ (BẠN Ở ĐÂY)

### Mục tiêu:
Verify C++ runtime hoạt động trong Secure World

### Các bước:

#### 1. Cài toolchain
```bash
cd /home/abc/nhat/optee_examples/hello_cpp
./install_toolchain.sh
source ~/.bashrc
```

#### 2. Build hello_cpp TA
```bash
./quickstart.sh
```

#### 3. Deploy lên Pi 5
```bash
# Thay <IP> bằng IP của Pi 5
./deploy.sh pi@<IP>
```

#### 4. Test
```bash
./run_test.sh pi@<IP>
```

### Expected Output:
```
✅ Test PASSED: Basic C++ (string, concatenation)
✅ Test PASSED: STL (vector, find, algorithms)
✅ Test PASSED: C++ Class (OOP, constructor, methods)
✅ Test PASSED: Data processing (uppercase conversion)
```

---

## Giai đoạn 2: Tích hợp eEVM vào TA ⏭️

### Mục tiêu:
Execute EVM bytecode trong Secure World

### Kiến trúc:
```
┌─────────────────────────────────────┐
│  Secure World (TEE)                 │
│  ┌───────────────────────────────┐  │
│  │  eEVM TA (C++)                │  │
│  │  - Processor::run()           │  │
│  │  - SimpleGlobalState          │  │
│  │  - Smart contract execution   │  │
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

### Files cần tạo:
```
optee_examples/evm_ta/
├── ta/
│   ├── evm_ta.cpp                 # TA với eEVM
│   ├── SimpleGlobalState.cpp      # In-memory state
│   ├── Makefile
│   └── sub.mk                     # Link eEVM library
└── host/
    └── main.c                     # Test với hello_world.sol
```

### Build:
```bash
cd /home/abc/eEVM
mkdir build_arm64 && cd build_arm64
cmake -DCMAKE_TOOLCHAIN_FILE=../CMakeToolchain.txt ..
make -j$(nproc)

# Copy libeevm.a to TA
cp libeevm.a /home/abc/nhat/optee_examples/evm_ta/ta/
```

---

## Giai đoạn 3: OCALL - eEVM gọi Normal World ⏭️

### Mục tiêu:
eEVM lấy blockchain state từ Normal World

### Kiến trúc:
```
┌─────────────────────────────────────────────────────┐
│  Secure World                                        │
│  ┌─────────────────────────────────────┐            │
│  │ eEVM TA                             │            │
│  │  - GlobalStateOCall                 │            │
│  │    ├─ get_account()  ───┐           │            │
│  │    └─ get_storage()  ───┤           │            │
│  └──────────────────────────┼───────────┘            │
│                             │ OCALL                  │
├─────────────────────────────┼───────────────────────┤
│  Normal World               ▼                        │
│  ┌─────────────────────────────────────┐            │
│  │ Supplicant (C)                      │            │
│  │  - handle_evm_ocall()               │            │
│  │    ├─ Read from cache               │            │
│  │    └─ Forward to Go RPC ────┐       │            │
│  └──────────────────────────────┼───────┘            │
└─────────────────────────────────┼─────────────────────┘
                                  │ HTTP/gRPC
                      ┌───────────▼──────────┐
                      │  Go RPC Server       │
                      │  - GetAccount()      │
                      │  - GetStorage()      │
                      │  - Database          │
                      └──────────────────────┘
```

### Implementation:

#### 3.1. GlobalStateOCall.h
```cpp
class GlobalStateOCall : public eevm::GlobalState {
public:
    eevm::Account get(const eevm::Address& addr) override {
        // Call OCALL to Normal World
        TEE_Param params[4];
        // ... setup params
        TEE_Result res = tee_rpc_get_file(
            OPTEE_MSG_RPC_CMD_OCALL_EVM_DB, params);
        
        // Parse result
        return parseAccount(params[1].memref.buffer);
    }
};
```

#### 3.2. Supplicant Handler (C)
```c
static size_t handle_evm_ocall(const struct optee_msg_arg *arg, void *buf) {
    uint32_t op_id = arg->params[0].u.value.a;
    
    switch (op_id) {
    case DB_OP_GET_ACCOUNT: {
        const char* address = arg->params[1].u.tmem.buf_ptr;
        
        // Call Go RPC via CGO
        char result[4096];
        size_t result_size = go_get_account(address, result, sizeof(result));
        
        // Return to TEE
        memcpy(buf, result, result_size);
        return result_size;
    }
    }
}
```

#### 3.3. Go RPC Bridge (CGO)
```go
//export go_get_account
func go_get_account(address *C.char, out *C.char, max_size C.size_t) C.size_t {
    addr := C.GoString(address)
    
    // Call Go RPC client
    account, err := rpcClient.GetAccount(ctx, addr)
    if err != nil {
        return 0
    }
    
    // Serialize to JSON
    jsonData, _ := json.Marshal(account)
    
    // Copy to output
    C.memcpy(unsafe.Pointer(out), unsafe.Pointer(&jsonData[0]), C.size_t(len(jsonData)))
    return C.size_t(len(jsonData))
}
```

---

## Giai đoạn 4: Full Integration với metaCoSign 🎯

### Mục tiêu:
eEVM trong TEE execute smart contract, state từ Go blockchain

### Flow hoàn chỉnh:

```
1. User sends transaction
   ↓
2. Go RPC receives transaction
   ↓
3. Go calls TEE (via tee-supplicant)
   ↓
4. eEVM TA loads contract bytecode
   ↓
5. eEVM.run() executes contract
   │
   ├─ Need account? → OCALL → Supplicant → Go RPC → DB
   ├─ Need storage? → OCALL → Supplicant → Go RPC → DB
   │
6. eEVM returns result to Go
   ↓
7. Go saves state changes to DB
   ↓
8. Go returns result to user
```

### Performance Optimizations:

1. **State Cache**: Cache frequently accessed accounts in Normal World
2. **Batch OCALL**: Group multiple storage reads into one OCALL
3. **Prefetch**: Predict and prefetch needed state
4. **Async OCALL**: Non-blocking calls for independent operations

---

## 📊 Comparison Table

| Feature | Hello C++ | eEVM TA | eEVM + OCALL | Full Integration |
|---------|-----------|---------|--------------|------------------|
| C++ Runtime | ✅ | ✅ | ✅ | ✅ |
| STL Support | ✅ | ✅ | ✅ | ✅ |
| EVM Execution | ❌ | ✅ | ✅ | ✅ |
| Blockchain State | ❌ | In-memory only | Via OCALL | Full DB access |
| Go Integration | ❌ | ❌ | Manual | Automated |
| Production Ready | ❌ | ❌ | ❌ | ✅ |

---

## 🚀 Quick Commands

### Giai đoạn 1 (Current):
```bash
cd /home/abc/nhat/optee_examples/hello_cpp
./install_toolchain.sh    # Lần đầu tiên
./quickstart.sh           # Build
./deploy.sh pi@<IP>       # Deploy
./run_test.sh pi@<IP>     # Test
```

### Giai đoạn 2 (Next):
```bash
cd /home/abc/nhat/optee_examples
cp -r hello_cpp evm_ta    # Copy template
cd evm_ta
# Modify ta/evm_ta.cpp to include eEVM
./build.sh
```

### Giai đoạn 3 (After eEVM works):
```bash
# Apply OCALL patches
cd /home/abc/nhat/optee_examples/docs
./setup_rpc_bridge.sh
./patch_rpc_cores.sh
# Rebuild OP-TEE OS
```

---

## 🐛 Troubleshooting

### TA build fails
```bash
# Check TA_DEV_KIT_DIR
ls -l $TA_DEV_KIT_DIR/mk/ta_dev_kit.mk

# Rebuild OP-TEE OS if needed
cd /home/abc/optee_os
make clean
make PLATFORM=vexpress-qemu_armv8a CFG_ARM64_core=y -j$(nproc)
```

### Host can't connect to TA
```bash
# On Pi, check tee-supplicant
sudo systemctl status tee-supplicant
sudo systemctl restart tee-supplicant

# Check TA is deployed
ls -l /lib/optee_armtz/*.ta
```

### eEVM crashes
```bash
# Check stack size in user_ta_header_defines.h
#define TA_STACK_SIZE  (512 * 1024)  // Increase if needed
#define TA_DATA_SIZE   (2 * 1024 * 1024)  // Increase for large contracts
```

---

## 📚 Documentation References

- [Hello C++ README](README.md) - Chi tiết về hello_cpp
- [cpp_integration_ocall_guide.md](../docs/cpp_integration_ocall_guide.md) - OCALL implementation
- [eEVM Documentation](https://github.com/microsoft/eEVM)
- [OP-TEE Documentation](https://optee.readthedocs.io/)

---

**Current Status**: ✅ Ready to build and test hello_cpp  
**Next Step**: Install toolchain → Build → Test on Pi 5
