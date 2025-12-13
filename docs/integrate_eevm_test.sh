
Folder highlights
eEVM integration into OP-TEE requires disabling C++ exceptions/RTTI and statically linking all dependencies, followed by building a 4-step OCALL RPC bridge.

#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
# Giả định PROJECT_ROOT là thư mục /home/optee/qemu-optee
PROJECT_ROOT=$(pwd)
EVM_REPO_DIR="$PROJECT_ROOT/eevm_repo"
EVM_TA_DIR="$PROJECT_ROOT/optee_examples/evm_ta"
TA_SRC_DIR="$EVM_TA_DIR/ta"
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Bắt đầu Tích hợp và Kiểm tra eEVM ---"

# --- BƯỚC 1: Tải mã nguồn eEVM (Nếu chưa có) ---
if [ ! -d "$EVM_REPO_DIR" ]; then
    echo "[1/5] Đang Clone mã nguồn eEVM (Microsoft/eEVM)..."
    git clone https://github.com/microsoft/eEVM.git "$EVM_REPO_DIR"
    if [ $? -ne 0 ]; then echo "Lỗi: Clone eEVM thất bại."; exit 1; fi
fi

# --- BƯỚC 2: Sao chép Code eEVM và Header ---
echo "[2/5] Đang sao chép code eEVM và Header vào TA..."

# Tìm các file nguồn C++ cốt lõi (src/*.cpp)
EVM_CORE_SRC=$(find "$EVM_REPO_DIR/src/" -maxdepth 1 -type f -name "*.cpp" -o -name "*.c")
cp $EVM_CORE_SRC "$TA_SRC_DIR/"

# Copy các header (include/eEVM)
cp -r "$EVM_REPO_DIR/include/eEVM" "$TA_SRC_DIR/include/"

# --- BƯỚC 3: Cấu hình Makefile TA để Tích hợp eEVM ---
echo "[3/5] Đang cập nhật Makefile TA để tích hợp eEVM files..."

# Lấy danh sách các file C++ vừa copy vào TA_SRC_DIR
EVM_CPP_FILES=$(find "$TA_SRC_DIR/" -maxdepth 1 -type f -name "*.cpp" | xargs -n 1 basename | grep -v 'evm_ta.cpp' | tr '\n' ' ')

# Thêm/Ghi đè nội dung ta/sub.mk (Đảm bảo tất cả các file EVM được biên dịch)
cat << EOF > "$TA_SRC_DIR/sub.mk"
global-incdirs-y += include
global-incdirs-y += include/eEVM # Thêm thư mục header eEVM

# Tên file TA C++ chính của chúng ta
srcs-y += evm_ta.cpp

# Thêm tất cả các file nguồn C++ eEVM
srcs-y += $EVM_CPP_FILES

# C++ FLAGS: Kích hoạt C++17, tắt RTTI/Exceptions (bắt buộc trong TEE)
cppflags-y += -std=c++17 -fno-exceptions -fno-rtti

# Linker Flags: Liên kết tĩnh C++ và các libs EVM
# Bắt buộc phải có -static -lstdc++
ldflags-y += -static -lstdc++ -lgmp -lmpfr -ltbb -lleveldb -lxapian -lm
EOF

# --- BƯỚC 4: Logic Kiểm tra eEVM (Sửa evm_ta.cpp) ---
echo "[4/5] Đang thêm logic kiểm tra eEVM (SimpleGlobalState) vào evm_ta.cpp..."

# Định nghĩa Command ID cho phép gọi test eEVM từ Host
# (Đã được định nghĩa là 0x01 trong Host Client của bạn)
TA_EVM_CMD_TEST=0x01

# Nội dung file evm_ta.cpp (Logic kiểm tra eEVM)
cat << EOF > "$TA_SRC_DIR/evm_ta.cpp"
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <stdio.h>
#include <string>
#include <map>
#include <memory> 
#include <vector> // Cần cho bytecode
#include "eEVM/simple/simpleglobalstate.h" // Sử dụng SimpleGlobalState
#include "eEVM/processor.h" // Sử dụng Processor

// Định nghĩa Command ID cho phép gọi test eEVM từ Host
#define TA_EVM_CMD_TEST 0x01

// Hàm chính chạy eEVM và trả về kết quả
TEE_Result run_evm_test()
{
    // Bytecode: PUSH1 0x03 PUSH1 0x04 ADD RETURN (4+3=7)
    std::vector<uint8_t> bytecode = {
        0x60, 0x04, // PUSH1 4
        0x60, 0x03, // PUSH1 3
        0x01,       // ADD
        0xf3        // RETURN
    };
    
    // Khởi tạo SimpleGlobalState (Sử dụng std::map, không cần OCALL phức tạp)
    eevm::SimpleGlobalState gs;
    eevm::Address sender = 0x10000;
    eevm::Address contract_address = 0x20000;

    // Khởi tạo Processor
    eevm::Processor p(gs);
    
    // Chạy bytecode
    eevm::ExecResult result = p.run(
        sender,
        gs.get(contract_address),
        bytecode,
        {},            // calldata (Empty)
        0,             // gas
        0              // call value
    );
    
    // Kiểm tra kết quả
    if (result.er == eevm::ExitReason::returned && !result.output.empty()) {
        uint8_t output = result.output[0]; // Lấy byte đầu tiên (kết quả 7)
        IMSG("EVM TEST SUCCESS: Result is 0x%x (Expected 0x07)", output);
        
        if (output == 0x07) {
            return TEE_SUCCESS;
        } else {
            EMSG("EVM Test FAILED: Unexpected result 0x%x", output);
            return TEE_ERROR_GENERIC;
        }
    }
    
    EMSG("EVM Test FAILED: Execution error or empty return. ExitReason: %d", (int)result.er);
    return TEE_ERROR_GENERIC;
}

extern "C" TEE_Result TA_CreateEntryPoint(void)
{
    IMSG("--- EVM C++ TA Initialized, ready for eEVM test. ---");
    return TEE_SUCCESS;
}
// Các hàm TA entry point khác (giữ nguyên TA_DestroyEntryPoint, TA_OpenSessionEntryPoint, TA_CloseSessionEntryPoint)

extern "C" void TA_DestroyEntryPoint(void)
{
    IMSG("--- EVM C++ TA Destroyed ---");
}

extern "C" TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types, TEE_Param params[4], void **sess_ctx)
{
    IMSG("--- EVM C++ TA Session Opened ---");
    (void)param_types;
    (void)params;
    (void)sess_ctx;
    return TEE_SUCCESS;
}

extern "C" void TA_CloseSessionEntryPoint(void *sess_ctx)
{
    IMSG("--- EVM C++ TA Session Closed ---");
    (void)sess_ctx;
}

// Hàm chính xử lý lệnh (Command) từ Host
extern "C" TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx, uint32_t cmd_id, uint32_t param_types, TEE_Param params[4])
{
    (void)sess_ctx;
    (void)param_types;
    (void)params;
    
    if (cmd_id == TA_EVM_CMD_TEST) {
        return run_evm_test();
    }
    
    return TEE_ERROR_BAD_PARAMETERS;
}
EOF

# --- BƯỚC 5: Biên dịch lại toàn bộ hệ thống ---
echo "[5/5] Đang chạy make để biên dịch TA mới và các thành phần khác..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)
Beta
0 / 0
used queries
1