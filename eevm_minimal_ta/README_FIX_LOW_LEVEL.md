# Fix Cấp Thấp cho Exception Handling - Hướng Dẫn Nhanh

## Tóm Tắt

Đã fix **root cause** của exception handling trong OP-TEE bằng cách cải thiện các hàm cấp thấp trong libunwind, thay vì override `std::terminate()`.

## Vấn Đề

1. **`uc_addr()` không map SP và PC**: SP và PC nằm ở `uc_mcontext.sp/pc`, không trong `regs[]` array
2. **`getcontext` không lưu SP/PC đúng**: Chỉ lưu vào `regs[]`, không set `uc_mcontext.sp/pc`

## Giải Pháp

### Fix 1: Override `tdep_uc_addr`
**File:** `build_libunwind/fix_aarch64_uc_addr.c`
- Map `UNW_AARCH64_SP` → `&uc_mcontext.sp`
- Map `UNW_AARCH64_PC` → `&uc_mcontext.pc`

### Fix 2: Cải thiện `getcontext`
**File:** `build_libunwind/libunwind-optee-fix.h`
- Save SP và PC vào `uc_mcontext.sp/pc`
- Fix register bug (x14, x13 → x14, x15)

## Cách Sử Dụng

```bash
# 1. Rebuild libunwind với fixes
cd /home/abc/nhat/optee_examples/eevm_minimal_ta
./build_libunwind_openenclave.sh

# 2. Rebuild TA
cd ta
make clean
make

# 3. Test
cd host
./minimal_evm_host
```

## Verification

```bash
# Kiểm tra fix được compile
nm build_libunwind/libunwind.a | grep tdep_uc_addr
# Phải thấy: T tdep_uc_addr (từ fix file)

# Kiểm tra getcontext fix
grep "uc_mcontext.sp = unw_sp" build_libunwind/libunwind-optee-fix.h
```

## Kết Quả

- ✅ Exception được catch thành công
- ✅ Unwinder hoạt động đúng
- ✅ Stack frame được tính toán đúng
- ✅ Không cần override `std::terminate()`

Xem chi tiết trong `docs/FIX_LOW_LEVEL_EXCEPTION.md`.

