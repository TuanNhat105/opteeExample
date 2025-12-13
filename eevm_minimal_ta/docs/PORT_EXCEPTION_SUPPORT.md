# How to Port libunwind + libcxxrt for std::map Support

**Goal:** Enable real std::map and try-catch in OP-TEE TA

**Estimated Effort:** 3-4 weeks full-time work

---

## 📦 Required Libraries

### 1. libunwind (Stack Unwinding)
**Source:** `external/openenclave/3rdparty/libunwind/`

**What it does:**
- Unwind call stack when exception is thrown
- Save/restore ARM64 registers
- Parse DWARF/EHABI debug info
- Call destructors during unwinding

**Key Files to Port:**
```
libunwind/
├── include/
│   ├── libunwind.h           # Public API
│   └── unwind.h              # C++ ABI interface (_Unwind_*)
├── src/
│   ├── UnwindLevel1.c        # _Unwind_RaiseException (CRITICAL!)
│   ├── UnwindLevel1-gcc-ext.c # _Unwind_Backtrace
│   ├── UnwindRegistersRestore.S # ARM64 register restore (ASM!)
│   ├── UnwindRegistersSave.S    # ARM64 register save (ASM!)
│   ├── Unwind-EHABI.c        # ARM Exception ABI
│   ├── libunwind.cpp         # Unwind context
│   └── Registers.hpp         # ARM64 register definitions
```

**Porting Difficulty:** ⚠️⚠️⚠️ HIGH
- Requires ARM64 assembly knowledge
- Platform-specific (different for ARM vs x86)
- Needs DWARF/EHABI parsing
- ~2500 lines of code

---

### 2. libcxxrt (C++ Runtime ABI)
**Source:** `external/openenclave/3rdparty/libcxxrt/`

**What it does:**
- Implement C++ language features
- Exception throwing/catching (`__cxa_throw`, `__cxa_catch`)
- RTTI (typeid, dynamic_cast)
- Static initialization guards

**Key Files to Port:**
```
libcxxrt/
├── exception.cc       # __cxa_throw, __cxa_catch (CRITICAL!)
├── stdexcept.cc      # std::runtime_error, std::logic_error
├── typeinfo.cc       # std::type_info, RTTI
├── cxa_handlers.cc   # std::terminate, std::unexpected
├── memory.cc         # std::bad_alloc
├── cxa_guard.cc      # Thread-safe static init
└── dynamic_cast.cc   # dynamic_cast<> implementation
```

**Porting Difficulty:** ⚠️⚠️ MEDIUM
- Well-defined C++ ABI (Itanium ABI)
- ~2000 lines of C++ code
- Depends on libunwind

---

### 3. libcxx Exception Sources
**Source:** `external/openenclave/3rdparty/libcxx/libcxx/src/`

**What it does:**
- Implement STL exception classes
- Must be compiled (not header-only)

**Key Files to Compile:**
```
libcxx/src/
├── exception.cpp     # std::exception base class
├── stdexcept.cpp    # std::runtime_error, std::logic_error
├── new.cpp          # std::bad_alloc
├── typeinfo.cpp     # std::type_info
└── iostream.cpp     # std::ios_base::failure (optional)
```

**Porting Difficulty:** ⚠️ LOW
- ~500 lines of code
- Just compile and link
- Depends on libcxxrt

---

## 🛠️ Step-by-Step Porting Guide

### Phase 1: Port libunwind (Week 1-2)

#### Step 1.1: Create build script
```bash
#!/bin/bash
# build_libunwind.sh

PROJECT_ROOT=$(pwd)
LIBUNWIND_SRC="$PROJECT_ROOT/external/openenclave/3rdparty/libunwind"
BUILD_DIR="$PROJECT_ROOT/build_full_musl/libunwind"
CROSS_COMPILE="aarch64-none-linux-gnu-"
TA_DEV_KIT_DIR="/home/abc/optee_os/out/arm-plat-rpi5/export-ta_arm64"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Copy headers
cp -r "$LIBUNWIND_SRC/include/"* ./

# Build UnwindLevel1.c
${CROSS_COMPILE}gcc \
    -I. \
    -I"$TA_DEV_KIT_DIR/include" \
    -I"$PROJECT_ROOT/build_full_musl/include" \
    -ffreestanding -fPIC -O2 \
    -D__ARM_EABI_UNWINDER__=1 \
    -D_LIBUNWIND_IS_BAREMETAL \
    -c "$LIBUNWIND_SRC/src/UnwindLevel1.c" \
    -o UnwindLevel1.o

# Build ARM64 assembly
${CROSS_COMPILE}gcc \
    -I. \
    -c "$LIBUNWIND_SRC/src/UnwindRegistersRestore.S" \
    -o UnwindRegistersRestore.o

${CROSS_COMPILE}gcc \
    -I. \
    -c "$LIBUNWIND_SRC/src/UnwindRegistersSave.S" \
    -o UnwindRegistersSave.o

# Build Unwind-EHABI.c
${CROSS_COMPILE}gcc \
    -I. \
    -I"$TA_DEV_KIT_DIR/include" \
    -ffreestanding -fPIC -O2 \
    -D__ARM_EABI_UNWINDER__=1 \
    -c "$LIBUNWIND_SRC/src/Unwind-EHABI.c" \
    -o Unwind-EHABI.o

# Build libunwind.cpp
${CROSS_COMPILE}g++ \
    -I. \
    -I"$LIBCXX_SRC/include" \
    -std=c++17 -nostdinc++ \
    -ffreestanding -fno-exceptions -fno-rtti -fPIC -O2 \
    -D_LIBUNWIND_IS_BAREMETAL \
    -c "$LIBUNWIND_SRC/src/libunwind.cpp" \
    -o libunwind.o

# Create library
${CROSS_COMPILE}ar rcs libunwind.a \
    UnwindLevel1.o \
    UnwindRegistersRestore.o \
    UnwindRegistersSave.o \
    Unwind-EHABI.o \
    libunwind.o

echo "✓ libunwind.a built!"
```

#### Step 1.2: Adapt for OP-TEE
You'll need to modify:

**`UnwindLevel1.c`** - Replace system calls:
```c
// BEFORE (OpenEnclave)
#include <stdlib.h>
#include <stdio.h>

// AFTER (OP-TEE)
#include <tee_internal_api.h>
#define abort() TEE_Panic(0xDEAD)
#define printf(...) DMSG(__VA_ARGS__)
```

**ARM64 Assembly** - May need adjustments for OP-TEE calling convention

---

### Phase 2: Port libcxxrt (Week 3)

#### Step 2.1: Create build script
```bash
#!/bin/bash
# build_libcxxrt.sh

PROJECT_ROOT=$(pwd)
LIBCXXRT_SRC="$PROJECT_ROOT/external/openenclave/3rdparty/libcxxrt"
BUILD_DIR="$PROJECT_ROOT/build_full_musl/libcxxrt"
CROSS_COMPILE="aarch64-none-linux-gnu-"
LIBUNWIND_DIR="$PROJECT_ROOT/build_full_musl/libunwind"
LIBCXX_INC="$PROJECT_ROOT/external/openenclave/3rdparty/libcxx/libcxx/include"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

CXX_FLAGS=(
    -std=c++17
    -nostdinc++
    -ffreestanding
    -fPIC
    -O2
    -I"$LIBCXXRT_SRC/src"
    -I"$LIBUNWIND_DIR"
    -I"$LIBCXX_INC"
    -I"$PROJECT_ROOT/build_full_musl/include"
    -DLIBCXXRT_DISABLE_THREADS
)

# Build exception.cc (CRITICAL!)
${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c "$LIBCXXRT_SRC/src/exception.cc" \
    -o exception.o

# Build stdexcept.cc
${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c "$LIBCXXRT_SRC/src/stdexcept.cc" \
    -o stdexcept.o

# Build typeinfo.cc
${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c "$LIBCXXRT_SRC/src/typeinfo.cc" \
    -o typeinfo.o

# Build memory.cc
${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c "$LIBCXXRT_SRC/src/memory.cc" \
    -o memory.o

# Build cxa_handlers.cc
${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c "$LIBCXXRT_SRC/src/cxa_handlers.cc" \
    -o cxa_handlers.o

# Create library
${CROSS_COMPILE}ar rcs libcxxrt.a \
    exception.o \
    stdexcept.o \
    typeinfo.o \
    memory.o \
    cxa_handlers.o

echo "✓ libcxxrt.a built!"
```

#### Step 2.2: Key functions provided
After building libcxxrt, you'll have:
```cpp
// Exception throwing
extern "C" void __cxa_throw(
    void* thrown_exception,
    std::type_info* tinfo,
    void (*dest)(void*)
);

// Exception catching
extern "C" void* __cxa_begin_catch(void* exception_obj);
extern "C" void __cxa_end_catch();

// Exception info
extern "C" std::type_info* __cxa_current_exception_type();

// Termination
extern "C" void std::terminate();
extern "C" void std::unexpected();
```

---

### Phase 3: Compile libcxx Exception Sources (Week 3)

```bash
#!/bin/bash
# build_libcxx_exceptions.sh

LIBCXX_SRC="$PROJECT_ROOT/external/openenclave/3rdparty/libcxx/libcxx"
BUILD_DIR="$PROJECT_ROOT/build_full_musl/libcxx_exceptions"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

CXX_FLAGS=(
    -std=c++17
    -nostdinc++
    -ffreestanding
    -fPIC
    -O2
    -I"$LIBCXX_SRC/include"
    -I"$PROJECT_ROOT/build_full_musl/include"
    -I"$PROJECT_ROOT/build_full_musl/libcxxrt"
    -D_LIBCPP_BUILDING_LIBRARY
    -D_LIBCPP_HAS_NO_THREADS
)

# Build exception.cpp
${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c "$LIBCXX_SRC/src/exception.cpp" \
    -o exception.o

# Build stdexcept.cpp
${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c "$LIBCXX_SRC/src/stdexcept.cpp" \
    -o stdexcept.o

# Build new.cpp
${CROSS_COMPILE}g++ "${CXX_FLAGS[@]}" \
    -c "$LIBCXX_SRC/src/new.cpp" \
    -o new.o

# Create library
${CROSS_COMPILE}ar rcs libcxx_exceptions.a \
    exception.o \
    stdexcept.o \
    new.o

echo "✓ libcxx_exceptions.a built!"
```

---

### Phase 4: Update Build System (Week 4)

#### Step 4.1: Update `ta/sub.mk`
```makefile
# Link order matters! (dependencies first)
libnames += musl cxx unwind cxxrt cxx_exceptions
libdirs += ../build_full_musl
libdirs += ../build_full_musl/libunwind
libdirs += ../build_full_musl/libcxxrt
libdirs += ../build_full_musl/libcxx_exceptions

libdeps += ../build_full_musl/libmusl.a
libdeps += ../build_full_musl/libcxx.a
libdeps += ../build_full_musl/libunwind/libunwind.a
libdeps += ../build_full_musl/libcxxrt/libcxxrt.a
libdeps += ../build_full_musl/libcxx_exceptions/libcxx_exceptions.a

# ENABLE exceptions and RTTI!
cppflags-y += -fexceptions
cppflags-y += -frtti
cppflags-y -= -fno-exceptions
cppflags-y -= -fno-rtti
cppflags-y -= -D_LIBCPP_HAS_NO_EXCEPTIONS
cppflags-y -= -D_LIBCPP_HAS_NO_RTTI
```

#### Step 4.2: Update main build script
```bash
# build_full_musl_runtime.sh

# ... existing dlmalloc + musl + math ...

# NEW: Build exception support
./build_libunwind.sh
./build_libcxxrt.sh  
./build_libcxx_exceptions.sh

echo "✓ Full C++ runtime with exception support ready!"
```

---

## 🧪 Testing

### Test 1: Simple throw/catch
```cpp
static TEE_Result test_exceptions(uint32_t param_types, TEE_Param params[4]) {
    DMSG("Testing exception handling...");
    
    try {
        DMSG("Throwing std::runtime_error");
        throw std::runtime_error("Test exception");
    } catch (const std::runtime_error& e) {
        DMSG("Caught: %s", e.what());
        params[0].value.a = 0; // Success
        return TEE_SUCCESS;
    } catch (...) {
        EMSG("Caught unknown exception");
        return TEE_ERROR_GENERIC;
    }
    
    return TEE_ERROR_GENERIC;
}
```

### Test 2: std::map with exceptions
```cpp
static TEE_Result test_real_map(uint32_t param_types, TEE_Param params[4]) {
    DMSG("Testing std::map with exception support...");
    
    try {
        std::map<int, int> mymap;
        
        // These will throw if allocation fails
        mymap[1] = 100;
        mymap[2] = 200;
        mymap[3] = 300;
        
        DMSG("Map size: %zu", mymap.size());
        
        for (const auto& pair : mymap) {
            DMSG("  %d -> %d", pair.first, pair.second);
        }
        
        params[0].value.a = 0; // Success
        return TEE_SUCCESS;
        
    } catch (const std::bad_alloc& e) {
        EMSG("Out of memory: %s", e.what());
        return TEE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        EMSG("Exception: %s", e.what());
        return TEE_ERROR_GENERIC;
    }
}
```

---

## ⚠️ Challenges & Solutions

### Challenge 1: ARM64 Assembly
**Problem:** `UnwindRegistersRestore.S` uses ARM64 assembly

**Solution:** 
- Study ARM64 calling convention
- Test with simple examples first
- Use GDB to debug register states

### Challenge 2: DWARF Debug Info
**Problem:** Unwinding needs `.eh_frame` section

**Solution:**
```bash
# Add to LDFLAGS
LDFLAGS += -Wl,--eh-frame-hdr
LDFLAGS += -funwind-tables
```

### Challenge 3: Static vs Dynamic Linking
**Problem:** OP-TEE uses static linking only

**Solution:** All libraries must be `.a` (static archives)

### Challenge 4: Memory Overhead
**Problem:** Exception handling adds ~50KB to TA size

**Solution:** Acceptable for most use cases (TA can be 200KB+)

---

## 📊 Before & After Comparison

### BEFORE (Current)
```
✅ std::vector
✅ std::string (basic)
✅ std::algorithm
❌ std::map
❌ try-catch
❌ RTTI

TA Size: 84 KB
Libraries: libmusl.a (57KB) + libcxx.a (4KB)
```

### AFTER (With Exception Support)
```
✅ std::vector
✅ std::string (full)
✅ std::algorithm
✅ std::map
✅ std::set
✅ try-catch
✅ RTTI (typeid, dynamic_cast)

TA Size: ~150 KB
Libraries:
  - libmusl.a (57KB)
  - libcxx.a (4KB)
  - libunwind.a (~30KB)
  - libcxxrt.a (~25KB)
  - libcxx_exceptions.a (~10KB)
```

---

## 🎯 Recommendation

### Option A: Port Full Exception Support (3-4 weeks)
**Pros:**
- ✅ Real std::map, std::set
- ✅ Full C++ STL support
- ✅ Industry-standard approach

**Cons:**
- ❌ 3-4 weeks effort
- ❌ Complex ARM64 assembly
- ❌ +70KB TA size

### Option B: Use SortedVectorMap (3-5 days)
**Pros:**
- ✅ Works now
- ✅ 95% of use cases
- ✅ No assembly needed

**Cons:**
- ❌ Not real std::map
- ❌ O(n) insert/erase

### Option C: Use std::vector Only (0 days)
**Pros:**
- ✅ Works perfectly now
- ✅ No additional effort

**Cons:**
- ❌ Need workarounds for map-like data

---

## 📝 My Recommendation

**For production TA development:** Use **std::vector + SortedVectorMap**
- Solves 95% of real-world use cases
- No complex porting required
- Proven to work

**For research/academic:** Port **full exception support**
- Demonstrates full C++ in TrustZone
- Opens door for complex C++ libraries
- Publishable research contribution

Anh muốn đi theo hướng nào? Tôi có thể hỗ trợ chi tiết hơn! 🚀
