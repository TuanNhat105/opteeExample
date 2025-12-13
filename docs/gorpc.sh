#!/bin/bash
# filepath: /home/abc/nhat/optee_examples/docs/integrate_go_rpc_backend.sh

PROJECT_ROOT=$(pwd)
TA_SRC_DIR="$PROJECT_ROOT/optee_examples/evm_ta/ta"
SUPPLICANT_C_FILE="$PROJECT_ROOT/optee_client/host/libteec/src/supplicant_impl.c"
BUILD_DIR="$PROJECT_ROOT/build"

echo "=== Tích hợp Go RPC Backend với OP-TEE OCALL ==="

# --- BƯỚC 1: Tạo Go RPC Client Wrapper (trong Normal World) ---
echo "[1/4] Tạo Go RPC client wrapper..."

cat << 'EOF' > "$PROJECT_ROOT/optee_client/host/go_rpc_bridge/rpc_client.go"
package main

/*
#include <stdint.h>
#include <stdlib.h>
*/
import "C"
import (
    "context"
    "encoding/json"
    "fmt"
    "unsafe"
    
    // Import Go RPC client từ metaCoSign
    "github.com/meta-node-blockchain/meta-node/pkg/client"
)

var rpcClient *client.ClientRpc

// InitRPCClient khởi tạo kết nối đến Go RPC server
//export InitRPCClient
func InitRPCClient(serverURL *C.char) C.int {
    url := C.GoString(serverURL)
    
    // Khởi tạo ClientRpc (sử dụng config từ metaCoSign)
    cfg := &client.Config{
        RpcUrl: url,
    }
    
    var err error
    rpcClient, err = client.NewClientRpc(cfg)
    if err != nil {
        fmt.Printf("Failed to init RPC client: %v\n", err)
        return -1
    }
    
    fmt.Printf("Go RPC Client initialized: %s\n", url)
    return 0
}

// CallGetAccount gọi RPC để lấy thông tin account
//export CallGetAccount
func CallGetAccount(address *C.char, outBuf *C.char, maxSize C.size_t) C.size_t {
    if rpcClient == nil {
        fmt.Println("RPC client not initialized")
        return 0
    }
    
    addr := C.GoString(address)
    ctx := context.Background()
    
    // Gọi eth_getBalance hoặc custom method
    balance, err := rpcClient.EthGetBalance(ctx, addr, "latest")
    if err != nil {
        fmt.Printf("RPC call failed: %v\n", err)
        return 0
    }
    
    // Serialize kết quả thành JSON
    result := map[string]interface{}{
        "address": addr,
        "balance": balance.String(),
    }
    
    jsonData, _ := json.Marshal(result)
    jsonStr := string(jsonData)
    
    // Copy vào output buffer
    if len(jsonStr) > int(maxSize) {
        fmt.Println("Buffer too small")
        return 0
    }
    
    // Copy string vào C buffer
    for i := 0; i < len(jsonStr); i++ {
        *(*C.char)(unsafe.Pointer(uintptr(unsafe.Pointer(outBuf)) + uintptr(i))) = C.char(jsonStr[i])
    }
    
    return C.size_t(len(jsonStr))
}

func main() {} // Required for cgo
EOF

# --- BƯỚC 2: Build Go shared library ---
echo "[2/4] Building Go RPC bridge shared library..."

cd "$PROJECT_ROOT/optee_client/host/go_rpc_bridge" || exit 1

# Build Go code thành shared library (.so)
CGO_ENABLED=1 GOOS=linux GOARCH=arm64 \
go build -buildmode=c-shared -o librpc_bridge.so rpc_client.go

if [ $? -ne 0 ]; then
    echo "Failed to build Go RPC bridge"
    exit 1
fi

# --- BƯỚC 3: Cập nhật Supplicant để gọi Go library ---
echo "[3/4] Patching supplicant to call Go RPC library..."

cat << 'EOF' > "$PROJECT_ROOT/optee_client/host/libteec/src/supplicant_go_bridge.c"
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

// Function pointers cho Go RPC functions
typedef int (*InitRPCClientFunc)(const char *serverURL);
typedef size_t (*CallGetAccountFunc)(const char *address, char *outBuf, size_t maxSize);

static void *go_rpc_handle = NULL;
static InitRPCClientFunc go_init_rpc = NULL;
static CallGetAccountFunc go_call_get_account = NULL;

// Khởi tạo Go RPC bridge
int init_go_rpc_bridge(const char *server_url)
{
    // Load Go shared library
    go_rpc_handle = dlopen("/usr/lib/librpc_bridge.so", RTLD_LAZY);
    if (!go_rpc_handle) {
        fprintf(stderr, "Failed to load Go RPC library: %s\n", dlerror());
        return -1;
    }
    
    // Load function pointers
    go_init_rpc = (InitRPCClientFunc)dlsym(go_rpc_handle, "InitRPCClient");
    go_call_get_account = (CallGetAccountFunc)dlsym(go_rpc_handle, "CallGetAccount");
    
    if (!go_init_rpc || !go_call_get_account) {
        fprintf(stderr, "Failed to load Go RPC functions\n");
        return -1;
    }
    
    // Khởi tạo RPC client
    return go_init_rpc(server_url);
}

// Wrapper function cho OCALL
size_t handle_evm_ocall_with_go(uint32_t op_id, const char *address, char *out_buf, size_t max_size)
{
    if (op_id == 0x01) { // DB_OP_GET_ACCOUNT
        if (!go_call_get_account) {
            fprintf(stderr, "Go RPC not initialized\n");
            return 0;
        }
        
        return go_call_get_account(address, out_buf, max_size);
    }
    
    return 0;
}
EOF

# Chèn vào supplicant_impl.c
if ! grep -q "init_go_rpc_bridge" "$SUPPLICANT_C_FILE"; then
    echo "Patching supplicant_impl.c..."
    
    # Include header
    sed -i '/#include <string.h>/a \
#include "supplicant_go_bridge.c"' "$SUPPLICANT_C_FILE"
    
    # Thêm khởi tạo vào hàm main (hoặc init)
    sed -i '/int main(/a \
    init_go_rpc_bridge("http://localhost:8545"); // Go RPC server URL' "$SUPPLICANT_C_FILE"
    
    # Sửa handle_evm_ocall để gọi Go bridge
    sed -i 's/memcpy(buf, test_data, data_len);/data_len = handle_evm_ocall_with_go(op_id, "0x123...", buf, max_size);/' "$SUPPLICANT_C_FILE"
fi

# --- BƯỚC 4: Rebuild với Go library linked ---
echo "[4/4] Rebuilding OP-TEE with Go RPC integration..."

cd "$BUILD_DIR" || exit 1

# Thêm linker flag cho Go library
export LDFLAGS="-L/usr/lib -lrpc_bridge -ldl"

make clean-all
make -j$(nproc)

echo "=== Go RPC Backend integration complete! ==="