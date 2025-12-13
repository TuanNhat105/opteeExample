# Full STL Support in OP-TEE TA - Challenge Report

## Thử Nghiệm và Kết Quả

### Mục Tiêu
Enable full C++ STL support (std::string, std::vector, std::map, std::shared_ptr) trong OP-TEE Trusted Application để chuẩn bị cho eEVM integration.

### Phương Pháp Đã Thử

#### 1. Sử dụng Sysroot và Standard Library (THẤT BẠI)
**Approach:** Link với glibc sysroot và libstdc++ đầy đủ từ toolchain

**Configuration:**
```makefile
--sysroot=$(TOOLCHAIN)/libc
-I$(SYSROOT)/usr/include
-I$(CPP_INCLUDE)
-lstdc++ -lc -lm
```

**Vấn Đề Gặp Phải:**
1. **Threading Dependencies:**
   - libstdc++ GCC 14.3.1 requires pthread support
   - `gthr-default.h` calls pthread_create, pthread_mutex_*, pthread_cond_*
   - TA environment không có pthread (single-threaded)

2. **Libc Symbol Dependencies:**
   - Features như `__BEGIN_DECLS`, `__END_DECLS`, `__THROW` missing
   - Atomic operations `__exchange_and_add_dispatch` not available
   - `sched_yield`, `pthread_once` và các hàm khác required

3. **Header Conflicts:**
   - TA build system dùng `-nostdinc` override sysroot paths
   - `#include_next` trong C++ headers không tìm được stdlib.h
   - Order of include paths bị TA build system control

**Kết Quả:** Hàng trăm compile errors liên quan đến pthread và libc dependencies

#### 2. Freestanding Mode với Stub Headers (THẤT BẠI)  
**Approach:** Tạo minimal stub headers (features.h, locale.h, stdlib.h) và dùng `-ffreestanding`

**Vấn Đề:**
- GCC 14 C++ headers check `_GLIBCXX_HOSTED` và từ chối compile với freestanding
- Error: "This header is not available in freestanding mode"
- Requires quá nhiều stub headers (hàng chục files)

### Phân Tích Căn Nguyên

#### Tại Sao Full STL Không Hoạt Động trong TEE?

**1. Architectural Mismatch:**
```
Standard C++ Environment:
├── Full OS (Linux/Windows)
├── pthread threading
├── Complete libc (glibc/musl)
├── Dynamic linking
├── Virtual memory
└── File system

OP-TEE TA Environment:  
├── No OS (bare metal trong Secure World)
├── Single-threaded
├── Minimal libc stubs
├── Static linking only
├── Limited memory
└── No file system
```

**2. libstdc++ Dependencies Chain:**
```
<string>
  → <bits/basic_string.h>
    → <ext/string_conversions.h>
      → <cstdlib>
        → stdlib.h (needs full libc)

<memory> (smart pointers)
  → <bits/shared_ptr_base.h>
    → __atomic_add_dispatch
      → pthread threading
        → <pthread.h> (NOT AVAILABLE IN TEE!)

<vector>
  → <bits/stl_vector.h>
    → <ext/concurrence.h>
      → __gthread_mutex_t
        → pthread_mutex_* (NOT IN TEE!)
```

**3. GCC 14.3.1 Specifics:**
- Tăng cường thread-safety requirements
- Tighter integration với glibc 2.35+
- Stricter freestanding mode checks
- More atomic operations trong STL containers

### Các Giải Pháp Khả Thi

#### ✅ Option 1: Embedded STL Libraries (RECOMMENDED)
Sử dụng STL implementation được thiết kế cho embedded systems:

**A. EASTL (Electronic Arts STL)**
- Repo: https://github.com/electronicarts/EASTL
- Features: Full STL-compatible containers
- Benefits: 
  - No threading dependencies
  - Custom allocators
  - Battle-tested trong game industry
  - Header-only option available

**B. ETL (Embedded Template Library)**  
- Repo: https://github.com/ETLCPP/etl
- Features: STL-like containers for embedded
- Benefits:
  - No dynamic allocation (fixed-size containers)
  - No exceptions
  - C++03/11/14/17 support
  - Specifically designed for microcontrollers

**C. µSTL (Micro STL)**
- Lightweight STL subset
- Minimal dependencies
- Good for resource-constrained environments

**Implementation Steps:**
```bash
# 1. Clone embedded STL library
cd /home/abc/nhat/optee_examples/hello_cpp/ta
git clone https://github.com/ETLCPP/etl.git external/etl

# 2. Add include path in sub.mk
cppflags-y += -I./external/etl/include

# 3. Use in TA code
#include <etl/string.h>
#include <etl/vector.h>
#include <etl/map.h>

etl::string<50> msg = "Hello from ETL!";
etl::vector<uint32_t, 10> numbers;
```

**Ưu Điểm:**
- ✅ Hoạt động ngay trong TEE environment
- ✅ API gần giống standard STL
- ✅ Không cần libc/pthread
- ✅ Optimized cho embedded systems

**Nhược Điểm:**
- ⚠️ Cần học API khác biệt một chút (ETL dùng fixed-size)
- ⚠️ EASTL có một số differences với std::

#### ✅ Option 2: Custom Minimal Containers (CURRENT)
Tiếp tục với Level 1 approach - viết custom containers:

```cpp
// In TA code - đã có sẵn trong hello_cpp_ta_simple.cpp
namespace HelloCpp {
    class String {
        char* data;
        size_t len;
    public:
        String(const char* s) {
            len = strlen(s);
            data = (char*)TEE_Malloc(len + 1, 0);
            strcpy(data, s);
        }
        ~String() { TEE_Free(data); }
        const char* c_str() const { return data; }
    };
    
    template<typename T>
    class Vector {
        T* data;
        size_t size_;
        size_t capacity_;
    public:
        Vector() : data(nullptr), size_(0), capacity_(0) {}
        void push_back(const T& val) {
            if (size_ >= capacity_) {
                capacity_ = (capacity_ == 0) ? 1 : capacity_ * 2;
                T* new_data = (T*)TEE_Malloc(capacity_ * sizeof(T), 0);
                for (size_t i = 0; i < size_; i++) {
                    new_data[i] = data[i];
                }
                TEE_Free(data);
                data = new_data;
            }
            data[size_++] = val;
        }
        size_t size() const { return size_; }
        T& operator[](size_t i) { return data[i]; }
    };
}
```

**Ưu Điểm:**
- ✅ Full control
- ✅ Optimized cho TEE
- ✅ Minimal overhead

**Nhược Điểm:**
- ⚠️ Phải implement nhiều code
- ⚠️ Maintenance overhead
- ⚠️ Testing required

#### ⚠️ Option 3: Port eEVM to Minimal C++ (FOR EEVM INTEGRATION)
Khi integrate eEVM, sẽ cần modify eEVM source code:

**eEVM Dependencies Analysis:**
```cpp
// eEVM typically uses:
std::vector<uint8_t>  → etl::vector<uint8_t, MAX_SIZE>
std::string           → etl::string<MAX_LEN>
std::map              → etl::map<K, V, MAX_ITEMS>
std::shared_ptr       → Custom RefCounted<T> class
```

**Porting Strategy:**
1. Identify all STL usage in eEVM
2. Replace with ETL/EASTL equivalents  
3. Wrap in compatibility layer if needed
4. Test with simple EVM bytecode first

#### ❌ Option 4: Use Older GCC Version (NOT RECOMMENDED)
- GCC 9 hoặc GCC 10 có ít pthread dependencies
- Nhưng toolchain compatibility issues
- Loses modern C++17/20 features

### Khuyến Nghị

#### Cho eEVM Integration:

**Phase 1: Immediate (Using ETL) - RECOMMENDED**
```bash
# Week 1-2: Setup embedded STL
1. Integrate ETL library vào project
2. Test basic ETL containers trong TA (string, vector, map)
3. Verify memory usage và performance
4. Update documentation

# Week 3-4: eEVM preparation  
5. Analyze eEVM STL dependencies
6. Create compatibility layer (std:: → etl::)
7. Port critical eEVM components
8. Test simple EVM execution
```

**Phase 2: Full Integration**
```bash
# Week 5-6: eEVM in TEE
9. Integrate ported eEVM code
10. Implement SimpleGlobalState with etl::map
11. Test EVM bytecode execution
12. Add OCALL stubs (without Go RPC first)

# Week 7-8: OCALL + Go RPC
13. Implement OCALL mechanism
14. Connect to Go RPC backend (metaCoSign)
15. Test end-to-end: eEVM in TEE ↔ Go backend
16. Performance optimization
```

### Kết Luận

**Tình Hình Hiện Tại:**
- ✅ Minimal C++ (Level 1) đã working: `hello_cpp_ta_simple.cpp` (84KB)
- ❌ Full STL with GCC 14 libstdc++: Not feasible trong TEE environment
- ✅ Alternative solutions available: ETL/EASTL

**Next Steps:**
1. **Chọn embedded STL library** (khuyến nghị ETL vì fixed-size rất phù hợp với TEE)
2. **Test ETL trong TA** với các containers cơ bản
3. **Analyze eEVM code** để hiểu STL usage patterns
4. **Plan porting strategy** cho eEVM → ETL migration

**Timeline Estimate:**
- ETL integration: 1-2 ngày
- eEVM analysis: 2-3 ngày  
- eEVM porting: 1-2 tuần
- OCALL + RPC: 1 tuần
- **Total: ~4 tuần** cho full eEVM in TEE với Go RPC backend

## Files Created During Attempt

### Working Files (Keep):
- ✅ `ta/hello_cpp_ta_simple.cpp` - Working minimal C++ TA (84KB)
- ✅ `ta/hello_cpp_ta_simple.cpp.backup` - Backup
- ✅ `ta/sub.mk` - Build configuration (current state preserves settings)
- ✅ `ta/Makefile` - TA build file
- ✅ `host/main.c` - Test host application
- ✅ `build.sh` - Build script
- ✅ `check_static.sh` - Static linking verification script

### Experimental Files (For Reference):
- ⚠️ `ta/hello_cpp_ta_stl.cpp` - Full STL attempt (doesn't compile)
- ⚠️ `ta/stl_support.cpp` - C library stubs (incomplete)

### Documentation:
- ✅ `docs/cpp_integration_ocall_guide.md` - C++ and OCALL integration guide
- ✅ `docs/static_linking_guide.md` - Static linking explanation
- ✅ `docs/static_linking_summary.md` - 3 levels of C++ support
- ✅ `docs/stl_challenge_report.md` - This file

## Recommended Action

**Để continue với eEVM integration:**

```bash
# 1. Restore working minimal C++ version
cd /home/abc/nhat/optee_examples/hello_cpp/ta
cp hello_cpp_ta_simple.cpp.backup hello_cpp_ta_simple.cpp

# 2. Update sub.mk to build simple version
# Edit ta/sub.mk:
srcs-y = hello_cpp_ta_simple.cpp  # Remove stl_support.cpp and hello_cpp_ta_stl.cpp

# 3. Clean and rebuild
cd ..
./build.sh

# 4. Then integrate ETL for eEVM preparation
# (See Phase 1 recommendations above)
```

Bạn muốn:
- **A) Tiếp tục với ETL integration** (recommended cho eEVM)?
- **B) Thử EASTL** (nếu muốn API gần std:: hơn)?
- **C) Stick với minimal C++** và port eEVM manually?

