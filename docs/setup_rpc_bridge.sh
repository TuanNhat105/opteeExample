
Folder highlights
eEVM integration into OP-TEE requires disabling C++ exceptions/RTTI and statically linking all dependencies, followed by building a 4-step OCALL RPC bridge.

#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
TA_SRC_DIR="$PROJECT_ROOT/optee_examples/evm_ta/ta"
OCALL_CPP_FILE="$TA_SRC_DIR/GlobalStateOCall.cpp"
OCALL_H_FILE="$TA_SRC_DIR/include/GlobalStateOCall.h"
TA_SUB_MK="$TA_SRC_DIR/sub.mk"
OPTEE_OS_HEADER="$PROJECT_ROOT/optee_os/core/include/optee_msg_supplicant.h"
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Bắt đầu tạo C++ OCALL Bridge cho eEVM (Kênh RPC) ---"

# --- BƯỚC 1: Định nghĩa RPC ID mới (Sửa Header Cốt lõi) ---
echo "[1/4] Đang định nghĩa RPC ID OPTEE_MSG_RPC_CMD_OCALL_EVM_DB (0x1000)..."

# Thêm RPC ID mới vào optee_msg_supplicant.h (Nếu chưa có)
if ! grep -q "OPTEE_MSG_RPC_CMD_OCALL_EVM_DB" "$OPTEE_OS_HEADER"; then
    echo "Thêm RPC ID mới vào $OPTEE_OS_HEADER"
    # Sử dụng sed để chèn dòng sau khi 'enum optee_msg_rpc_cmd {'
    sed -i '/enum optee_msg_rpc_cmd {/a \
        OPTEE_MSG_RPC_CMD_OCALL_EVM_DB = 0x1000, /* Custom command for eEVM DB Access */' "$OPTEE_OS_HEADER"
fi

# --- BƯỚC 2: Tạo Header GlobalStateOCall.h ---
echo "[2/4] Tạo Header GlobalStateOCall.h (Định nghĩa OCALL Interface)..."
mkdir -p "$TA_SRC_DIR/include"
cat << 'EOF' > "$OCALL_H_FILE"
#pragma once
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

// Định nghĩa các loại thao tác DB cho RPC
enum DbOperation {
    DB_OP_GET_ACCOUNT = 0x01,
    DB_OP_SET_ACCOUNT = 0x02,
    DB_OP_GET_STORAGE = 0x03,
    DB_OP_SET_STORAGE = 0x04
};

// Hàm chính gọi RPC ra Host (Normal World)
// Hàm này sẽ gọi C wrapper trong OP-TEE OS
TEE_Result rpc_call_db(enum DbOperation op, TEE_Param params[4]);
EOF

# --- BƯỚC 3: Tạo GlobalStateOCall.cpp (Bridge Stub) ---
echo "[3/4] Tạo GlobalStateOCall.cpp (Bridge Implementation Stub)..."
cat << 'EOF' > "$OCALL_CPP_FILE"
#include "GlobalStateOCall.h"
#include <stdio.h>

// Định nghĩa hàm C wrapper (sẽ được implement trong optee_os/core/kernel/rpc.c)
extern "C" TEE_Result tee_rpc_get_file(uint32_t type, TEE_Param params[4]); 

TEE_Result rpc_call_db(enum DbOperation op, TEE_Param params[4])
{
    // Cần 4 tham số (Argument) cho EVM OCALL: 
    // Tham số 1: Lệnh DB_OP
    // Tham số 2: Buffer MemRef (cho Key/Value)
    // Tham số 3, 4: Không dùng cho OCALL GET/SET đơn giản

    // Tạm thời chỉ thông báo:
    IMSG("RPC Bridge: Đã chặn lệnh DB 0x%x. Đang gọi ra Host...", op);

    // CHÚ THÍCH: Logic RPC thực tế sẽ được thêm vào sau. 
    // Hiện tại chỉ là stub, sẽ trả về lỗi nếu không có logic RPC.
    
    // Tạm thời trả về lỗi để xác định rằng OCALL chưa được triển khai hoàn chỉnh.
    return TEE_ERROR_NOT_IMPLEMENTED; 
}
EOF

# --- BƯỚC 4: Cập nhật TA sub.mk để bao gồm GlobalStateOCall.cpp ---
echo "[4/4] Cập nhật ta/sub.mk để bao gồm GlobalStateOCall.cpp..."

# Kiểm tra nếu GlobalStateOCall.cpp chưa được thêm vào sub.mk
if ! grep -q "GlobalStateOCall.cpp" "$TA_SUB_MK"; then
    echo "Thêm GlobalStateOCall.cpp vào srcs-y trong $TA_SUB_MK"
    sed -i '/srcs-y += evm_ta.cpp/a \
srcs-y += GlobalStateOCall.cpp' "$TA_SUB_MK"
fi
# Thêm include directory (đã làm ở bước tích hợp eevm)
if ! grep -q "include/GlobalStateOCall.h" "$TA_SUB_MK"; then
    echo "global-incdirs-y += include" >> "$TA_SUB_MK"
fi

# --- Biên dịch lại TA để áp dụng thay đổi Header OP-TEE OS ---
echo "--- Đang chạy make để kiểm tra biên dịch RPC Bridge Header/Stub ---"

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)
Beta
0 / 0
used queries
1