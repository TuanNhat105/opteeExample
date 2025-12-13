# EASTL Integration - Current Status

## Issue: Dependencies

EASTL có nhiều dependencies không available trong TEE:
- `math.h` - TEE không có full math library
- `cstddef`, `cstdint` - C++ standard headers
- EABase auto-detection cần nhiều platform headers

## Attempts Made

1. ✅ Cloned EASTL và EABase
2. ✅ Created custom allocator using TEE_Malloc/TEE_Free
3. ✅ Created eastl_support.cpp với memory functions
4. ❌ Build fails - EABase requires standard headers

## Alternative Approaches

### Option 1: Minimal EASTL (Recommended)
Sử dụng chỉ một số EASTL headers đơn giản nhất:
- `eastl::vector` - thay cho std::vector
- `eastl::string` - thay cho std::string  
- Bỏ qua EABase, tự implement config

### Option 2: ETL (Embedded Template Library)
ETL được thiết kế riêng cho embedded, ít dependencies hơn:
```bash
cd ta/external
git clone https://github.com/ETLCPP/etl.git
```

ETL benefits:
- Fixed-size containers (no dynamic allocation issues)
- No EABase dependency
- Simpler headers
- Designed for microcontrollers

### Option 3: Custom STL-like Containers
Implement minimal containers cho eEVM:
- CustomString - wraps char* with TEE_Malloc
- CustomVector<T> - dynamic array
- CustomMap<K,V> - simple hash map

## Recommendation for eEVM

Vì mục đích chính là integrate eEVM, best approach là:

1. **Analyze eEVM dependencies trước**
   ```bash
   cd /home/abc/nhat
   git clone https://github.com/microsoft/eEVM.git
   grep -r "std::" eEVM/include | head -20
   ```

2. **Identify actual STL usage**
   - std::vector<uint8_t> - for bytecode
   - std::map - for storage
   - std::string - for addresses
   
3. **Choose appropriate library**
   - If heavy std::map usage → ETL (fixed-size maps)
   - If mostly vector/string → Custom minimal containers
   - If need full compatibility → EASTL (but need fix dependencies)

4. **Create compatibility layer**
   ```cpp
   // evm_types.h
   #ifdef USE_ETL
       #include <etl/vector.h>
       template<typename T> using EVMVector = etl::vector<T, 1024>;
   #else
       #include "custom_vector.h"
       template<typename T> using EVMVector = CustomVector<T>;
   #endif
   ```

## Next Steps

**Immediate (Today):**
1. Clone eEVM và analyze dependencies
2. Tạo list các STL types được sử dụng
3. Quyết định: ETL vs Custom vs continue fixing EASTL

**Short-term (This week):**
4. Implement chosen approach
5. Test basic containers trong TA
6. Port một phần nhỏ của eEVM để test

**Files to Check:**
- `eEVM/include/evm/globalstate.h` - Storage interface
- `eEVM/include/evm/processor.h` - Main execution
- `eEVM/src/simple*.cpp` - Simple implementations

Bạn muốn:
- **A) Try ETL instead?** (simpler, less dependencies)
- **B) Analyze eEVM first** to see what we actually need?
- **C) Continue fixing EASTL** dependencies?
- **D) Write custom minimal containers?**
