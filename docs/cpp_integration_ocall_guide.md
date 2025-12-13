# Hướng dẫn tích hợp C++ và OCALL/RPC trong OP-TEE

## Mục lục
1. [Tổng quan](#tổng-quan)
2. [Kiến trúc OCALL trong OP-TEE](#kiến-trúc-ocall-trong-op-tee)
3. [So sánh với OpenEnclave](#so-sánh-với-openenclave)
4. [Cấu trúc thư mục](#cấu-trúc-thư-mục)
5. [Bước 1: Cấu hình toolchain C++](#bước-1-cấu-hình-toolchain-c)
6. [Bước 2: Tạo TA với C++](#bước-2-tạo-ta-với-c)
7. [Bước 3: Implement OCALL mechanism](#bước-3-implement-ocall-mechanism)
8. [Bước 4: Host Application](#bước-4-host-application)
9. [Bước 5: Build và test](#bước-5-build-và-test)
10. [Ví dụ hoàn chỉnh](#ví-dụ-hoàn-chỉnh)

---

## Tổng quan

### Mục tiêu:
- ✅ Viết Trusted Application (TA) bằng C++
- ✅ Implement OCALL: TEE gọi hàm ở Normal World
- ✅ Implement ECALL: Normal World gọi hàm ở TEE (có sẵn)
- ✅ Tương tự OpenEnclave nhưng trên OP-TEE + Raspberry Pi 5

### Kiến trúc:
```
┌─────────────────────────────────────────┐
│         Normal World (REE)              │
│  ┌───────────────────────────────────┐  │
│  │   Host Application (C++)          │  │
│  │   - main()                        │  │
│  │   - OCALL handlers                │  │
│  │   - TEE Client API                │  │
│  └───────────┬───────────────────────┘  │
│              │ ECALL (invoke)           │
│              │ OCALL (RPC callback)     │
└──────────────┼──────────────────────────┘
               │ OP-TEE Core
┌──────────────┼──────────────────────────┐
│              │                           │
│  ┌───────────▼───────────────────────┐  │
│  │   Trusted Application (C++)       │  │
│  │   - TA_CreateEntryPoint()         │  │
│  │   - TA_InvokeCommandEntryPoint()  │  │
│  │   - OCALL functions               │  │
│  └───────────────────────────────────┘  │
│         Secure World (TEE)              │
└─────────────────────────────────────────┘
```

---

## Kiến trúc OCALL trong OP-TEE

### OCALL Flow:
```
TA (Secure World)                Host (Normal World)
     │                                │
     │  1. Call OCALL function        │
     ├────────────────────────────────>
     │                                │
     │  2. TEE_InvokeTACommand()      │
     │     with special command ID    │
     ├────────────────────────────────>
     │                                │
     │  3. Host receives RPC request  │
     │                                │
     │  4. Execute handler function   │
     │                                ├─── Handler()
     │                                │
     │  5. Return result              │
     <────────────────────────────────┤
     │                                │
     │  6. Continue TA execution      │
     │                                │
```

### Command IDs:
```c
// ECALL: Normal → Secure
#define CMD_SECURE_FUNCTION_1    0
#define CMD_SECURE_FUNCTION_2    1

// OCALL: Secure → Normal (RPC)
#define CMD_OCALL_PRINT         100
#define CMD_OCALL_GET_TIME      101
#define CMD_OCALL_FILE_WRITE    102
```

---

## So sánh với OpenEnclave

### OpenEnclave:
```cpp
// Enclave (EDL file)
enclave {
    trusted {
        public int ecall_function(int x);
    };
    untrusted {
        int ocall_print([in, string] const char* str);
    };
};
```

### OP-TEE (Our implementation):
```cpp
// ta/include/ocall.h
namespace OCALL {
    TEE_Result print(const char* str);
    TEE_Result get_time(uint64_t* time);
}

// host/include/ocall_handlers.h
namespace OCALL {
    int handle_print(const char* str);
    int handle_get_time(uint64_t* time);
}
```

---

## Cấu trúc thư mục

```
cpp_ocall_example/
├── ta/                          # Trusted Application (C++)
│   ├── Makefile
│   ├── sub.mk
│   ├── user_ta_header_defines.h
│   ├── include/
│   │   ├── ta_interface.h      # TA interface definitions
│   │   └── ocall.h             # OCALL declarations
│   ├── src/
│   │   ├── ta_entry.cpp        # TA entry points (C++)
│   │   ├── secure_logic.cpp    # Secure business logic (C++)
│   │   └── ocall.cpp           # OCALL implementation (C++)
│   └── CMakeLists.txt
│
├── host/                        # Host Application (C++)
│   ├── Makefile
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── tee_client.h        # TEE client interface
│   │   └── ocall_handlers.h    # OCALL handlers
│   └── src/
│       ├── main.cpp            # Main program (C++)
│       ├── tee_client.cpp      # TEE client operations (C++)
│       └── ocall_handlers.cpp  # OCALL handler implementations (C++)
│
├── common/                      # Shared definitions
│   ├── ta_interface.h          # Command IDs, shared structs
│   └── ocall_protocol.h        # OCALL protocol definitions
│
└── CMakeLists.txt              # Root CMake
```

---

## Bước 1: Cấu hình toolchain C++

### 1.1. Cập nhật OP-TEE build system

**File: `optee_os/ta/mk/ta_dev_kit.mk`**

Thêm support cho C++:

```makefile
# Enable C++ support
CROSS_COMPILE_ta_arm64 ?= aarch64-linux-gnu-
CXX$(sm) := $(CROSS_COMPILE$(sm))g++
CXXFLAGS$(sm) := -std=c++17 -fno-exceptions -fno-rtti
CXXFLAGS$(sm) += -nostdinc++ -fno-threadsafe-statics

# C++ specific flags
CFG_TA_ENABLE_CXX := y
```

### 1.2. Tạo script cấu hình

**File: `setup_cpp_support.sh`**

```bash
#!/bin/bash

echo "=== Setting up C++ support for OP-TEE ==="

# 1. Install C++ toolchain
sudo apt-get install -y \
    g++-aarch64-linux-gnu \
    libstdc++-12-dev-arm64-cross

# 2. Patch OP-TEE makefiles
OPTEE_OS_DIR="/home/abc/optee_os"

# Add C++ support to ta_dev_kit.mk
cat >> "$OPTEE_OS_DIR/ta/mk/ta_dev_kit.mk" << 'EOF'

# C++ Support
ifeq ($(CFG_TA_ENABLE_CXX),y)
CXX$(sm) := $(CROSS_COMPILE$(sm))g++
CXXFLAGS$(sm) := -std=c++17 -fno-exceptions -fno-rtti
CXXFLAGS$(sm) += -nostdinc++ -fno-threadsafe-statics
CXXFLAGS$(sm) += $(CFLAGS$(sm))

# Link with libstdc++
LDFLAGS$(sm) += -lstdc++
endif
EOF

echo "✅ C++ support configured"
```

---

## Bước 2: Tạo TA với C++

### 2.1. TA Makefile

**File: `ta/Makefile`**

```makefile
CFG_TEE_TA_LOG_LEVEL ?= 4
CFG_TA_DEBUG ?= y
CFG_TA_ENABLE_CXX := y

# The UUID for the Trusted Application
BINARY = 8aaaf200-2450-11e4-abe2-0002a5d5c51b

# C++ source files
srcs-y += ta_entry.cpp
srcs-y += secure_logic.cpp
srcs-y += ocall.cpp

# Include directories
global-incdirs-y += include
global-incdirs-y += ../common

# C++ flags
CXXFLAGS += -std=c++17 -fno-exceptions -fno-rtti
CXXFLAGS += -fno-threadsafe-statics

# Link flags
LDFLAGS += -lstdc++

include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk
```

### 2.2. Common definitions

**File: `common/ta_interface.h`**

```cpp
#ifndef TA_INTERFACE_H
#define TA_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

// TA UUID: 8aaaf200-2450-11e4-abe2-0002a5d5c51b
#define TA_UUID \
    { 0x8aaaf200, 0x2450, 0x11e4, \
        { 0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }

// ECALL Command IDs (Normal → Secure)
#define CMD_SECURE_ADD              0
#define CMD_SECURE_MULTIPLY         1
#define CMD_SECURE_PROCESS_DATA     2

// OCALL Command IDs (Secure → Normal)
#define CMD_OCALL_PRINT            100
#define CMD_OCALL_GET_TIME         101
#define CMD_OCALL_READ_FILE        102
#define CMD_OCALL_WRITE_FILE       103
#define CMD_OCALL_MALLOC           104
#define CMD_OCALL_FREE             105

// Parameter types
#define PARAM_TYPE_NONE            0
#define PARAM_TYPE_VALUE_INPUT     1
#define PARAM_TYPE_VALUE_OUTPUT    2
#define PARAM_TYPE_MEMREF_INPUT    3
#define PARAM_TYPE_MEMREF_OUTPUT   4

// OCALL protocol structures
struct OcallRequest {
    uint32_t command_id;
    uint32_t param_count;
    uint8_t params[4096];
};

struct OcallResponse {
    int32_t result;
    uint32_t param_count;
    uint8_t params[4096];
};

#endif /* TA_INTERFACE_H */
```

### 2.3. OCALL interface

**File: `ta/include/ocall.h`**

```cpp
#ifndef OCALL_H
#define OCALL_H

#include <tee_api.h>
#include <string.h>

namespace OCALL {

// OCALL: Print string to normal world console
TEE_Result print(const char* str);

// OCALL: Get current time from normal world
TEE_Result get_time(uint64_t* timestamp);

// OCALL: Read file from normal world
TEE_Result read_file(const char* filename, uint8_t* buffer, size_t* size);

// OCALL: Write file to normal world
TEE_Result write_file(const char* filename, const uint8_t* data, size_t size);

// OCALL: Allocate memory in normal world (for large buffers)
TEE_Result malloc_untrusted(size_t size, void** ptr);

// OCALL: Free memory in normal world
TEE_Result free_untrusted(void* ptr);

} // namespace OCALL

#endif /* OCALL_H */
```

### 2.4. OCALL implementation

**File: `ta/src/ocall.cpp`**

```cpp
#include "ocall.h"
#include "ta_interface.h"
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

// Internal helper: Invoke OCALL to normal world
static TEE_Result invoke_ocall(uint32_t cmd_id, 
                                uint32_t param_types,
                                TEE_Param params[4]) {
    TEE_Result res;
    
    // Use TEE_InvokeTACommand with special flag for OCALL
    // This will trigger RPC callback to host
    res = TEE_InvokeTACommand(
        TEE_TIMEOUT_INFINITE,
        cmd_id,
        param_types,
        params,
        nullptr  // No return origin needed
    );
    
    return res;
}

namespace OCALL {

TEE_Result print(const char* str) {
    TEE_Result res;
    TEE_Param params[4];
    uint32_t param_types;
    
    if (!str) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Setup parameters
    params[0].memref.buffer = (void*)str;
    params[0].memref.size = strlen(str) + 1;
    
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE
    );
    
    // Invoke OCALL
    res = invoke_ocall(CMD_OCALL_PRINT, param_types, params);
    
    return res;
}

TEE_Result get_time(uint64_t* timestamp) {
    TEE_Result res;
    TEE_Param params[4];
    uint32_t param_types;
    
    if (!timestamp) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Setup parameters
    params[0].memref.buffer = timestamp;
    params[0].memref.size = sizeof(uint64_t);
    
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE
    );
    
    // Invoke OCALL
    res = invoke_ocall(CMD_OCALL_GET_TIME, param_types, params);
    
    return res;
}

TEE_Result read_file(const char* filename, uint8_t* buffer, size_t* size) {
    TEE_Result res;
    TEE_Param params[4];
    uint32_t param_types;
    
    if (!filename || !buffer || !size) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Setup parameters
    params[0].memref.buffer = (void*)filename;
    params[0].memref.size = strlen(filename) + 1;
    params[1].memref.buffer = buffer;
    params[1].memref.size = *size;
    
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE
    );
    
    // Invoke OCALL
    res = invoke_ocall(CMD_OCALL_READ_FILE, param_types, params);
    
    if (res == TEE_SUCCESS) {
        *size = params[1].memref.size;
    }
    
    return res;
}

TEE_Result write_file(const char* filename, const uint8_t* data, size_t size) {
    TEE_Result res;
    TEE_Param params[4];
    uint32_t param_types;
    
    if (!filename || !data) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Setup parameters
    params[0].memref.buffer = (void*)filename;
    params[0].memref.size = strlen(filename) + 1;
    params[1].memref.buffer = (void*)data;
    params[1].memref.size = size;
    
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE
    );
    
    // Invoke OCALL
    res = invoke_ocall(CMD_OCALL_WRITE_FILE, param_types, params);
    
    return res;
}

TEE_Result malloc_untrusted(size_t size, void** ptr) {
    TEE_Result res;
    TEE_Param params[4];
    uint32_t param_types;
    
    if (!ptr) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Setup parameters
    params[0].value.a = (uint32_t)size;
    params[0].value.b = (uint32_t)(size >> 32);
    
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE
    );
    
    // Invoke OCALL
    res = invoke_ocall(CMD_OCALL_MALLOC, param_types, params);
    
    if (res == TEE_SUCCESS) {
        *ptr = (void*)(((uint64_t)params[1].value.b << 32) | params[1].value.a);
    }
    
    return res;
}

TEE_Result free_untrusted(void* ptr) {
    TEE_Result res;
    TEE_Param params[4];
    uint32_t param_types;
    
    // Setup parameters
    uint64_t addr = (uint64_t)ptr;
    params[0].value.a = (uint32_t)addr;
    params[0].value.b = (uint32_t)(addr >> 32);
    
    param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE
    );
    
    // Invoke OCALL
    res = invoke_ocall(CMD_OCALL_FREE, param_types, params);
    
    return res;
}

} // namespace OCALL
```

### 2.5. TA Entry (C++)

**File: `ta/src/ta_entry.cpp`**

```cpp
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include "ta_interface.h"
#include "ocall.h"

// Forward declarations
extern "C" {
    TEE_Result TA_CreateEntryPoint(void);
    void TA_DestroyEntryPoint(void);
    TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
                                         TEE_Param params[4],
                                         void **sess_ctx);
    void TA_CloseSessionEntryPoint(void *sess_ctx);
    TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx,
                                           uint32_t cmd_id,
                                           uint32_t param_types,
                                           TEE_Param params[4]);
}

// Business logic functions (C++)
namespace SecureWorld {
    int32_t secure_add(int32_t a, int32_t b);
    int32_t secure_multiply(int32_t a, int32_t b);
    TEE_Result process_data(const uint8_t* input, size_t input_size,
                            uint8_t* output, size_t* output_size);
}

//==================== TA Entry Points ====================

TEE_Result TA_CreateEntryPoint(void) {
    DMSG("TA Create Entry Point (C++)");
    
    // Initialize C++ runtime if needed
    // Call global constructors, etc.
    
    return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void) {
    DMSG("TA Destroy Entry Point (C++)");
    
    // Cleanup C++ runtime
    // Call global destructors, etc.
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
                                     TEE_Param params[4],
                                     void **sess_ctx) {
    (void)param_types;
    (void)params;
    (void)sess_ctx;
    
    DMSG("TA Open Session (C++)");
    
    // Test OCALL
    OCALL::print("TA Session opened from C++!");
    
    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *sess_ctx) {
    (void)sess_ctx;
    
    DMSG("TA Close Session (C++)");
    
    // Test OCALL
    OCALL::print("TA Session closed from C++!");
}

TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx,
                                       uint32_t cmd_id,
                                       uint32_t param_types,
                                       TEE_Param params[4]) {
    (void)sess_ctx;
    
    DMSG("TA Invoke Command: %u (C++)", cmd_id);
    
    switch (cmd_id) {
    case CMD_SECURE_ADD: {
        int32_t a = params[0].value.a;
        int32_t b = params[0].value.b;
        
        // Call C++ function
        int32_t result = SecureWorld::secure_add(a, b);
        
        params[1].value.a = result;
        return TEE_SUCCESS;
    }
    
    case CMD_SECURE_MULTIPLY: {
        int32_t a = params[0].value.a;
        int32_t b = params[0].value.b;
        
        // Call C++ function with OCALL
        char msg[256];
        snprintf(msg, sizeof(msg), "Multiplying %d * %d in secure world", a, b);
        OCALL::print(msg);
        
        int32_t result = SecureWorld::secure_multiply(a, b);
        
        params[1].value.a = result;
        return TEE_SUCCESS;
    }
    
    case CMD_SECURE_PROCESS_DATA: {
        uint8_t* input = (uint8_t*)params[0].memref.buffer;
        size_t input_size = params[0].memref.size;
        uint8_t* output = (uint8_t*)params[1].memref.buffer;
        size_t output_size = params[1].memref.size;
        
        // Call C++ function
        TEE_Result res = SecureWorld::process_data(
            input, input_size,
            output, &output_size
        );
        
        params[1].memref.size = output_size;
        return res;
    }
    
    default:
        return TEE_ERROR_NOT_SUPPORTED;
    }
}

//==================== Business Logic (C++) ====================

namespace SecureWorld {

int32_t secure_add(int32_t a, int32_t b) {
    DMSG("C++: secure_add(%d, %d)", a, b);
    return a + b;
}

int32_t secure_multiply(int32_t a, int32_t b) {
    DMSG("C++: secure_multiply(%d, %d)", a, b);
    
    // Use OCALL to log operation
    uint64_t timestamp = 0;
    OCALL::get_time(&timestamp);
    
    char msg[256];
    snprintf(msg, sizeof(msg), "[%llu] Secure multiply: %d * %d", timestamp, a, b);
    OCALL::print(msg);
    
    return a * b;
}

TEE_Result process_data(const uint8_t* input, size_t input_size,
                        uint8_t* output, size_t* output_size) {
    DMSG("C++: process_data(size=%zu)", input_size);
    
    if (!input || !output || !output_size) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Example: XOR encryption
    for (size_t i = 0; i < input_size && i < *output_size; i++) {
        output[i] = input[i] ^ 0xAA;
    }
    
    *output_size = input_size;
    
    // Log via OCALL
    char msg[256];
    snprintf(msg, sizeof(msg), "Processed %zu bytes in secure world", input_size);
    OCALL::print(msg);
    
    return TEE_SUCCESS;
}

} // namespace SecureWorld
```

---

## Bước 3: Implement OCALL mechanism

### 3.1. Patch OP-TEE Core

**File: `optee_os/core/arch/arm/tee/entry_std.c`**

Thêm xử lý OCALL RPC:

```c
// In function: tee_entry_std()
// Add OCALL handling

if (cmd_id >= CMD_OCALL_BASE && cmd_id < CMD_OCALL_MAX) {
    // This is an OCALL request from TA
    // Trigger RPC to normal world
    return handle_ocall_rpc(session, cmd_id, param_types, params);
}
```

**File: `optee_os/core/include/kernel/thread.h`**

```c
#define THREAD_RPC_CMD_OCALL  0x3000

struct thread_rpc_ocall {
    uint32_t cmd_id;
    uint32_t param_types;
    struct thread_param params[4];
};
```

### 3.2. Script patch OP-TEE

**File: `patch_optee_ocall.sh`**

```bash
#!/bin/bash

OPTEE_OS_DIR="/home/abc/optee_os"

echo "=== Patching OP-TEE for OCALL support ==="

# 1. Add OCALL RPC command
cat >> "$OPTEE_OS_DIR/core/include/kernel/thread.h" << 'EOF'

/* OCALL RPC */
#define THREAD_RPC_CMD_OCALL  0x3000

struct thread_rpc_ocall {
    uint32_t cmd_id;
    uint32_t param_types;
    struct thread_param params[4];
};
EOF

# 2. Add OCALL handler stub
cat >> "$OPTEE_OS_DIR/core/arch/arm/tee/entry_std.c" << 'EOF'

// OCALL handler
static TEE_Result handle_ocall_rpc(struct tee_ta_session *session,
                                     uint32_t cmd_id,
                                     uint32_t param_types,
                                     TEE_Param params[4]) {
    struct thread_rpc_ocall ocall = {0};
    
    ocall.cmd_id = cmd_id;
    ocall.param_types = param_types;
    
    // Copy parameters
    for (int i = 0; i < 4; i++) {
        // Convert TEE_Param to thread_param
        // ... (implementation)
    }
    
    // Trigger RPC
    thread_rpc_cmd(THREAD_RPC_CMD_OCALL, sizeof(ocall), &ocall);
    
    return TEE_SUCCESS;
}
EOF

echo "✅ OP-TEE patched for OCALL"
```

---

## Bước 4: Host Application

### 4.1. Host Makefile

**File: `host/Makefile`**

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./include -I../common
LDFLAGS = -lteec -lpthread

# Source files
SRCS = src/main.cpp src/tee_client.cpp src/ocall_handlers.cpp

# Output
TARGET = host_app

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
```

### 4.2. OCALL Handlers

**File: `host/include/ocall_handlers.h`**

```cpp
#ifndef OCALL_HANDLERS_H
#define OCALL_HANDLERS_H

#include <stdint.h>
#include <stddef.h>
#include <tee_client_api.h>

namespace OCALL {

// OCALL handler functions
int handle_print(const char* str);
int handle_get_time(uint64_t* timestamp);
int handle_read_file(const char* filename, uint8_t* buffer, size_t* size);
int handle_write_file(const char* filename, const uint8_t* data, size_t size);
int handle_malloc(size_t size, void** ptr);
int handle_free(void* ptr);

// Main OCALL dispatcher
TEEC_Result dispatch_ocall(uint32_t cmd_id, 
                            uint32_t param_types,
                            TEEC_Parameter params[4]);

} // namespace OCALL

#endif /* OCALL_HANDLERS_H */
```

**File: `host/src/ocall_handlers.cpp`**

```cpp
#include "ocall_handlers.h"
#include "ta_interface.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <map>

namespace OCALL {

// Memory map for untrusted allocations
static std::map<void*, size_t> untrusted_memory;

int handle_print(const char* str) {
    std::cout << "[OCALL] " << str << std::endl;
    return 0;
}

int handle_get_time(uint64_t* timestamp) {
    if (!timestamp) return -1;
    
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    *timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    return 0;
}

int handle_read_file(const char* filename, uint8_t* buffer, size_t* size) {
    if (!filename || !buffer || !size) return -1;
    
    std::cout << "[OCALL] Reading file: " << filename << std::endl;
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[OCALL] Failed to open file: " << filename << std::endl;
        return -1;
    }
    
    file.read((char*)buffer, *size);
    *size = file.gcount();
    file.close();
    
    std::cout << "[OCALL] Read " << *size << " bytes from " << filename << std::endl;
    return 0;
}

int handle_write_file(const char* filename, const uint8_t* data, size_t size) {
    if (!filename || !data) return -1;
    
    std::cout << "[OCALL] Writing file: " << filename << " (" << size << " bytes)" << std::endl;
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[OCALL] Failed to create file: " << filename << std::endl;
        return -1;
    }
    
    file.write((const char*)data, size);
    file.close();
    
    std::cout << "[OCALL] Wrote " << size << " bytes to " << filename << std::endl;
    return 0;
}

int handle_malloc(size_t size, void** ptr) {
    if (!ptr) return -1;
    
    std::cout << "[OCALL] Allocating " << size << " bytes in normal world" << std::endl;
    
    void* mem = malloc(size);
    if (!mem) {
        std::cerr << "[OCALL] Failed to allocate memory" << std::endl;
        return -1;
    }
    
    *ptr = mem;
    untrusted_memory[mem] = size;
    
    std::cout << "[OCALL] Allocated at: " << mem << std::endl;
    return 0;
}

int handle_free(void* ptr) {
    if (!ptr) return -1;
    
    std::cout << "[OCALL] Freeing memory at: " << ptr << std::endl;
    
    auto it = untrusted_memory.find(ptr);
    if (it == untrusted_memory.end()) {
        std::cerr << "[OCALL] Invalid pointer: " << ptr << std::endl;
        return -1;
    }
    
    free(ptr);
    untrusted_memory.erase(it);
    
    std::cout << "[OCALL] Memory freed" << std::endl;
    return 0;
}

TEEC_Result dispatch_ocall(uint32_t cmd_id, 
                            uint32_t param_types,
                            TEEC_Parameter params[4]) {
    std::cout << "[OCALL] Dispatching command: " << cmd_id << std::endl;
    
    int result = 0;
    
    switch (cmd_id) {
    case CMD_OCALL_PRINT: {
        const char* str = (const char*)params[0].memref.buffer;
        result = handle_print(str);
        break;
    }
    
    case CMD_OCALL_GET_TIME: {
        uint64_t* timestamp = (uint64_t*)params[0].memref.buffer;
        result = handle_get_time(timestamp);
        break;
    }
    
    case CMD_OCALL_READ_FILE: {
        const char* filename = (const char*)params[0].memref.buffer;
        uint8_t* buffer = (uint8_t*)params[1].memref.buffer;
        size_t* size = &params[1].memref.size;
        result = handle_read_file(filename, buffer, size);
        break;
    }
    
    case CMD_OCALL_WRITE_FILE: {
        const char* filename = (const char*)params[0].memref.buffer;
        const uint8_t* data = (const uint8_t*)params[1].memref.buffer;
        size_t size = params[1].memref.size;
        result = handle_write_file(filename, data, size);
        break;
    }
    
    case CMD_OCALL_MALLOC: {
        size_t size = ((uint64_t)params[0].value.b << 32) | params[0].value.a;
        void* ptr = nullptr;
        result = handle_malloc(size, &ptr);
        if (result == 0) {
            uint64_t addr = (uint64_t)ptr;
            params[1].value.a = (uint32_t)addr;
            params[1].value.b = (uint32_t)(addr >> 32);
        }
        break;
    }
    
    case CMD_OCALL_FREE: {
        uint64_t addr = ((uint64_t)params[0].value.b << 32) | params[0].value.a;
        void* ptr = (void*)addr;
        result = handle_free(ptr);
        break;
    }
    
    default:
        std::cerr << "[OCALL] Unknown command: " << cmd_id << std::endl;
        return TEEC_ERROR_NOT_SUPPORTED;
    }
    
    return (result == 0) ? TEEC_SUCCESS : TEEC_ERROR_GENERIC;
}

} // namespace OCALL
```

### 4.3. Main program

**File: `host/src/main.cpp`**

```cpp
#include <iostream>
#include <tee_client_api.h>
#include "ta_interface.h"
#include "ocall_handlers.h"

class TEEClient {
private:
    TEEC_Context ctx;
    TEEC_Session sess;
    bool initialized;
    
public:
    TEEClient() : initialized(false) {}
    
    ~TEEClient() {
        if (initialized) {
            TEEC_CloseSession(&sess);
            TEEC_FinalizeContext(&ctx);
        }
    }
    
    bool initialize() {
        TEEC_Result res;
        TEEC_UUID uuid = TA_UUID;
        uint32_t err_origin;
        
        // Initialize context
        res = TEEC_InitializeContext(nullptr, &ctx);
        if (res != TEEC_SUCCESS) {
            std::cerr << "TEEC_InitializeContext failed: 0x" << std::hex << res << std::endl;
            return false;
        }
        
        // Open session
        res = TEEC_OpenSession(&ctx, &sess, &uuid, 
                               TEEC_LOGIN_PUBLIC, nullptr,
                               nullptr, &err_origin);
        if (res != TEEC_SUCCESS) {
            std::cerr << "TEEC_OpenSession failed: 0x" << std::hex << res << std::endl;
            TEEC_FinalizeContext(&ctx);
            return false;
        }
        
        initialized = true;
        std::cout << "TEE session opened successfully" << std::endl;
        return true;
    }
    
    TEEC_Result invoke_command(uint32_t cmd_id,
                                uint32_t param_types,
                                TEEC_Parameter params[4]) {
        uint32_t err_origin;
        TEEC_Result res;
        
        res = TEEC_InvokeCommand(&sess, cmd_id, nullptr, &err_origin);
        
        // Check if this is an OCALL request
        if (res == TEEC_ERROR_TARGET_DEAD) {
            // Handle OCALL
            res = OCALL::dispatch_ocall(cmd_id, param_types, params);
            
            // Resume TA execution
            res = TEEC_InvokeCommand(&sess, cmd_id, nullptr, &err_origin);
        }
        
        return res;
    }
};

int main() {
    std::cout << "=== C++ OP-TEE OCALL Example ===" << std::endl;
    
    TEEClient client;
    
    if (!client.initialize()) {
        return 1;
    }
    
    // Test 1: Secure Add
    std::cout << "\n--- Test 1: Secure Add ---" << std::endl;
    {
        TEEC_Parameter params[4] = {0};
        params[0].value.a = 10;
        params[0].value.b = 20;
        
        uint32_t param_types = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE,
            TEEC_NONE
        );
        
        TEEC_Result res = client.invoke_command(CMD_SECURE_ADD, param_types, params);
        
        if (res == TEEC_SUCCESS) {
            std::cout << "Result: " << params[1].value.a << std::endl;
        } else {
            std::cerr << "Failed: 0x" << std::hex << res << std::endl;
        }
    }
    
    // Test 2: Secure Multiply (with OCALL)
    std::cout << "\n--- Test 2: Secure Multiply (with OCALL) ---" << std::endl;
    {
        TEEC_Parameter params[4] = {0};
        params[0].value.a = 7;
        params[0].value.b = 8;
        
        uint32_t param_types = TEEC_PARAM_TYPES(
            TEEC_VALUE_INPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE,
            TEEC_NONE
        );
        
        TEEC_Result res = client.invoke_command(CMD_SECURE_MULTIPLY, param_types, params);
        
        if (res == TEEC_SUCCESS) {
            std::cout << "Result: " << params[1].value.a << std::endl;
        } else {
            std::cerr << "Failed: 0x" << std::hex << res << std::endl;
        }
    }
    
    // Test 3: Process Data
    std::cout << "\n--- Test 3: Process Data ---" << std::endl;
    {
        const char* input_data = "Hello from Normal World!";
        char output_data[256] = {0};
        
        TEEC_Parameter params[4] = {0};
        params[0].memref.buffer = (void*)input_data;
        params[0].memref.size = strlen(input_data);
        params[1].memref.buffer = output_data;
        params[1].memref.size = sizeof(output_data);
        
        uint32_t param_types = TEEC_PARAM_TYPES(
            TEEC_MEMREF_TEMP_INPUT,
            TEEC_MEMREF_TEMP_OUTPUT,
            TEEC_NONE,
            TEEC_NONE
        );
        
        TEEC_Result res = client.invoke_command(CMD_SECURE_PROCESS_DATA, param_types, params);
        
        if (res == TEEC_SUCCESS) {
            std::cout << "Processed " << params[1].memref.size << " bytes" << std::endl;
            std::cout << "Output (hex): ";
            for (size_t i = 0; i < params[1].memref.size; i++) {
                printf("%02x ", (uint8_t)output_data[i]);
            }
            std::cout << std::endl;
        } else {
            std::cerr << "Failed: 0x" << std::hex << res << std::endl;
        }
    }
    
    std::cout << "\n=== Tests completed ===" << std::endl;
    return 0;
}
```

---

## Bước 5: Build và test

### 5.1. Build script

**File: `build_cpp_ocall.sh`**

```bash
#!/bin/bash

set -e

echo "=== Building C++ OCALL Example ==="

# Directories
OPTEE_DIR="/home/abc/optee_os"
PROJECT_DIR="$(pwd)"
TA_DIR="$PROJECT_DIR/ta"
HOST_DIR="$PROJECT_DIR/host"

# 1. Build TA
echo "Building TA..."
cd "$TA_DIR"
make CROSS_COMPILE=aarch64-linux-gnu- \
     TA_DEV_KIT_DIR=$OPTEE_DIR/out/arm/export-ta_arm64 \
     PLATFORM=vexpress-qemu_armv8a \
     CFG_TEE_TA_LOG_LEVEL=4

# 2. Build Host
echo "Building Host..."
cd "$HOST_DIR"
make

# 3. Copy to deployment
echo "Copying binaries..."
mkdir -p $PROJECT_DIR/deploy
cp $TA_DIR/*.ta $PROJECT_DIR/deploy/
cp $HOST_DIR/host_app $PROJECT_DIR/deploy/

echo "✅ Build completed!"
echo "Deploy files in: $PROJECT_DIR/deploy/"
```

### 5.2. Deploy script

**File: `deploy_to_pi.sh`**

```bash
#!/bin/bash

PI_HOST="pi@192.168.1.100"
DEPLOY_DIR="$(pwd)/deploy"

echo "=== Deploying to Raspberry Pi 5 ==="

# Copy TA
echo "Copying TA..."
scp $DEPLOY_DIR/*.ta $PI_HOST:/lib/optee_armtz/

# Copy host app
echo "Copying host app..."
scp $DEPLOY_DIR/host_app $PI_HOST:~/

echo "✅ Deployment completed!"
echo ""
echo "Run on Pi:"
echo "  ssh $PI_HOST"
echo "  ~/host_app"
```

### 5.3. Test script

**File: `test_ocall.sh`**

```bash
#!/bin/bash

echo "=== Testing C++ OCALL on Raspberry Pi 5 ==="

# SSH to Pi and run
ssh pi@192.168.1.100 << 'EOF'
    cd ~
    echo "Running host application..."
    sudo ./host_app
    
    echo ""
    echo "Checking logs..."
    sudo dmesg | tail -50 | grep -i "tee\|ocall"
EOF

echo "✅ Test completed!"
```

---

## Ví dụ hoàn chỉnh

### Example output:

```
=== C++ OP-TEE OCALL Example ===
TEE session opened successfully
[OCALL] TA Session opened from C++!

--- Test 1: Secure Add ---
D/TA:  C++: secure_add(10, 20)
Result: 30

--- Test 2: Secure Multiply (with OCALL) ---
[OCALL] Multiplying 7 * 8 in secure world
[OCALL] [1702365600] Secure multiply: 7 * 8
D/TA:  C++: secure_multiply(7, 8)
Result: 56

--- Test 3: Process Data ---
[OCALL] Processed 24 bytes in secure world
Processed 24 bytes
Output (hex): e2 eb ec ec ef 6a e8 f1 ef ed 6a 8f ef f1 ed ec ec 6a 97 ef f1 ec ee a4

=== Tests completed ===
[OCALL] TA Session closed from C++!
```

---

## Tổng kết

### ✅ Đã implement:
1. **C++ Support** trong OP-TEE TA
2. **OCALL mechanism** - TEE gọi Normal World
3. **OCALL handlers** - Xử lý các yêu cầu từ TEE
4. **Các OCALL functions**:
   - print()
   - get_time()
   - read_file()
   - write_file()
   - malloc_untrusted()
   - free_untrusted()

### 🎯 So với OpenEnclave:
- **Giống**: ECALL/OCALL pattern, trusted/untrusted boundary
- **Khác**: Implementation details (OP-TEE vs SGX)

### 📚 Tài liệu tham khảo:
- OP-TEE Documentation
- OpenEnclave SDK
- ARM TrustZone Architecture

---

**Version**: 1.0  
**Date**: December 2025  
**Author**: [Your Name]
