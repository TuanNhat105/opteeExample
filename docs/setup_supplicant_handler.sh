
Folder highlights
eEVM integration into OP-TEE requires disabling C++ exceptions/RTTI and statically linking all dependencies, followed by building a 4-step OCALL RPC bridge.

#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
SUPPLICANT_FILE="$PROJECT_ROOT/optee_client/host/libteec/src/supplicant_impl.c"
BUILD_DIR="$PROJECT_ROOT/build"
OPTEE_OS_HEADER="$PROJECT_ROOT/optee_os/core/include/optee_msg_supplicant.h"

echo "--- Bắt đầu triển khai RPC Handler (Supplicant) ---"

# --- 1. Xác nhận RPC ID mới (Đảm bảo file optee_msg_supplicant.h đã được sửa) ---
echo "[1/3] Đang xác nhận RPC ID trong optee_msg_supplicant.h..."
if ! grep -q "OPTEE_MSG_RPC_CMD_OCALL_EVM_DB" "$OPTEE_OS_HEADER"; then
    echo "RPC ID OPTEE_MSG_RPC_CMD_OCALL_EVM_DB chưa tồn tại. Đang thêm..."
    # Thêm RPC ID mới vào optee_msg_supplicant.h
    sed -i '/enum optee_msg_rpc_cmd {/a \
        OPTEE_MSG_RPC_CMD_OCALL_EVM_DB = 0x1000, /* Custom command for eEVM DB Access */' "$OPTEE_OS_HEADER"
fi

# --- 2. Triển khai RPC Handler trong supplicant_impl.c ---
echo "[2/3] Đang thêm RPC Handler cho EVM vào supplicant_impl.c..."

# Mã xử lý EVM RPC (Sử dụng dữ liệu giả để kiểm tra giao tiếp)
EVM_RPC_HANDLER_CODE='
/*
 * Xử lý lệnh RPC EVM OCALL (DB Access/File Transfer)
 * Lệnh này được gọi từ Secure World thông qua OPTEE_MSG_RPC_CMD_OCALL_EVM_DB (0x1000).
 */
static size_t handle_evm_ocall(const struct optee_msg_arg *arg, void *buf)
{
    // Tham số 0: Lệnh DbOperation (GET/SET Account/Storage)
    uint32_t op_id = arg->params[0].value.a;
    
    // Giả định tải file cấu hình EVM
    const char *test_data = "EVM_CONFIG_DATA_FROM_HOST_OK";
    size_t data_len = strlen(test_data) + 1; // +1 cho null terminator
    
    DMSG("EVM RPC OCALL được nhận. Lệnh: 0x%x", op_id);

    /* Chỉ hỗ trợ DB_OP_GET_ACCOUNT (0x01) cho thử nghiệm */
    if (op_id != 0x01) {
        EMSG("OCALL EVM: Lệnh RPC không hợp lệ: 0x%x", op_id);
        return -1;
    }
    
    // Tham số 1: Buffer đích và Kích thước tối đa
    // Kiểm tra tính hợp lệ của memref (chỉ chấp nhận SHM)
    if (arg->params[1].memref.shm_ref == 0) {
        EMSG("OCALL EVM: Không có SHM reference hợp lệ.");
        return -1;
    }
    
    // Kích thước tối đa TA cho phép
    size_t max_size = arg->params[1].memref.size;
    
    if (data_len > max_size) {
        EMSG("Dữ liệu (%zu bytes) quá lớn so với buffer (%zu bytes).", data_len, max_size);
        return -1;
    }
    
    // --- LOGIC TẢI DỮ LIỆU (Giả lập) ---
    // Ghi dữ liệu giả vào buffer (buf là địa chỉ SHM được ánh xạ)
    memcpy(buf, test_data, data_len);
    
    DMSG("EVM RPC: Dữ liệu giả định (%s) đã được ghi vào buffer SHM.", test_data);
    
    // Trả về kích thước dữ liệu đã gửi
    return data_len; 
}
'

# Chèn hàm xử lý EVM RPC vào supplicant_impl.c (sau các include)
if ! grep -q "handle_evm_ocall" "$SUPPLICANT_FILE"; then
    # Tìm vị trí để chèn hàm (ví dụ: sau #include <string.h>)
    sed -i "/#include <string.h>/a \
\
$EVM_RPC_HANDLER_CODE" "$SUPPLICANT_FILE"
fi

# Chèn logic switch case vào hàm supplicant_handle_rpc_cmd
RPC_CASE_LOGIC='
    case OPTEE_MSG_RPC_CMD_OCALL_EVM_DB:
        return handle_evm_ocall(arg, buf);
'

if ! grep -q "OPTEE_MSG_RPC_CMD_OCALL_EVM_DB" "$SUPPLICANT_FILE"; then
    # Chèn logic vào bên trong hàm supplicant_handle_rpc_cmd (sau switch cmd)
    # Tìm vị trí để chèn logic case mới
    sed -i "/switch (cmd) {/a \
    $RPC_CASE_LOGIC" "$SUPPLICANT_FILE"
fi

# --- 3. Biên dịch lại Host Client (libteec) ---
echo "[3/3] Đang biên dịch lại OP-TEE Client Library (libteec)..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)

echo "--- RPC Handler triển khai trong Supplicant hoàn tất. ---"
Beta
0 / 0
used queries


1