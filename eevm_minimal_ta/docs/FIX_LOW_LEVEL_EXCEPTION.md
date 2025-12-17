# Fix Cấp Thấp cho Exception Handling trong OP-TEE

## Vấn Đề Root Cause

Theo [OpenEnclave Issue #2274](https://github.com/openenclave/openenclave/issues/2274), vấn đề không phải ở override `std::terminate()`, mà ở **các hàm cấp thấp** trong libunwind:

### 1. `uc_addr()` không map đúng SP và PC

**File:** `libunwind/src/aarch64/Ginit.c`

**Vấn đề:**
```c
static inline void *
uc_addr (ucontext_t *uc, int reg)
{
  if (reg >= UNW_AARCH64_X0 && reg < UNW_AARCH64_V0)
    return &uc->uc_mcontext.regs[reg];  // ❌ Chỉ map X0-X30
  // SP và PC không được handle!
}
```

**Nguyên nhân:**
- `UNW_AARCH64_SP = 31` và `UNW_AARCH64_PC = 32`
- SP và PC nằm ở `uc_mcontext.sp` và `uc_mcontext.pc`, **KHÔNG** trong `regs[]` array
- `uc_addr()` chỉ map `regs[reg]` → SP và PC trả về `NULL`
- Unwinder không tìm thấy SP/PC → không thể tính toán stack frame

### 2. `getcontext` macro không lưu SP và PC đúng

**File:** `libunwind/include/libunwind-aarch64.h`

**Vấn đề:**
- Macro chỉ lưu vào `regs[]` array
- Không set `uc_mcontext.sp` và `uc_mcontext.pc`
- Có bug: `stp x14, x13` thay vì `stp x14, x15`

## Giải Pháp

### Fix 1: Override `tdep_uc_addr` để map SP/PC đúng

**File:** `build_libunwind/fix_aarch64_uc_addr.c`

```c
HIDDEN void *
tdep_uc_addr (ucontext_t *uc, int reg)
{
  /* Handle SP and PC specially - they're not in regs[] array */
  if (reg == UNW_AARCH64_SP)
    return (void*)&uc->uc_mcontext.sp;
  else if (reg == UNW_AARCH64_PC)
    return (void*)&uc->uc_mcontext.pc;
  else if (reg >= UNW_AARCH64_X0 && reg < UNW_AARCH64_V0)
    return &uc->uc_mcontext.regs[reg];
  // ... rest
}
```

### Fix 2: Cải thiện `getcontext` macro

**File:** `build_libunwind/libunwind-optee-fix.h`

```c
#define unw_tdep_getcontext(uc) (({					\
  // ... save all registers ...
  unw_ctx->uc_mcontext.sp = unw_sp;  // ✅ Set SP
  unw_ctx->uc_mcontext.pc = unw_pc;  // ✅ Set PC
  }), 0)
```

## Cách Build

### 1. Rebuild libunwind với fixes

```bash
cd /home/abc/nhat/optee_examples/eevm_minimal_ta
./build_libunwind_openenclave.sh
```

Script sẽ:
- Fix register saving bug (x14, x13 → x14, x15)
- Compile `fix_aarch64_uc_addr.c` để override `tdep_uc_addr`
- Include `libunwind-optee-fix.h` để override `getcontext`

### 2. Rebuild TA

```bash
cd ta
make clean
make
```

### 3. Test

```bash
cd host
./minimal_evm_host
```

## Kiểm Tra Fix

### 1. Verify `tdep_uc_addr` được override

```bash
nm build_libunwind/libunwind.a | grep tdep_uc_addr
# Phải thấy: T tdep_uc_addr (từ fix_aarch64_uc_addr.o)
```

### 2. Verify `getcontext` fix

```bash
grep "uc_mcontext.sp = unw_sp" build_libunwind/libunwind-optee-fix.h
# Phải thấy fix
```

### 3. Test exception

```bash
cd host
./minimal_evm_host
# Exception test phải PASS
```

## So Sánh: Override vs Fix Cấp Thấp

### Override `std::terminate()` (Không đủ)

**Vấn đề:**
- Chỉ workaround, không fix root cause
- Unwinder vẫn fail → exception không được catch
- Chỉ prevent crash, không enable exception handling

### Fix Cấp Thấp (Đúng cách)

**Ưu điểm:**
- Fix root cause: SP/PC được map đúng
- Unwinder hoạt động đúng → exception được catch
- Exception handling hoạt động đầy đủ

## Technical Details

### Cấu Trúc mcontext_t

```c
typedef struct {
    unsigned long regs[31];  // X0-X30
    unsigned long sp;         // Stack pointer (KHÔNG trong regs[])
    unsigned long pc;         // Program counter (KHÔNG trong regs[])
    unsigned long pstate;
    // ...
} mcontext_t;
```

### Register Mapping

- `UNW_AARCH64_X0` = 0 → `regs[0]`
- `UNW_AARCH64_X30` = 30 → `regs[30]`
- `UNW_AARCH64_SP` = 31 → `sp` (KHÔNG phải `regs[31]`)
- `UNW_AARCH64_PC` = 32 → `pc` (KHÔNG phải `regs[32]`)

### Flow Exception Handling

```
throw 42
  ↓
__cxa_throw()
  ↓
_Unwind_RaiseException()
  ↓
unw_getcontext() → unw_tdep_getcontext()  ← Fix 1: Save SP/PC đúng
  ↓
unw_init_local() → common_init()
  ↓
DWARF_REG_LOC(SP) → tdep_uc_addr(SP)  ← Fix 2: Map SP đúng
  ↓
dwarf_get(SP) → *uc_mcontext.sp  ← ✅ Tìm thấy SP
  ↓
unw_step() → traverse stack frames
  ↓
Find exception handler
  ↓
catch (int e)  ← ✅ Exception được catch!
```

## Files Đã Tạo/Sửa

1. **`build_libunwind/fix_aarch64_uc_addr.c`**
   - Override `tdep_uc_addr` để map SP/PC đúng

2. **`build_libunwind/libunwind-optee-fix.h`**
   - Override `unw_tdep_getcontext` để save SP/PC đúng

3. **`build_libunwind_openenclave.sh`**
   - Compile `fix_aarch64_uc_addr.c`
   - Include `libunwind-optee-fix.h`

## Kết Quả Mong Đợi

- ✅ Exception được throw và catch thành công
- ✅ Unwinder tìm thấy handler
- ✅ Stack frame được tính toán đúng
- ✅ Không cần override `std::terminate()`

## References

- [OpenEnclave Issue #2274](https://github.com/openenclave/openenclave/issues/2274)
- [libunwind aarch64 Ginit.c](https://github.com/libunwind/libunwind/blob/master/src/aarch64/Ginit.c)
- [ARM64 Calling Convention](https://developer.arm.com/documentation/102374/0101/Procedure-Call-Standard)

