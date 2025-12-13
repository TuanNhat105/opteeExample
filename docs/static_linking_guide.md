# Static Linking trong OP-TEE TA - Hướng dẫn chi tiết

## 🎯 Vấn đề: Tại sao cần Static Linking?

### TEE Environment đặc biệt:
```
┌─────────────────────────────────────┐
│   Normal World (Rich OS)            │
│   - Có shared libraries (.so)       │
│   - Dynamic linker hoạt động         │
│   - Có filesystem đầy đủ             │
└─────────────────────────────────────┘
              ↕ (Isolated)
┌─────────────────────────────────────┐
│   Secure World (TEE)                │
│   ❌ KHÔNG có shared libraries      │
│   ❌ KHÔNG có dynamic linker        │
│   ❌ KHÔNG có filesystem access     │
│   ✅ CHỈ chạy standalone binary     │
└─────────────────────────────────────┘
```

### Kết quả:
- **TA (.ta file)** phải chứa **TẤT CẢ** code cần thiết
- Không thể load `.so` files lúc runtime
- Phải **static link** tất cả libraries

---

## 📚 Các thư viện C++ chuẩn

### 1. libstdc++ (C++ Standard Library)

**Cung cấp:**
- `std::string`, `std::vector`, `std::map`
- `std::algorithm`, `std::sort`, `std::find`
- Memory management (new/delete operators)
- Exception handling (nếu enable)
- RTTI (nếu enable)

**File libraries:**
```
libstdc++.a       - Static library (cần cho TA)
libstdc++.so      - Shared library (không dùng được trong TA)
```

### 2. libgcc (GCC Runtime Library)

**Cung cấp:**
- Integer arithmetic helpers (64-bit operations)
- Floating-point operations
- Exception unwinding
- TLS (Thread Local Storage)

**File:** `libgcc.a`

### 3. libm (Math Library)

**Cung cấp:**
- `sin()`, `cos()`, `sqrt()`, `pow()`
- Floating-point math functions

**File:** `libm.a`

### 4. libc (C Standard Library)

**Cung cấp:**
- `malloc()`, `free()`, `memcpy()`, `strlen()`
- File I/O (không hoạt động trong TEE)
- Syscalls (thay bằng TEE API)

**Trong TEE:** OP-TEE cung cấp `libutee.a` thay thế

---

## 🔧 Cách Static Link trong TA

### Cấu trúc Makefile:

```makefile
# ta/sub.mk

# 1. C++ compiler flags
cppflags-y += -std=c++17
cppflags-y += -fno-exceptions    # Tắt exceptions để giảm size
cppflags-y += -fno-rtti          # Tắt RTTI để giảm size

# 2. Static linking flags
ldflags-y += -static             # Force static linking
ldflags-y += -lstdc++            # Link libstdc++ statically
ldflags-y += -lm                 # Link libm (math) statically
# ldflags-y += -lgcc             # Tự động link, không cần explicit

# 3. Nếu dùng thêm libraries khác
# ldflags-y += -lgmp             # GNU Multi-Precision (big integers)
# ldflags-y += -lmpfr            # Multi-Precision Floating-Point
# ldflags-y += -lcrypto          # OpenSSL crypto
```

### Giải thích:

#### `-static`
```bash
# Không có -static:
TA → tìm libstdc++.so lúc runtime → FAIL (TEE không có .so)

# Có -static:
TA → embed toàn bộ libstdc++.a vào .ta file → OK
```

#### `-lstdc++`
```bash
# Linker sẽ tìm:
1. libstdc++.a trong toolchain
2. Extract .o files cần thiết
3. Merge vào .ta binary
```

---

## 🛠️ Kiểm tra static linking

### 1. Check dependencies:

```bash
# Xem TA có dependencies không
aarch64-none-linux-gnu-readelf -d your_ta.ta

# Output mong muốn:
# (KHÔNG có NEEDED entries ngoài libutee)
```

### 2. Check symbols:

```bash
# Xem symbols được link
aarch64-none-linux-gnu-nm -C your_ta.ta | grep "std::"

# Ví dụ output:
# 00001234 T std::__cxx11::basic_string<...>::assign
# 00005678 T std::vector<...>::push_back
```

### 3. Check size:

```bash
ls -lh your_ta.ta

# Dynamic linking:  ~50KB
# Static linking:   ~200-500KB (lớn hơn nhiều!)
```

---

## 💡 Thực hành: Sử dụng STL trong TA

### Approach 1: Minimal STL (Recommended for TEE)

**File: `ta/hello_stl.cpp`**

```cpp
extern "C" {
#include <tee_internal_api.h>
}

// ❌ KHÔNG include full STL headers
// #include <string>
// #include <vector>

// ✅ Dùng C++ features cơ bản
namespace SimpleSTL {

// Custom string class (lightweight)
class String {
private:
    char* data_;
    size_t size_;
    
public:
    String(const char* str) {
        size_ = 0;
        while (str[size_]) size_++;
        data_ = new char[size_ + 1];
        for (size_t i = 0; i <= size_; i++) {
            data_[i] = str[i];
        }
    }
    
    ~String() {
        delete[] data_;
    }
    
    const char* c_str() const { return data_; }
    size_t length() const { return size_; }
};

// Custom vector class (lightweight)
template<typename T>
class Vector {
private:
    T* data_;
    size_t size_;
    size_t capacity_;
    
public:
    Vector() : data_(nullptr), size_(0), capacity_(0) {}
    
    ~Vector() {
        delete[] data_;
    }
    
    void push_back(const T& value) {
        if (size_ >= capacity_) {
            resize(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        data_[size_++] = value;
    }
    
    T& operator[](size_t index) {
        return data_[index];
    }
    
    size_t size() const { return size_; }
    
private:
    void resize(size_t new_capacity) {
        T* new_data = new T[new_capacity];
        for (size_t i = 0; i < size_; i++) {
            new_data[i] = data_[i];
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
    }
};

} // namespace SimpleSTL
```

**sub.mk:**
```makefile
cppflags-y += -std=c++17
cppflags-y += -fno-exceptions
cppflags-y += -fno-rtti

# Chỉ cần link libstdc++ cho new/delete operators
ldflags-y += -static -lstdc++
```

### Approach 2: Full STL với Newlib (Advanced)

Nếu muốn dùng **FULL STL** (`std::string`, `std::vector`), cần:

#### Bước 1: Build newlib cho ARM64

```bash
# Download newlib
wget ftp://sourceware.org/pub/newlib/newlib-4.3.0.tar.gz
tar xzf newlib-4.3.0.tar.gz
cd newlib-4.3.0

# Configure
./configure \
    --target=aarch64-none-elf \
    --prefix=$HOME/newlib-arm64 \
    --enable-newlib-nano-formatted-io \
    --enable-lite-exit \
    --enable-newlib-reent-small \
    --disable-newlib-fvwrite-in-streamio \
    --disable-newlib-multithread

# Build
make -j$(nproc)
make install
```

#### Bước 2: Link với newlib

**sub.mk:**
```makefile
cppflags-y += -std=c++17
cppflags-y += -fno-exceptions
cppflags-y += -fno-rtti

# Include newlib headers
NEWLIB_ROOT := $(HOME)/newlib-arm64
cppflags-y += -I$(NEWLIB_ROOT)/include
cppflags-y += -I$(NEWLIB_ROOT)/include/c++/v1

# Link statically
ldflags-y += -static
ldflags-y += -L$(NEWLIB_ROOT)/lib
ldflags-y += -lstdc++
ldflags-y += -lc
ldflags-y += -lm
```

#### Bước 3: Sử dụng STL

```cpp
extern "C" {
#include <tee_internal_api.h>
}

// Bây giờ có thể dùng STL!
#include <string>
#include <vector>
#include <algorithm>

TEE_Result test_stl() {
    std::string message = "Hello from STL!";
    DMSG("Message: %s", message.c_str());
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::sort(numbers.begin(), numbers.end());
    
    return TEE_SUCCESS;
}
```

---

## 🔍 Approach 3: Sử dụng eEVM approach

eEVM library đã solve vấn đề này bằng cách:

### 1. Compile all dependencies as static

**File: `eEVM/CMakeLists.txt`**

```cmake
# Build static library
add_library(eevm STATIC
    src/processor.cpp
    src/stack.cpp
    src/util.cpp
)

# Link dependencies statically
target_link_libraries(eevm
    PRIVATE
    intx::intx        # Static
    keccak            # Static
    nlohmann_json     # Header-only
)

# No shared libraries
set_target_properties(eevm PROPERTIES
    POSITION_INDEPENDENT_CODE OFF
)
```

### 2. Link vào TA

**File: `ta/sub.mk`**

```makefile
# Add eEVM library
EEVM_DIR := $(PROJECT_ROOT)/eEVM

# Include paths
global-incdirs-y += $(EEVM_DIR)/include
global-incdirs-y += $(EEVM_DIR)/3rdparty/intx/include
global-incdirs-y += $(EEVM_DIR)/3rdparty/nlohmann

# Link eEVM statically
ldflags-y += -L$(EEVM_DIR)/build
ldflags-y += -static
ldflags-y += -leevm       # eEVM library
ldflags-y += -lstdc++     # C++ runtime
ldflags-y += -lm          # Math
```

### 3. Sử dụng trong TA

```cpp
extern "C" {
#include <tee_internal_api.h>
}

// eEVM headers
#include <eEVM/processor.h>
#include <eEVM/simple/simpleglobalstate.h>

TEE_Result run_evm_contract() {
    // eEVM code đã có sẵn STL bên trong!
    eevm::Processor processor;
    
    // Execute bytecode...
    
    return TEE_SUCCESS;
}
```

---

## ⚖️ So sánh approaches

| Approach | Binary Size | Complexity | STL Support | eEVM Ready |
|----------|-------------|------------|-------------|------------|
| **Minimal C++** | 50-100KB | Low | ❌ Custom | ❌ No |
| **Custom STL** | 100-200KB | Medium | ⚠️ Partial | ❌ No |
| **Newlib** | 200-400KB | High | ✅ Full | ⚠️ Maybe |
| **eEVM Static** | 500KB-1MB | High | ✅ Full | ✅ Yes |

---

## 📝 Recommended: Incremental approach

### Phase 1: Hello C++ (✅ DONE)
```makefile
# Minimal C++, no STL
cppflags-y += -std=c++17 -fno-exceptions -fno-rtti
ldflags-y += -static -lstdc++
```

### Phase 2: Custom lightweight STL
```makefile
# Add new/delete, minimal containers
cppflags-y += -std=c++17 -fno-exceptions -fno-rtti
ldflags-y += -static -lstdc++
```

### Phase 3: eEVM integration
```makefile
# Full eEVM with all dependencies
cppflags-y += -std=c++17 -fno-exceptions -fno-rtti
ldflags-y += -static -leevm -lstdc++ -lm
```

---

## 🛠️ Script: Check static linking

**File: `check_static.sh`**

```bash
#!/bin/bash

TA_FILE=$1

if [ -z "$TA_FILE" ]; then
    echo "Usage: $0 <ta_file.ta>"
    exit 1
fi

echo "=== Checking Static Linking ==="
echo ""

echo "1. Dynamic dependencies:"
aarch64-none-linux-gnu-readelf -d "$TA_FILE" 2>/dev/null | grep NEEDED || echo "  ✅ No dynamic dependencies"

echo ""
echo "2. C++ symbols:"
aarch64-none-linux-gnu-nm -C "$TA_FILE" 2>/dev/null | grep "std::" | head -5

echo ""
echo "3. File size:"
ls -lh "$TA_FILE" | awk '{print "  " $5}'

echo ""
echo "4. Sections:"
aarch64-none-linux-gnu-readelf -S "$TA_FILE" 2>/dev/null | grep -E "\.text|\.data|\.bss"
```

Usage:
```bash
chmod +x check_static.sh
./check_static.sh deploy/*.ta
```

---

## 💡 Tóm tắt

### Static Linking là gì?
- Nhúng **toàn bộ code** từ libraries vào binary
- TA file lớn hơn nhưng **standalone**
- Không cần external dependencies

### Tại sao cần trong TEE?
- TEE **không có shared libraries**
- Không có dynamic linker
- Binary phải **self-contained**

### Cách sử dụng STL?
1. **Minimal**: Viết custom containers (nhẹ nhất)
2. **Newlib**: Build newlib cho ARM64 (phức tạp)
3. **eEVM**: Dùng eEVM library có sẵn STL (recommend)

### Flags cần thiết:
```makefile
ldflags-y += -static       # Force static linking
ldflags-y += -lstdc++      # C++ standard library
ldflags-y += -lm           # Math library
```

---

**Next step**: Bạn muốn thử approach nào?
1. Custom lightweight STL
2. Full STL với newlib
3. Tích hợp eEVM ngay (recommended)
