
Folder highlights
eEVM integration into OP-TEE requires disabling C++ exceptions/RTTI and statically linking all dependencies, followed by building a 4-step OCALL RPC bridge.

#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
RPC_C_FILE="$PROJECT_ROOT/optee_os/core/kernel/rpc.c"
SUPPLICANT_C_FILE="$PROJECT_ROOT/optee_client/host/libteec/src/supplicant_impl.c"
BUILD_DIR="$PROJECT_ROOT/build"
OPTEE_OS_HEADER="$PROJECT_ROOT/optee_os/core/include/optee_msg_supplicant.h"

echo "--- Bắt đầu vá OP-TEE OS và OP-TEE Client cho RPC Tải File ---"

# --- 1. Vá OP-TEE OS (rpc.c): Thêm hàm C wrapper tee_rpc_get_file ---
echo "[1/3] Đang vá optee_os/core/kernel/rpc.c để thêm hàm RPC wrapper..."

# Định nghĩa hàm C wrapper tee_rpc_get_file
RPC_WRAPPER_CODE='
TEE_Result tee_rpc_get_file(uint32_t type, TEE_Param params[4])
{
    // Hàm này được gọi từ C++ TA (GlobalStateOCall.cpp)
    // Nó gọi hàm optee_msg_rpc_cmd() để tạo ra yêu cầu RPC gửi đến Host
    return optee_msg_rpc_cmd(OPTEE_MSG_RPC_CMD_OCALL_EVM_DB, type, params);
}
'
# Chèn hàm này vào file rpc.c (sau các include)
if ! grep -q "tee_rpc_get_file" "$RPC_C_FILE"; then
    # Tìm vị trí để chèn (ví dụ: sau #include <util.h>)
    sed -i "/#include <util.h>/a \
    $RPC_WRAPPER_CODE" "$RPC_C_FILE"
fi


# --- 2. Vá OP-TEE Client Supplicant (supplicant_impl.c): Cập nhật Handler ---
echo "[2/3] Đang vá optee_client/host/libteec/src/supplicant_impl.c (Host Handler)..."

# Mã xử lý EVM RPC (Sử dụng dữ liệu giả EVM_CONFIG_DATA_FROM_HOST_OK)
EVM_RPC_HANDLER_CODE='
/*
 * Xử lý lệnh RPC EVM OCALL (DB Access/File Transfer)
 */
static size_t handle_evm_ocall(const struct optee_msg_arg *arg, void *buf)
{
    // Dữ liệu giả định Host sẽ gửi (phải khớp với evm_ta.cpp)
    const char *test_data = "EVM_CONFIG_DATA_FROM_HOST_OK";
    size_t data_len = strlen(test_data) + 1; // +1 cho null terminator

    // Tham số 0: Lệnh DbOperation (GET/SET)
    uint32_t op_id = arg->params[0].value.a;
    
    // Chỉ hỗ trợ DB_OP_GET_ACCOUNT (0x01) cho thử nghiệm
    if (op_id != 0x01) {
        EMSG("OCALL EVM: Lệnh RPC không hợp lệ: 0x%x", op_id);
        return -1;
    }
    
    // Kích thước tối đa TA cho phép
    size_t max_size = arg->params[1].memref.size;
    
    if (data_len > max_size) {
        EMSG("Dữ liệu (%zu bytes) quá lớn so với buffer (%zu bytes).", data_len, max_size);
        return -1;
    }
    
    // Ghi dữ liệu giả vào buffer (buf là địa chỉ SHM được ánh xạ)
    memcpy(buf, test_data, data_len);
    
    DMSG("EVM RPC: Dữ liệu giả định (%s) đã được ghi vào buffer SHM.", test_data);
    
    // Trả về kích thước dữ liệu đã gửi
    return data_len; 
}
'

# Chèn hàm xử lý EVM RPC vào supplicant_impl.c (sau các include)
if ! grep -q "handle_evm_ocall" "$SUPPLICANT_C_FILE"; then
    # Tìm vị trí để chèn hàm (ví dụ: sau #include <string.h>)
    sed -i "/#include <string.h>/a \
\
$EVM_RPC_HANDLER_CODE" "$SUPPLICANT_C_FILE"
fi

# Chèn logic switch case vào hàm supplicant_handle_rpc_cmd
RPC_CASE_LOGIC='
    case OPTEE_MSG_RPC_CMD_OCALL_EVM_DB:
        return handle_evm_ocall(arg, buf);
'
if ! grep -q "OPTEE_MSG_RPC_CMD_OCALL_EVM_DB" "$SUPPLICANT_C_FILE"; then
    # Chèn logic vào bên trong hàm supplicant_handle_rpc_cmd (sau switch cmd)
    sed -i "/switch (cmd) {/a \
    $RPC_CASE_LOGIC" "$SUPPLICANT_C_FILE"
fi

# --- 3. Biên dịch lại toàn bộ hệ thống ---
echo "[3/3] Đang biên dịch lại OP-TEE OS và Client Library..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
# Chạy clean-all và make lại để đảm bảo các file core đã vá được áp dụng
make clean-all
make -j$(nproc)

echo "--- RPC File Transfer Bridge hoàn tất. ---"
Beta
0 / 0
used queries
1