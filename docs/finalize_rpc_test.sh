
Folder highlights
eEVM integration into OP-TEE requires disabling C++ exceptions/RTTI and statically linking all dependencies, followed by building a 4-step OCALL RPC bridge.

#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
TA_SRC_DIR="$PROJECT_ROOT/optee_examples/evm_ta/ta"
EVM_TA_CPP_FILE="$TA_SRC_DIR/evm_ta.cpp"
OCALL_CPP_FILE="$TA_SRC_DIR/GlobalStateOCall.cpp"
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Bắt đầu hoàn thiện TA C++ OCALL Logic và kiểm tra RPC End-to-End ---"

# --- BƯỚC 1: Hoàn thiện GlobalStateOCall.cpp (RPC Bridge) ---
echo "[1/3] Hoàn thiện GlobalStateOCall.cpp để gọi RPC Wrapper..."

cat << 'EOF' > "$OCALL_CPP_FILE"
#include "GlobalStateOCall.h"
#include <tee_internal_api.h>
#include <stdio.h>
#include <string.h>

// Định nghĩa hàm C wrapper trong OP-TEE OS (đã được vá trong rpc.c)
extern "C" TEE_Result tee_rpc_get_file(uint32_t type, TEE_Param params[4]); 

// Hàm chính gọi RPC ra Host (Normal World)
TEE_Result rpc_call_db(enum DbOperation op, TEE_Param params[4])
{
    // Cấu hình tham số cho RPC Wrapper (TEEC_VALUE_INPUT, TEEC_MEMREF_TEMP_OUTPUT)
    uint32_t ptypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_MEMREF_TEMP_OUTPUT, 
                                       TEEC_NONE, TEEC_NONE);
    
    // Tham số 1 (Value Input): Lệnh DB_OP (ví dụ: DB_OP_GET_ACCOUNT)
    TEE_Param rpc_params[4];
    rpc_params[0].value.a = (uint32_t)op; 
    
    // Tham số 2 (MemRef Output): Buffer (chứa dữ liệu file)
    rpc_params[1].memref.buffer = params[1].memref.buffer;
    rpc_params[1].memref.size = params[1].memref.size;

    IMSG("RPC Bridge: Yêu cầu Host tải file/DB OCALL (Op: 0x%x).", (uint32_t)op);

    // Gọi hàm RPC wrapper C trong OP-TEE OS
    TEE_Result res = tee_rpc_get_file(ptypes, rpc_params); 

    // Cập nhật kích thước dữ liệu thực tế được trả về từ Host
    if (res == TEE_SUCCESS) {
        // Cần cập nhật kích thước trả về từ Host (được lưu trong rpc_params[1].memref.size)
        params[1].memref.size = rpc_params[1].memref.size;
    }
    
    return res;
}
EOF

# --- BƯỚC 2: Sửa evm_ta.cpp (Logic RPC Test End-to-End) ---
echo "[2/3] Cập nhật evm_ta.cpp với logic RPC File Transfer Test..."

cat << 'EOF' > "$EVM_TA_CPP_FILE"
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "GlobalStateOCall.h" // Thư viện OCALL Bridge

// Định nghĩa Command ID cho phép gọi test RPC từ Host
#define TA_EVM_CMD_TEST_RPC 0x02 

// Dữ liệu Host Supplicant đã được cấu hình để gửi (trong supplicant_impl.c)
#define EXPECTED_HOST_DATA "EVM_CONFIG_DATA_FROM_HOST_OK"

TEE_Result run_file_transfer_test()
{
    TEE_Result res;
    // Tên file (sử dụng DB_OP_GET_ACCOUNT (0x01) cho mục đích thử nghiệm)
    enum DbOperation file_op = DB_OP_GET_ACCOUNT; 
    
    // Buffer để chứa dữ liệu tải về (1KB)
    size_t buffer_size = 1024; 
    char *file_buffer = (char *)TEE_Malloc(buffer_size, 0);

    if (!file_buffer) {
        EMSG("Không thể cấp phát bộ nhớ cho file buffer.");
        return TEE_ERROR_OUT_OF_MEMORY;
    }

    // Đóng gói tham số cho rpc_call_db:
    TEE_Param params[4];
    // Tham số 1 (Value): Lệnh OCALL
    params[0].value.a = (uint32_t)file_op; 
    // Tham số 2 (MemRef): Buffer và size
    params[1].memref.buffer = file_buffer;
    params[1].memref.size = buffer_size;
    
    // Gọi RPC (sẽ gọi Host Supplicant để tải file)
    res = rpc_call_db(file_op, params); 

    if (res == TEE_SUCCESS) {
        // --- KIỂM TRA DỮ LIỆU ĐÃ NHẬN ---
        size_t received_size = params[1].memref.size;
        size_t expected_size = strlen(EXPECTED_HOST_DATA) + 1;
        
        IMSG("OCALL RPC SUCCESS! Đã nhận %zu bytes từ Host.", received_size);

        if (received_size == expected_size && 
            memcmp(file_buffer, EXPECTED_HOST_DATA, expected_size) == 0) 
        {
            IMSG("Dữ liệu OCALL RPC khớp: %s", file_buffer);
            res = TEE_SUCCESS;
        } else {
            EMSG("Dữ liệu OCALL RPC KHÔNG KHỚP. Nhận %zu bytes, Dữ liệu: %s", 
                 received_size, file_buffer);
            res = TEE_ERROR_GENERIC;
        }

    } else {
        EMSG("OCALL RPC FAILED (Code 0x%x).", res);
    }
    
    TEE_Free(file_buffer);
    return res;
}

// CÁC HÀM ENTRY POINT CỦA TA (Đã được định nghĩa ở bước trước)
extern "C" TEE_Result TA_CreateEntryPoint(void) {
    IMSG("--- EVM C++ TA Initialized, sẵn sàng cho RPC Test. ---");
    return TEE_SUCCESS;
}
extern "C" void TA_DestroyEntryPoint(void) { IMSG("--- EVM C++ TA Destroyed ---"); }
extern "C" TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types, TEE_Param params[4], void **sess_ctx) { 
    IMSG("--- EVM C++ TA Session Opened ---");
    (void)param_types; (void)params; (void)sess_ctx;
    return TEE_SUCCESS;
}
extern "C" void TA_CloseSessionEntryPoint(void *sess_ctx) { 
    IMSG("--- EVM C++ TA Session Closed ---");
    (void)sess_ctx; 
}

// Hàm chính xử lý lệnh (Command) từ Host
extern "C" TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx, uint32_t cmd_id, uint32_t param_types, TEE_Param params[4])
{
    (void)sess_ctx;
    (void)param_types;
    (void)params;
    
    // TA_EVM_CMD_TEST_RPC = 0x02
    if (cmd_id == TA_EVM_CMD_TEST_RPC) {
        return run_file_transfer_test();
    }
    
    return TEE_ERROR_BAD_PARAMETERS;
}
EOF

# --- BƯỚC 3: Biên dịch lại toàn bộ hệ thống ---
echo "[3/3] Đang chạy make để biên dịch lại TA và các file Core đã vá..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)

echo "--- Hoàn tất Finalize RPC Logic. Sẵn sàng Kiểm tra RPC End-to-End. ---"
Beta
0 / 0
used queries
1