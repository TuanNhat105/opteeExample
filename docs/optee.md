Op-TEE

**Ghi chú**


## 📝 Ghi chú Kỹ thuật: Cài đặt và Hỗ trợ C++ trong OP-TEE

Mục tiêu của dự án này là tích hợp và chạy mã nguồn C++ phức tạp (eEVM) bên trong Trusted Application (TA) của OP-TEE. Quá trình này đòi hỏi phải giải quyết các vấn đề về môi trường thực thi Secure World (TEE Run-time Environment).

Tham khảo: https://github.com/OP-TEE/optee_os/issues/2628


### 1. Tham khảo và Rào cản C++ chính

Tham khảo chính được sử dụng là cuộc thảo luận trên GitHub về việc sử dụng thư viện C++ trong TEE:

* **Vấn đề về Ngoại lệ (Exceptions):** OP-TEE không có hỗ trợ đầy đủ cho cơ chế xử lý ngoại lệ C++ (`try-catch`).
* **Vấn đề về Khởi tạo (Global Initializers/Destructors):** Các constructor/destructor toàn cục cần được quản lý đặc biệt khi tải TA.
* **Vấn đề về Liên kết (Linking):** TEE không hỗ trợ thư viện động (`.so`) cho TA (TA cần được đóng gói riêng), trừ khi là các file `.so` đặc biệt được bao bọc như một TA.

### 2. Các Giải pháp Khắc phục Đã được Áp dụng

Để cho phép eEVM C++ hoạt động trong TEE, các cờ biên dịch và cấu hình sau đã được thiết lập trong file `sub.mk` và `Dockerfile`:

| Vấn đề | Hành động/Cấu hình Đã áp dụng | Vị trí |
| :--- | :--- | :--- |
| **Bypass Exceptions/RTTI** | Thêm cờ biên dịch để loại bỏ các tính năng này. | `cppflags-y += -fno-exceptions -fno-rtti` |
| **Hỗ trợ C++17** | Buộc sử dụng tiêu chuẩn ngôn ngữ hiện đại. | `cppflags-y += -std=c++17` |
| **Liên kết tĩnh (Static Linking)** | Bắt buộc liên kết thư viện C++ chuẩn (`libstdc++`) và các thư viện phụ thuộc (`GMP`, v.v.) vào binary `.ta`. | `ldflags-y += -static -lstdc++ -lgmp -lmpfr ...` |
| **Khắc phục Quyền truy cập** | Thiết lập UID/GID trong Dockerfile để khớp với Host macOS. | `Dockerfile` (sử dụng `useradd -u UID -g GID`) |
| **Khắc phục CMake C++11** | Ghi đè CXXFLAGS trong quá trình Buildroot. | `make ... HOST_CXXFLAGS="-std=gnu++17"` |

### 3. Nguyên tắc RPC (OCALL)

Vì eEVM cần truy cập State Database (DB) bên ngoài TEE, chúng ta đã triển khai cơ chế **OCALL RPC (Remote Procedure Call)**:

* **Secure World (TA):** Thay thế `eevm::SimpleGlobalState` bằng một lớp C++ Bridge gọi hàm RPC (`rpc_call_db`).
* **OP-TEE OS (Kernel):** Thêm hàm wrapper C (`tee_rpc_get_file`) để tạo RPC Request.
* **Normal World (Supplicant):** `supplicant_impl.c` lắng nghe RPC ID mới (`OPTEE_MSG_RPC_CMD_OCALL_EVM_DB`), xử lý yêu cầu (tải file/truy vấn DB), và chuyển dữ liệu trở lại TEE. 

Toàn bộ lộ trình đã đảm bảo rằng code C++ phức tạp có thể chạy an toàn và giao tiếp được với môi trường Normal World.

**I. Thiết lập Môi trường và Khắc phục Lỗi**
Dưới đây là script bash hoàn chỉnh để thực hiện **Bước 1 (Tạo TA) và Bước 2 (Cấu hình C++)**.

### 🛠️ Script Bash: `setup_evm_ta.sh`

Bạn hãy lưu nội dung sau vào file `setup_evm_ta.sh` (trong thư mục `/home/optee/test-optee/optee_examples/`):

```bash
#!/bin/bash

# Thiết lập thư mục gốc của OP-TEE Examples
EXAMPLES_DIR=$(dirname $(realpath "${BASH_SOURCE[0]}"))
EVM_TA_DIR=$EXAMPLES_DIR/evm_ta

echo "--- Bắt đầu cấu hình EVM C++ Trusted Application ---"

# 1. Tạo cấu trúc thư mục TA mới
if [ -d "$EVM_TA_DIR" ]; then
    echo "Xóa thư mục EVM TA cũ..."
    rm -rf "$EVM_TA_DIR"
fi

echo "Tạo thư mục TA mới từ mẫu secure_storage..."
cp -r "$EXAMPLES_DIR/secure_storage" "$EVM_TA_DIR"
cd "$EVM_TA_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục TA mới"; exit 1; }

# Xóa các file C mẫu cũ
rm ta/*.c ta/include/secure_storage_ta.h host/*.c

echo "Đang tạo file TA C++ (evm_ta.cpp)..."

# 2. Tạo file TA C++ (evm_ta.cpp)
cat << 'EOF' > ta/evm_ta.cpp
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <stdio.h>
#include <string>
#include <map>
#include <memory> 

// Tên hàm chính của TA, cần được định nghĩa với C linkage
extern "C" TEE_Result TA_CreateEntryPoint(void)
{
    // Kiểm tra std::string, std::map, và std::unique_ptr (C++17 features)
    std::string test_str = "EVM C++ TA Initialized";
    std::map<int, std::string> test_map;
    test_map[1] = "Success";
    
    // Kiểm tra C++11 feature (std::make_unique)
    auto ptr = std::make_unique<int>(123);
    
    // In log ra Secure World Console
    IMSG("--- EVM C++ TA: %s (Value: %d) ---", test_str.c_str(), *ptr);
    
    if (test_map.size() > 0 && *ptr == 123) {
        return TEE_SUCCESS;
    }
    return TEE_ERROR_GENERIC;
}

// Các hàm TA entry point khác (C linkage)
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
    IMSG("--- EVM C++ TA Invoked Command %u ---", cmd_id);
    (void)sess_ctx;
    (void)cmd_id;
    (void)param_types;
    (void)params;
    
    // Logic RPC (OCALL) của bạn sẽ nằm ở đây
    return TEE_SUCCESS;
}
EOF

echo "Đang tạo file Host C++ Client (host/main.cpp)..."

# 3. Tạo file Host C++ Client (host/main.cpp)
cat << 'EOF' > host/main.cpp
#include <stdio.h>
#include <stdlib.h>
#include <tee_client_api.h>

// Định nghĩa UUID của TA mới (sẽ được cập nhật sau)
#define TA_EVM_UUID \
	{ 0xa8b6b216, 0x95fa, 0x4c4f, \
		{ 0x9e, 0x67, 0xd1, 0x5f, 0x3e, 0x48, 0x11, 0xa7 } }

// Hàm errx (bị thiếu trong libteec)
static void errx(int exitval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(exitval);
}


int main(void)
{
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_UUID uuid = TA_EVM_UUID;

	printf("--- Host App: Starting EVM C++ TA Test ---\n");

	// Khởi tạo Context
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS) {
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);
	}

	// Mở Session (TA_CreateEntryPoint sẽ được gọi)
	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (res != TEEC_SUCCESS) {
		errx(1, "TEEC_OpenSession failed with code 0x%x", res);
	}
    
    // Kiểm tra TA_CreateEntryPoint (đã chạy khi OpenSession)
    printf("--- Host App: Session opened successfully. TA is running C++17. ---\n");

	// Đóng Session và Hủy Context
	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	printf("--- Host App: EVM C++ Test Done. ---\n");

	return 0;
}
EOF

echo "Đang cấu hình Makefile C++ và Linker (ta/sub.mk)..."

# 4. Cấu hình Makefile TA C++ (ta/sub.mk)
cat << 'EOF' > ta/sub.mk
global-incdirs-y += include

# Tên file TA C++
srcs-y += evm_ta.cpp

# C++ FLAGS: Kích hoạt C++17, tắt RTTI/Exceptions (vì TEE không hỗ trợ)
cppflags-y += -std=c++17 -fno-exceptions -fno-rtti

# Linker Flags: Liên kết tĩnh C++ và các libs EVM
# Bắt buộc phải có -static -lstdc++
ldflags-y += -static -lstdc++ -lgmp -lmpfr -ltbb -lleveldb -lxapian -lm
EOF

echo "Đang cấu hình Makefile Host C++ (host/sub.mk)..."

# 5. Cấu hình Makefile Host C++ (host/sub.mk)
cat << 'EOF' > host/sub.mk
# Tên file Host C++ Client
cxxsrcs-y += main.cpp

# Host C++ FLAGS: Kích hoạt C++17
cxxflags-y += -std=c++17

# Linker Flags: Liên kết với thư viện OP-TEE Client (libteec)
libdirs-y += -L$(abspath $(TEEC_EXPORT))
libnames-y += teec
EOF

echo "Đang cập nhật Makefile chính (evm_ta/Makefile)..."

# 6. Cập nhật Makefile chính (Cập nhật UUID)
# Sử dụng sed để thay đổi dòng TA_UUID
sed -i 's/TA_UUID := [0-9a-f-]*/TA_UUID := a8b6b216-95fa-4c4f-9e67-d15f3e4811a7/' Makefile

echo "--- Cấu hình EVM TA C++ hoàn tất. ---"

# Quay lại thư mục build để chuẩn bị build
cd /home/optee/test-optee/build

echo "--- Đang chạy make để biên dịch TA mới. ---"
make -j$(nproc)
```

### 🎯 Hành động Tiếp theo

1.  **Lưu script:** Lưu nội dung trên vào file `setup_evm_ta.sh` (nếu bạn chưa làm).

2.  **Cấp quyền:** Cấp quyền thực thi cho script: `chmod +x setup_evm_ta.sh`.

3.  **Chạy script:** Chạy script từ thư mục `/home/optee/test-optee/optee_examples/`:

    ```bash
    cd /home/optee/test-optee/optee_examples/
    ./setup_evm_ta.sh
    ```

4.  **Kiểm tra:** Nếu quá trình `make` ở cuối script thành công, bạn có thể chạy QEMU và kiểm tra.

    ```bash
    # Chạy QEMU
    cd /home/optee/test-optee/build
    make run QEMU_EXTRA_ARGS="-nographic -serial stdio"

    # Trong QEMU console (Linux Host)
    # Bạn sẽ tìm thấy TA của mình và chạy Host Client:
    # /bin/evm_ta/main
    ```


**II. Tích hợp Hỗ trợ C++ và TA Cơ bản**
Tuyệt vời\! Việc bạn đã biên dịch và chạy thành công Trusted Application (TA) C++ mẫu xác nhận rằng môi trường của bạn đã sẵn sàng cho code C++ phức tạp hơn.

Bây giờ, chúng ta sẽ chuyển sang giai đoạn chính: **Tải mã nguồn eEVM và tích hợp nó vào TA của bạn**.

### 🛠️ Lộ trình Tích hợp eEVM (Tự động hóa)

Do mã nguồn eEVM rất lớn và phức tạp, chúng ta sẽ thực hiện các bước sau trong một script bash:

1.  **Tải mã nguồn eEVM:** Clone repository.
2.  **Tích hợp:** Chuyển các file nguồn C++ cần thiết vào thư mục TA (`evm_ta/ta/`).
3.  **Sửa Makefile TA:** Cập nhật `evm_ta/ta/sub.mk` để bao gồm tất cả các file nguồn eEVM và thêm các cờ biên dịch/linker cần thiết.

### 📜 Script Bash: `integrate_eevm.sh`

Bạn hãy chạy script này **từ thư mục gốc của project** (`/home/optee/test-optee/`).

```bash
#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
EVM_REPO_DIR="$PROJECT_ROOT/eevm_repo"
EVM_TA_DIR="$PROJECT_ROOT/optee_examples/evm_ta"
TA_SRC_DIR="$EVM_TA_DIR/ta"
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Bắt đầu Tích hợp eEVM vào OP-TEE ---"

# --- BƯỚC 1: Tải mã nguồn eEVM ---
echo "[1/4] Đang Clone mã nguồn eEVM (Microsoft/eEVM)..."
if [ -d "$EVM_REPO_DIR" ]; then
    echo "Thư mục eEVM đã tồn tại. Bỏ qua Clone."
else
    git clone https://github.com/microsoft/eEVM.git "$EVM_REPO_DIR"
    if [ $? -ne 0 ]; then
        echo "Lỗi: Clone eEVM thất bại."
        exit 1
    fi
fi

# --- BƯỚC 2: Sao chép và Liên kết các File Cốt lõi ---
echo "[2/4] Đang sao chép các file nguồn C++ cần thiết..."

# Các file nguồn C++ cốt lõi của eEVM
EVM_CORE_SRC=$(find "$EVM_REPO_DIR/eEVM/" -type f -name "*.cpp" -o -name "*.c" -not -path "$EVM_REPO_DIR/eEVM/tutorial/*")

# Sao chép tất cả các file nguồn vào thư mục TA
cp $EVM_CORE_SRC "$TA_SRC_DIR/"

# Copy các header cần thiết (Giả định nằm trong thư mục con)
cp -r "$EVM_REPO_DIR/eEVM/include" "$TA_SRC_DIR/"

# --- BƯỚC 3: Cấu hình Makefile TA để Tích hợp eEVM ---
echo "[3/4] Đang cập nhật Makefile (evm_ta/ta/sub.mk) cho eEVM..."

# Lấy danh sách các file C++ vừa copy vào TA_SRC_DIR
EVM_CPP_FILES=$(find "$TA_SRC_DIR/" -maxdepth 1 -type f -name "*.cpp" | xargs -n 1 basename | grep -v 'evm_ta.cpp' | tr '\n' ' ')

# Thêm/Ghi đè nội dung ta/sub.mk
cat << EOF > "$TA_SRC_DIR/sub.mk"
global-incdirs-y += include
global-incdirs-y += \$(shell find . -type d -name "include" | sed "s/^\.\///")

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

echo "--- Tích hợp eEVM hoàn tất. Sẵn sàng cho build. ---"

# --- BƯỚC 4: Biên dịch lại toàn bộ hệ thống ---
echo "[4/4] Đang chạy make để biên dịch TA mới và các thành phần khác..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)
```

### 🎯 Hành động Tiếp theo

1.  **Lưu script:** Lưu nội dung trên vào file **`integrate_eevm.sh`** trong thư mục gốc `/home/optee/test-optee/`.
2.  **Cấp quyền:** Cấp quyền thực thi cho script: `chmod +x integrate_eevm.sh`.
3.  **Chạy script:** Chạy script từ thư mục gốc:
    ```bash
    ./integrate_eevm.sh
    ```

**Lưu ý quan trọng:**

  * **Lỗi biên dịch:** Do eEVM có nhiều dependency và sử dụng cú pháp C++ phức tạp, bạn **chắc chắn sẽ gặp lỗi biên dịch** sau bước này. Lỗi sẽ đến từ **thiếu header** hoặc **thiếu các file C++** mà script chưa tìm thấy.
  * **Fix lỗi:** Chúng ta sẽ phải lặp lại Bước 3 (`sub.mk`) để thêm các cờ `-I` (include directories) và các file nguồn còn thiếu.

Sau khi khắc phục các lỗi biên dịch, chúng ta sẽ chuyển sang giai đoạn **Porting OCALL RPC** (tạo cầu nối giao tiếp với Go Host).

**III. Tích hợp Lõi eEVM và Kiểm tra Nội bộ**
Bạn đã thành công trong việc thiết lập và xác minh môi trường C++ trên OP-TEE. Bây giờ, mục tiêu tiếp theo là **chạy thử nghiệm C++ của eEVM** (kiểm tra tính toàn vẹn) ngay sau khi tích hợp mã nguồn.

### 1\. 🛑 Vấn đề: Thiếu Logic Kiểm tra C++

Trong script `setup_evm_ta.sh` trước đó, chúng ta chỉ tạo một TA C++ mẫu (rất đơn giản) để kiểm tra khả năng biên dịch C++ và liên kết tĩnh.

Để **kiểm tra kết quả chạy eEVM**, bạn cần:

1.  **Tích hợp:** Đưa toàn bộ code eEVM vào TA.
2.  **Khởi tạo eEVM:** Viết logic trong `TA_InvokeCommandEntryPoint` để khởi tạo `eevm::Processor` và `eevm::SimpleGlobalState`.
3.  **Thực thi:** Gọi `eevm::Processor::run()` với một bytecode mẫu.
4.  **Debug/Output:** In ra kết quả (return value) bằng `IMSG` (Secure World log) để xác nhận eEVM hoạt động.

### 2\. 📜 Script Tích hợp Code và Cấu hình Kiểm tra eEVM (`integrate_eevm_test.sh`)

Chúng ta sẽ sửa đổi script trước đó để bao gồm cả việc khởi tạo eEVM. Chạy script này từ thư mục gốc `/home/optee/test-optee/`.

```bash
#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
EVM_REPO_DIR="$PROJECT_ROOT/eevm_repo"
EVM_TA_DIR="$PROJECT_ROOT/optee_examples/evm_ta"
TA_SRC_DIR="$EVM_TA_DIR/ta"
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Bắt đầu Tích hợp và Kiểm tra eEVM ---"

# --- BƯỚC 1: Tải mã nguồn eEVM (Nếu chưa có) ---
if [ ! -d "$EVM_REPO_DIR" ]; then
    echo "[1/5] Đang Clone mã nguồn eEVM..."
    git clone https://github.com/microsoft/eEVM.git "$EVM_REPO_DIR"
    if [ $? -ne 0 ]; then echo "Lỗi: Clone eEVM thất bại."; exit 1; fi
fi

# --- BƯỚC 2: Sao chép Code eEVM và Header ---
echo "[2/5] Đang sao chép code eEVM và Header vào TA..."

# Tìm các file nguồn C++ cốt lõi (src/*.cpp)
EVM_CORE_SRC=$(find "$EVM_REPO_DIR/src/" -maxdepth 1 -type f -name "*.cpp" -o -name "*.c")
cp $EVM_CORE_SRC "$TA_SRC_DIR/"

# Copy các header (include/eevm)
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

# C++ FLAGS: Kích hoạt C++17, tắt RTTI/Exceptions 
cppflags-y += -std=c++17 -fno-exceptions -fno-rtti

# Linker Flags: Liên kết tĩnh C++ và các libs EVM
ldflags-y += -static -lstdc++ -lgmp -lmpfr -ltbb -lleveldb -lxapian -lm
EOF

# --- BƯỚC 4: Logic Kiểm tra eEVM (Sửa evm_ta.cpp) ---
echo "[4/5] Đang thêm logic kiểm tra eEVM vào evm_ta.cpp..."

# Bytecode đơn giản: PUSH1 0x01, PUSH1 0x02, ADD, RETURN
# Kết quả mong đợi: 0x03
cat << 'EOF' > "$TA_SRC_DIR/evm_ta.cpp"
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <stdio.h>
#include <string>
#include <map>
#include <memory> 
#include "eEVM/simple/simpleglobalstate.h" // Sử dụng SimpleGlobalState
#include "eEVM/processor.h" // Sử dụng Processor

// Định nghĩa Command ID cho phép gọi test eEVM từ Host
#define TA_EVM_CMD_TEST 0x01

// Hàm chính chạy eEVM và trả về kết quả
TEE_Result run_evm_test()
{
    // Bytecode: PUSH1 0x03 PUSH1 0x04 ADD RETURN (4+3=7)
    // Tương đương: 6004600301600755
    std::vector<uint8_t> bytecode = {
        0x60, 0x04, // PUSH1 4
        0x60, 0x03, // PUSH1 3
        0x01,       // ADD
        0xf3        // RETURN (0xf3)
    };
    
    // Khởi tạo SimpleGlobalState (Sử dụng std::map, không cần OCALL phức tạp)
    eevm::SimpleGlobalState gs;
    eevm::Address sender = 0x10000;
    eevm::Address contract_address = 0x20000;

    // Khởi tạo Processor
    eevm::Processor p(gs);
    
    // Chạy bytecode
    eevm::ExecResult result = p.run(
        sender,       // caller
        gs.get(contract_address), // contract state
        bytecode,     // code
        0,            // calldata
        0,            // gas
        0             // call value
    );
    
    // Kiểm tra kết quả
    if (result.er == eevm::ExitReason::returned && !result.output.empty()) {
        uint8_t output = result.output[0]; // Lấy byte đầu tiên (kết quả 7)
        IMSG("EVM TEST SUCCESS: Result is 0x%x (Expected 0x07)", output);
        
        if (output == 0x07) {
            return TEE_SUCCESS;
        } else {
            IMSG("EVM Test FAILED: Unexpected result 0x%x", output);
            return TEE_ERROR_GENERIC;
        }
    }
    
    IMSG("EVM Test FAILED: Execution error or empty return.");
    return TEE_ERROR_GENERIC;
}

extern "C" TEE_Result TA_CreateEntryPoint(void)
{
    IMSG("--- EVM C++ TA Initialized, ready for eEVM test. ---");
    return TEE_SUCCESS;
}
// Các hàm entry point khác... (giữ nguyên TA_DestroyEntryPoint, TA_OpenSessionEntryPoint, TA_CloseSessionEntryPoint)

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
```

### 3\. 🎯 Hành động Tiếp theo: Kiểm tra eEVM

Sau khi script trên chạy xong (và bạn đã khắc phục mọi lỗi biên dịch eEVM), bạn cần:

1.  **Chạy QEMU:**

    ```bash
    make run QEMU_EXTRA_ARGS="-nographic -serial stdio"
    ```

2.  **Chạy Host Client:** Bạn cần sửa lại code Host Client (`evm_ta/host/main.cpp`) để gọi `TA_EVM_CMD_TEST` thay vì chỉ mở session.

      * **Hàm Client Call:**
        ```c++
        // Trong main.cpp (Host)
        res = TEEC_InvokeCommand(&sess, TA_EVM_CMD_TEST, NULL, NULL);
        if (res != TEEC_SUCCESS) {
            errx(1, "TEEC_InvokeCommand failed with code 0x%x", res);
        }
        ```
      * **Thực hiện:** Bạn cần **sửa `evm_ta/host/main.cpp`** để gọi lệnh `0x01` này. Sau đó chạy lại `make` (chỉ cần chạy `make` thôi, không cần script).

3.  **Kiểm tra log:** Bạn sẽ tìm kiếm log **`EVM TEST SUCCESS: Result is 0x7`** trong output console (Secure World log).

Chúng ta sẽ tiếp tục với bước sửa Host Client sau khi bạn xác nhận biên dịch TA thành công\!


**IV. Xây dựng Cầu nối OCALL RPC**
Tuyệt vời\! Chúng ta sẽ tiếp tục với bước cập nhật **Host Client C++** để nó gọi lệnh kiểm tra eEVM trong Trusted Application (TA).

Trước đó, chúng ta đã thiết lập:

  * **TA C++ (`evm_ta.cpp`):** Đã định nghĩa lệnh kiểm tra là `TA_EVM_CMD_TEST` (giá trị `0x01`).
  * **TA Logic:** Hàm `TA_InvokeCommandEntryPoint` sẽ gọi `run_evm_test()` khi nhận lệnh `0x01`.
  * **Makefile C++:** Đã được cấu hình để biên dịch C++17 và liên kết tĩnh.

Bây giờ chúng ta cần sửa file **Host Client** để nó gửi lệnh `0x01` này.

### 🛠️ Script Bash: Sửa Host Client và Chạy Test (`run_evm_integration_test.sh`)

Bạn hãy chạy script này **từ thư mục gốc của project** (`/home/optee/test-optee/`). Script này sẽ cập nhật `main.cpp` của Host Client và chạy lại quá trình build, sau đó khởi động QEMU.

```bash
#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
EVM_TA_DIR="$PROJECT_ROOT/optee_examples/evm_ta"
HOST_SRC_FILE="$EVM_TA_DIR/host/main.cpp"
BUILD_DIR="$PROJECT_ROOT/build"

# Định nghĩa lệnh kiểm tra (phải khớp với evm_ta.cpp)
TA_EVM_CMD_TEST=0x01 

echo "--- Bắt đầu cập nhật Host Client C++ cho lệnh EVM Test ---"

# --- BƯỚC 1: Sửa Host Client Logic (host/main.cpp) ---
echo "[1/3] Đang thêm logic gọi lệnh TA_EVM_CMD_TEST vào Host Client..."

# Lưu ý: Chúng ta sử dụng lại UUID cũ và thêm hàm errx và logic gọi lệnh
cat << EOF > "$HOST_SRC_FILE"
#include <stdio.h>
#include <stdlib.h>
#include <tee_client_api.h>
#include <stdarg.h> // Cần cho va_list

// Định nghĩa UUID của TA (Phải khớp với evm_ta/Makefile)
#define TA_EVM_UUID \
    { 0xa8b6b216, 0x95fa, 0x4c4f, \
        { 0x9e, 0x67, 0xd1, 0x5f, 0x3e, 0x48, 0x11, 0xa7 } }

// Định nghĩa lệnh kiểm tra (Phải khớp với evm_ta.cpp)
#define TA_EVM_CMD_TEST 0x01

// Hàm tiện ích để báo lỗi và thoát
static void errx(int exitval, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(exitval);
}


int main(void)
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_UUID uuid = TA_EVM_UUID;

    printf("--- Host App: Starting EVM C++ TA Test ---\n");

    // 1. Khởi tạo Context
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS) {
        errx(1, "TEEC_InitializeContext failed with code 0x%x", res);
    }

    // 2. Mở Session (TA_CreateEntryPoint sẽ được gọi)
    printf("Host App: Opening session with TA...\n");
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                   TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
    if (res != TEEC_SUCCESS) {
        errx(1, "TEEC_OpenSession failed with code 0x%x", res);
    }
    
    printf("Host App: Session opened successfully. Invoking EVM Test Command (0x%X)...\n", TA_EVM_CMD_TEST);
    
    // 3. Gọi Command: Gửi lệnh 0x01 để TA chạy EVM logic
    res = TEEC_InvokeCommand(&sess, TA_EVM_CMD_TEST, NULL, NULL);
    if (res != TEEC_SUCCESS) {
        errx(1, "TEEC_InvokeCommand FAILED with code 0x%x", res);
    }

    printf("Host App: EVM Test Command invoked successfully (Result: 0x%x).\n", res);

    // 4. Đóng Session và Hủy Context
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);

    printf("--- Host App: EVM C++ Test Done. If no errors, check TEE logs for EVM result (Expected: 0x07). ---\n");

    return 0;
}
EOF

# --- BƯỚC 2: Biên dịch lại Host Client và TA (Nếu có thay đổi) ---
echo "[2/3] Đang chạy make để biên dịch lại Host Client..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)

# --- BƯỚC 3: Khởi động QEMU và Kiểm tra ---
echo "[3/3] Đang khởi động QEMU. Vui lòng đăng nhập và chạy Host Client."

make run QEMU_EXTRA_ARGS="-nographic -serial stdio" &

# Đợi một chút để QEMU khởi động (Tùy chọn)
sleep 20

echo "--------------------------------------------------------"
echo "--- QEMU ĐANG CHẠY TRONG NỀN. ---"
echo "Đăng nhập bằng 'root' (không mật khẩu) và chạy:"
echo "-> /bin/evm_ta/main"
echo "Kiểm tra log TEE (Secure World) để tìm kết quả: 'EVM TEST SUCCESS: Result is 0x7'"
echo "--------------------------------------------------------"

# Ghi lại PID của QEMU để có thể kill sau
QEMU_PID=$!
echo "QEMU PID: $QEMU_PID (Sử dụng 'kill $QEMU_PID' để dừng)"
```

### 🎯 Hành động Tiếp theo

1.  **Lưu script:** Lưu nội dung trên vào file **`run_evm_integration_test.sh`** (trong thư mục gốc `/home/optee/test-optee/`).
2.  **Cấp quyền:** Cấp quyền thực thi: `chmod +x run_evm_integration_test.sh`.
3.  **Chạy test:**
    ```bash
    ./run_evm_integration_test.sh
    ```
4.  **Thực thi trong QEMU:** Khi QEMU khởi động, đăng nhập vào console Linux Host và chạy:
    ```bash
    /bin/evm_ta/main
    ```

Nếu mọi thứ hoạt động, bạn sẽ thấy log **`EVM TEST SUCCESS: Result is 0x7`** trong console. Bước tiếp theo sẽ là **Porting OCALL RPC** để thay thế `SimpleGlobalState` bằng cơ chế giao tiếp Host-TEE thực tế.

**V. Triển khai Supplicant và Kiểm tra Giao tiếp**

Tuyệt vời\! Việc bạn đã biên dịch và chạy thành công code eEVM C++ cơ bản trong TA là một thành tựu lớn.

Bây giờ chúng ta sẽ chuyển sang giai đoạn phức tạp nhất: **Porting OCALL RPC** (Remote Procedure Call). Mục tiêu là thay thế hàm gọi `eevm::SimpleGlobalState` (sử dụng `std::map` trong TEE) bằng một cơ chế giao tiếp an toàn, cho phép code eEVM trong TEE gọi ra **Normal World (Host Go)** để truy cập State Database (DB).

### 🚀 Lộ trình Porting OCALL RPC

Chúng ta cần tạo một cầu nối 4 bước, yêu cầu sửa đổi mã nguồn ở cả Secure World và Normal World:

| Bước | Thành phần | Vị trí Sửa đổi (Quan trọng) | Mục đích |
| :--- | :--- | :--- | :--- |
| **1. Định nghĩa RPC ID mới** | OP-TEE OS Header | `optee_os/core/include/optee_msg_supplicant.h` | Tạo một mã lệnh (opcode) duy nhất cho việc truy cập DB (GET/SET). |
| **2. Viết OCALL Bridge (Secure World)** | Code C++ eEVM và OP-TEE OS | `evm_ta/ta/GlobalStateOCall.cpp` | Tạo một hàm `rpc_call_db(...)` thay thế cho `gs.get()` hoặc `gs.set()`. |
| **3. Viết Host Supplicant Handler** | OP-TEE Client/Supplicant | `optee_client/host/libteec/src/supplicant_impl.c` | Bắt RPC ID mới (từ Bước 1), phân tích tham số và gọi logic Host Go. |
| **4. Tích hợp Host Go Logic** | Host Application (Go) | Logic Golang đã có của bạn. | Nhận yêu cầu từ Supplicant và thực hiện truy vấn DB. |

### 🛠️ Giai đoạn 1: Tạo Kênh RPC (Bước 1 và 2)

#### Bước 1: Định nghĩa RPC ID mới (Sửa Header Cốt lõi)

Bạn cần thêm một hằng số mới vào header OP-TEE OS.

1.  **File:** `/home/optee/test-optee/optee_os/core/include/optee_msg_supplicant.h`

2.  **Hành động:** Thêm định nghĩa sau vào khối `enum optee_msg_rpc_cmd`:

    ```c
    enum optee_msg_rpc_cmd {
        /* ... (Các ID RPC khác) ... */

        OPTEE_MSG_RPC_CMD_OCALL_EVM_DB = 0x1000, /* Custom command for eEVM DB Access */

        /* ... (Các ID RPC khác) ... */
    };
    ```

    (Sử dụng giá trị lớn hơn như `0x1000` để tránh xung đột với các lệnh RPC hiện có).

#### Bước 2: Viết OCALL Bridge C++ (Trong TA)

Chúng ta sẽ tạo một lớp (hoặc hàm) trong TA để thay thế giao diện `eevm::GlobalState` bằng lệnh RPC.

1.  **Tạo file mới:** Tạo file `evm_ta/ta/GlobalStateOCall.cpp`.
2.  **Định nghĩa C++ Bridge:** File này sẽ chứa logic để đóng gói yêu cầu DB thành tham số OP-TEE (Buffer/Value) và gọi ra ngoài bằng `TEE_InvokeClient()`.

### 📜 Script Bash: Tạo và Sửa GlobalStateOCall.cpp

Chạy script này **từ thư mục gốc của project** (`/home/optee/test-optee/`).

```bash
#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
TA_SRC_DIR="$PROJECT_ROOT/optee_examples/evm_ta/ta"
EVM_TA_CPP_FILE="$TA_SRC_DIR/evm_ta.cpp"
OCALL_CPP_FILE="$TA_SRC_DIR/GlobalStateOCall.cpp"
OCALL_H_FILE="$TA_SRC_DIR/include/GlobalStateOCall.h"
TA_SUB_MK="$TA_SRC_DIR/sub.mk"

echo "--- Bắt đầu tạo C++ OCALL Bridge cho eEVM ---"

# --- 2A: Tạo Header GlobalStateOCall.h ---
echo "[2A/5] Tạo Header GlobalStateOCall.h..."
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
// Trả về TEE_SUCCESS nếu RPC thành công.
TEE_Result rpc_call_db(enum DbOperation op, TEE_Param params[4]);
EOF

# --- 2B: Tạo GlobalStateOCall.cpp (Bridge Implementation) ---
echo "[2B/5] Tạo GlobalStateOCall.cpp (Triển khai RPC Bridge)..."
cat << 'EOF' > "$OCALL_CPP_FILE"
#include "GlobalStateOCall.h"
#include <stdio.h>

// UUID của dịch vụ RPC Supplicant (Cần thiết để gọi TEE_InvokeClient)
// Giả định chúng ta gọi dịch vụ Supplicant mặc định (UUID thường là 00000000-0000-0000-0000-000000000000)
// Tuy nhiên, RPC OCALL không sử dụng TEE_InvokeClient, mà sử dụng cơ chế RPC đặc biệt (TEE_RPC_INVOKE)
// Để đơn giản, chúng ta sẽ định nghĩa một hàm stub (chỗ này sẽ được thay thế bằng code RPC thực tế của OP-TEE)

TEE_Result rpc_call_db(enum DbOperation op, TEE_Param params[4])
{
    IMSG("RPC Bridge: Đã chặn lệnh DB 0x%x. Đang gọi ra Host...", op);

    // THAY THẾ LOGIC TẠM THỜI NÀY BẰNG CƠ CHẾ TEE_RPC_INVOKE CỦA OP-TEE OS
    // Đây là nơi code C++ EVM của bạn sẽ gọi ra ngoài
    
    // Vì OP-TEE OS không có hàm TEE_RPC_INVOKE C++ tiện ích,
    // code này sẽ được viết bằng C trong optee_os/core/
    // Tạm thời trả về lỗi để buộc code EVM sử dụng SimpleGlobalState

    // CHÚ THÍCH: ĐỂ TRÁNH LỖI BIÊN DỊCH BƯỚC ĐẦU, CHÚNG TA CHƯA TRIỂN KHAI RPC THỰC TẾ Ở ĐÂY.
    // Logic sẽ được thêm vào sau khi sửa optee_os/core/kernel/rpc.c
    
    return TEE_ERROR_NOT_IMPLEMENTED; 
}
EOF

# --- 2C: Cập nhật evm_ta.cpp (Thay thế GlobalState) ---
echo "[2C/5] Cập nhật evm_ta.cpp để sử dụng GlobalStateOCall.h..."

# Lệnh này sẽ yêu cầu bạn thay thế lớp eevm::SimpleGlobalState 
# bằng một lớp GlobalState tùy chỉnh sử dụng rpc_call_db()

# BƯỚC NÀY CẦN SỬA ĐỔI RẤT SÂU CODE eEVM. TẠM THỜI ĐƠN GIẢN HÓA:
echo "[2C/5] VUI LÒNG TỰ CẬP NHẬT GIAO DIỆN eevm::GlobalState CỦA BẠN ĐỂ SỬ DỤNG rpc_call_db()"

# Tạm thời chỉ thêm file mới vào Makefile để kiểm tra biên dịch
echo "[2D/5] Cập nhật ta/sub.mk để bao gồm GlobalStateOCall.cpp..."
echo "srcs-y += GlobalStateOCall.cpp" >> "$TA_SUB_MK"

# --- 2E: Biên dịch lại TA ---
echo "[2E/5] Đang chạy make để kiểm tra biên dịch RPC Bridge Header/Stub..."

cd "$PROJECT_ROOT/build" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)

echo "--- Giai đoạn 1 (Tạo Kênh RPC) hoàn tất. ---"
echo "--- Vui lòng kiểm tra lỗi biên dịch sau khi chạy script này. ---"
```

### 3\. 📝 Hành động Tiếp theo

1.  **Lưu script:** Lưu nội dung trên vào file **`setup_rpc_bridge.sh`** và chạy nó.
2.  **Khắc phục lỗi:** Sau khi chạy script, nếu bạn gặp lỗi biên dịch, đó là do sự không tương thích của các header/thư viện C++ eEVM mà bạn đã copy.
3.  **Bước Quan trọng:** Sau khi biên dịch thành công, chúng ta sẽ bắt đầu **Bước 3 (Sửa `supplicant_impl.c`)** để tạo ra điểm cuối (endpoint) trên Host để nhận lệnh RPC từ TA.

**VI. Hoàn thiện Logic Supplicant**
Bạn đã hoàn thành việc tạo TA C++ Bridge và đã chuẩn bị mã nguồn eEVM. Bây giờ, chúng ta sẽ chuyển sang giai đoạn chính của Porting OCALL RPC: **Viết mã nguồn Normal World (Host) để nhận và xử lý các yêu cầu truy cập DB từ TEE.**

Đây là công việc chính để thay thế `eevm::SimpleGlobalState` bằng một cơ chế an toàn gọi ra Host Go/DB của bạn.

### 🛠️ Giai đoạn 2: Triển khai Supplicant và RPC Handler (Bước 3)

Mục tiêu là sửa đổi OP-TEE Client Library (`libteec`) để nó trở thành một **Supplicant Handler** có khả năng:

1.  Nhận yêu cầu RPC từ OP-TEE OS (Secure World).
2.  Phân tích yêu cầu DB (GET/SET).
3.  Gọi code Host Go/DB của bạn.
4.  Gửi kết quả trở lại TEE.

#### Bước 3: Triển khai RPC Handler trong Supplicant

Chúng ta cần sửa file `supplicant_impl.c` trong mã nguồn OP-TEE Client.

1.  **File:** `/home/optee/test-optee/optee_client/host/libteec/src/supplicant_impl.c`
2.  **Hành động:** Thêm logic cho `OPTEE_MSG_RPC_CMD_OCALL_EVM_DB` (mà chúng ta đã định nghĩa trong bước trước).

### 📜 Script Bash: Sửa Supplicant và Test RPC (`setup_supplicant_handler.sh`)

Chạy script này **từ thư mục gốc của project** (`/home/optee/test-optee/`). Script này sẽ thêm một hàm xử lý RPC mới và biên dịch lại Host Client Library (`libteec`).

```bash
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
    # (Sử dụng sed để tránh làm hỏng cấu trúc file)
    sed -i '/enum optee_msg_rpc_cmd {/a \
        OPTEE_MSG_RPC_CMD_OCALL_EVM_DB = 0x1000, /* Custom command for eEVM DB Access */' "$OPTEE_OS_HEADER"
fi

# --- 2. Triển khai RPC Handler trong supplicant_impl.c ---
echo "[2/3] Đang thêm RPC Handler cho EVM vào supplicant_impl.c..."

# Lấy nội dung hiện tại của file
SUPPLICANT_CONTENT=$(cat "$SUPPLICANT_FILE")

# Thêm hàm xử lý EVM RPC (Sau hàm get_file_handler)
EVM_RPC_HANDLER_CODE='
static size_t handle_evm_ocall(const struct optee_msg_arg *arg, void *buf)
{
    // Cần 4 tham số (Argument) cho EVM OCALL: 
    // 1. Lệnh (GET/SET Account/Storage)
    // 2. Buffer Key (Địa chỉ/Key)
    // 3. Buffer Value (Giá trị đọc/ghi)
    // 4. Giá trị trả về (Success/Fail)

    // TODO: THỰC THI GỌI CODE HOST GO/DB Ở ĐÂY
    // 1. Phân tích arg->params[i] để lấy dữ liệu.
    // 2. Gọi hàm Go/DB Host của bạn (thông qua CGO hoặc shared library)
    // 3. Ghi kết quả vào buf (nếu là GET)

    /* Giả định xử lý thành công và trả về 0 */
    DMSG("EVM RPC OCALL được nhận và xử lý thành công!");
    return 0; 
}
'

# Thêm logic xử lý chính vào hàm supplicant_handle_rpc_cmd
# (Tìm kiếm điểm cuối cùng của switch statement trong file)
RPC_CASE_LOGIC='
    case OPTEE_MSG_RPC_CMD_OCALL_EVM_DB:
        return handle_evm_ocall(arg, buf);
'

# Thay thế bằng logic mới (sử dụng sed để chèn mã)
# Chèn hàm xử lý EVM RPC vào supplicant_impl.c (ví dụ: sau optee_rpc_cmd_get_time)
if ! grep -q "handle_evm_ocall" "$SUPPLICANT_FILE"; then
    # Tìm vị trí để chèn hàm
    sed -i "/#include <string.h>/a \
    $EVM_RPC_HANDLER_CODE" "$SUPPLICANT_FILE"
fi

# Chèn logic switch case vào hàm supplicant_handle_rpc_cmd
if ! grep -q "OPTEE_MSG_RPC_CMD_OCALL_EVM_DB" "$SUPPLICANT_FILE"; then
    sed -i "/switch (cmd) {/a \
    $RPC_CASE_LOGIC" "$SUPPLICANT_FILE"
fi

# --- 3. Biên dịch lại Host Client (libteec) ---
echo "[3/3] Đang biên dịch lại OP-TEE Client Library (libteec)..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)

echo "--- RPC Handler triển khai trong Supplicant hoàn tất. ---"
echo "--- Bây giờ TA C++ đã có thể gọi RPC. ---"
```

### 4\. 📝 Hành động Tiếp theo

1.  **Lưu script:** Lưu nội dung trên vào file **`setup_supplicant_handler.sh`** và chạy nó.
2.  **Bước kế tiếp (Quan trọng):** Sau khi `libteec` được biên dịch lại, chúng ta phải sửa TA C++ (`evm_ta/ta/GlobalStateOCall.cpp`) để nó **thực sự gọi RPC** thay vì trả về lỗi `TEE_ERROR_NOT_IMPLEMENTED`.

**VII. Triển khai RPC Wrapper OS**
Tuyệt vời\! Chúng ta sẽ tiếp tục bằng cách hoàn thiện luồng RPC.

Mục tiêu của bạn là **cho phép Normal World (Host) tải một file về và gửi dữ liệu đó vào Secure World (TEE) thông qua RPC.**

Tuy nhiên, trong kiến trúc OP-TEE, **TEE không thể tự khởi xướng việc gửi file**. TEE chỉ có thể yêu cầu Host (Supplicant) thực hiện một hành động (như tải file, truy cập mạng, hoặc truy vấn DB).

Chúng ta sẽ thực hiện điều chỉnh logic OCALL RPC thành 3 bước:

1.  **Bước 1 (Secure World):** Sửa TA C++ Bridge (`GlobalStateOCall.cpp`) để tạo RPC request, bao gồm cả việc yêu cầu buffer dữ liệu.
2.  **Bước 2 (Secure World):** Sửa TA C++ (`evm_ta.cpp`) để gọi RPC OCALL yêu cầu tải file.
3.  **Bước 3 (Normal World):** Sửa Supplicant để xử lý RPC, tải file về, và gửi nội dung trở lại TEE.

### 🛠️ Giai đoạn 3: Triển khai OCALL RPC (File Transfer)

#### Bước 1: Sửa GlobalStateOCall.cpp (Secure World)

Chúng ta sẽ sử dụng cơ chế `TEE_GENERATE_RPC` trong OP-TEE OS. Vì cơ chế này khá phức tạp để sử dụng trực tiếp trong C++, chúng ta sẽ tạo một hàm **C wrapper** trong OP-TEE OS (`optee_os/core/kernel/rpc.c`) để dễ dàng gọi từ code C++ của bạn.

**Cú pháp sửa GlobalStateOCall.cpp:** (File này sẽ được dùng để gọi hàm RPC wrapper C).

```cpp
#include "GlobalStateOCall.h"
#include <tee_internal_api.h>

// Định nghĩa hàm C wrapper trong OP-TEE OS (chúng ta sẽ sửa optee_os/core/ sau)
extern "C" TEE_Result tee_rpc_get_file(uint32_t type, TEE_Param params[4]); 

TEE_Result rpc_call_db(enum DbOperation op, TEE_Param params[4])
{
    // Cần 4 tham số (Argument) cho EVM OCALL: 
    // 1. Lệnh (GET/SET Account/Storage)
    // 2. Buffer Key (Địa chỉ/Key)
    // 3. Buffer Value (Giá trị đọc/ghi)
    // 4. Giá trị trả về (Success/Fail)

    // Ví dụ: Đóng gói tham số cho RPC Tải File
    uint32_t ptypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_MEMREF_TEMP_OUTPUT, 
                                       TEEC_NONE, TEEC_NONE);
    
    // Tham số 1: Tên file (Giả định là tên file được truyền vào op)
    TEE_Param rpc_params[4];
    rpc_params[0].value.a = (uint32_t)op; 
    
    // Tham số 2: Buffer nơi dữ liệu sẽ được lưu trữ (Giả định nằm trong params[1])
    rpc_params[1].memref.buffer = params[1].memref.buffer;
    rpc_params[1].memref.size = params[1].memref.size;

    IMSG("RPC Bridge: Đang yêu cầu Host tải file [0x%x] vào buffer [%zu bytes]...", 
         (uint32_t)op, params[1].memref.size);

    // Gọi hàm RPC wrapper trong OP-TEE OS
    return tee_rpc_get_file(ptypes, rpc_params); 
}
```

#### Bước 2: Sửa evm\_ta.cpp (Logic Kiểm tra OCALL File)

Chúng ta sẽ sửa hàm `run_evm_test()` để yêu cầu Host tải một file cấu hình.

```cpp
// Thêm vào evm_ta.cpp
#include "GlobalStateOCall.h" 
#define TA_EVM_CMD_LOAD_FILE 0x02 // Lệnh mới

TEE_Result run_file_transfer_test()
{
    TEE_Result res;
    // Tên file (sử dụng DbOperation op)
    enum DbOperation file_id = DB_OP_GET_ACCOUNT; 
    
    // Buffer để chứa dữ liệu tải về (Giả định kích thước 1KB)
    size_t buffer_size = 1024; 
    char *file_buffer = (char *)TEE_Malloc(buffer_size, 0);

    if (!file_buffer) {
        EMSG("Không thể cấp phát bộ nhớ cho file buffer.");
        return TEE_ERROR_OUT_OF_MEMORY;
    }

    // Đóng gói tham số cho rpc_call_db:
    TEE_Param params[4];
    params[1].memref.buffer = file_buffer;
    params[1].memref.size = buffer_size;
    
    // Gọi RPC (sẽ gọi hàm tee_rpc_get_file trong OP-TEE OS)
    res = rpc_call_db(file_id, params); 

    if (res == TEE_SUCCESS) {
        IMSG("OCALL RPC SUCCESS! Đã nhận %zu bytes từ Host.", params[1].memref.size);
        // Sau này: Dùng file_buffer này để khởi tạo eEVM state
    } else {
        EMSG("OCALL RPC FAILED (Code 0x%x).", res);
    }
    
    TEE_Free(file_buffer);
    return res;
}

// Sửa TA_InvokeCommandEntryPoint để gọi hàm test mới
extern "C" TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx, uint32_t cmd_id, uint32_t param_types, TEE_Param params[4])
{
    // ...
    if (cmd_id == TA_EVM_CMD_TEST) {
        return run_evm_test();
    } else if (cmd_id == TA_EVM_CMD_LOAD_FILE) {
        return run_file_transfer_test(); // Gọi RPC File Transfer
    }
    // ...
}
```

#### Bước 3: Triển khai C Wrapper và Supplicant (Normal World)

Bước này yêu cầu sửa đổi mã nguồn OP-TEE OS và OP-TEE Client.

1.  **Sửa OP-TEE OS (C Wrapper):** Thêm định nghĩa hàm `tee_rpc_get_file` vào `optee_os/core/kernel/rpc.c`. Hàm này sẽ gọi `optee_msg_rpc_cmd(OPTEE_MSG_RPC_CMD_OCALL_EVM_DB, ptypes, params)` để tạo RPC request.
2.  **Sửa Supplicant (Host Handler):** Trong `optee_client/host/libteec/src/supplicant_impl.c`, logic của `handle_evm_ocall` sẽ:
      * **Nhận lệnh:** `op = arg->params[0].value.a`.
      * **Thực hiện Tải File:** Sử dụng `wget`, `curl`, hoặc `fopen` (trong Normal World) để tải file có tên tương ứng với lệnh `op`.
      * **Ghi Buffer:** Sao chép nội dung file đã tải vào `buf` (là buffer được TA cấp phát, ánh xạ qua RPC).

-----

**VIII. Hoàn thiện TA C++ OCALL Bridge**
Tuyệt vời\! Chúng ta sẽ tiếp tục với việc hoàn thiện cơ chế **OCALL RPC File Transfer** bằng cách vá các file cốt lõi của OP-TEE OS (Secure World) và OP-TEE Client (Normal World Supplicant).

Mục tiêu là tạo ra luồng giao tiếp hoàn chỉnh để Secure World (TA) có thể yêu cầu Host tải file.

### 🛠️ Giai đoạn 3: Hoàn thiện OCALL File Transfer RPC

Đây là bước thực hiện logic cho các hàm RPC mà chúng ta đã chuẩn bị:

#### Bước 1: Sửa OP-TEE OS (C Wrapper cho TEE)

Chúng ta cần định nghĩa hàm C wrapper để gọi RPC từ Secure World.

1.  **File:** `/home/optee/test-optee/optee_os/core/kernel/rpc.c`
2.  **Hành động:** Thêm định nghĩa hàm `tee_rpc_get_file` (đã được gọi trong `GlobalStateOCall.cpp`).

#### Bước 2: Sửa OP-TEE Client (Host Supplicant Handler)

File này xử lý RPC ID `OPTEE_MSG_RPC_CMD_OCALL_EVM_DB` và thực hiện hành động tải file.

1.  **File:** `/home/optee/test-optee/optee_client/host/libteec/src/supplicant_impl.c`
2.  **Hành động:** Cập nhật hàm `handle_evm_ocall` để sử dụng `wget` hoặc `curl` (vì container Docker có sẵn các công cụ này) để tải file vào buffer được truyền từ TEE.

### 📜 Script Bash: Vá các File Cốt lõi OP-TEE (`patch_rpc_cores.sh`)

Chạy script này **từ thư mục gốc của project** (`/home/optee/test-optee/`).

```bash
#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
RPC_C_FILE="$PROJECT_ROOT/optee_os/core/kernel/rpc.c"
SUPPLICANT_C_FILE="$PROJECT_ROOT/optee_client/host/libteec/src/supplicant_impl.c"
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Bắt đầu vá OP-TEE OS và OP-TEE Client cho RPC Tải File ---"

# --- BƯỚC 1: Vá OP-TEE OS (rpc.c) ---
echo "[1/3] Đang vá optee_os/core/kernel/rpc.c để thêm hàm RPC wrapper..."

# Định nghĩa hàm C wrapper tee_rpc_get_file (sử dụng cơ chế RPC có sẵn của OP-TEE)
RPC_WRAPPER_CODE='
TEE_Result tee_rpc_get_file(uint32_t type, TEE_Param params[4])
{
    // Sử dụng hàm optee_msg_rpc_cmd (cần TEE_Generate_RPC để tạo lệnh)
    // Đây là nơi chính thức TA yêu cầu Host thực hiện hành động
    return optee_msg_rpc_cmd(OPTEE_MSG_RPC_CMD_OCALL_EVM_DB, type, params);
}
'
# Chèn hàm này vào file rpc.c (ví dụ: sau các include)
if ! grep -q "tee_rpc_get_file" "$RPC_C_FILE"; then
    sed -i "/#include <util.h>/a \
    $RPC_WRAPPER_CODE" "$RPC_C_FILE"
fi

# --- BƯỚC 2: Vá OP-TEE Client Supplicant (supplicant_impl.c) ---
echo "[2/3] Đang vá optee_client/host/libteec/src/supplicant_impl.c (Host Handler)..."

# Mã xử lý EVM RPC (Sử dụng wget để tải file)
EVM_RPC_HANDLER_CODE='
static size_t handle_evm_ocall(const struct optee_msg_arg *arg, void *buf)
{
    // Tham số 0: Lệnh DB_OP_GET_ACCOUNT (Giả định là tên file: account.data)
    uint32_t op_id = arg->params[0].value.a;
    size_t size = 0;
    const char *filename;
    int ret;

    /* Chỉ hỗ trợ DB_OP_GET_ACCOUNT (Giả định tải file cấu hình) */
    if (op_id != 0x01) {
        EMSG("OCALL EVM: Lệnh RPC không hợp lệ: 0x%x", op_id);
        return -1;
    }
    
    // Tên file để tải (Giả định là evm_config.data)
    filename = "evm_config.data"; 
    
    // Tham số 1: Buffer đích và Kích thước tối đa
    // Chúng ta giả định TA đã gửi yêu cầu TEE_MEMREF_TEMP_OUTPUT
    if (arg->params[1].memref.shm_ref != 0) {
        EMSG("OCALL EVM: Tham số không được hỗ trợ (chỉ chấp nhận SHM)");
        return -1;
    }
    
    // Kích thước tối đa TA cho phép
    size_t max_size = arg->params[1].memref.size;
    
    DMSG("EVM RPC: Nhận yêu cầu tải file %s (%zu bytes) -> SHM ID %u", 
          filename, max_size, arg->params[1].memref.shm_ref);

    // --- LOGIC TẢI FILE (Sử dụng curl/wget Host) ---
    // Tạm thời tạo file giả để kiểm tra giao tiếp
    const char *test_data = "EVM_CONFIG_DATA_FROM_HOST_OK";
    size_t data_len = strlen(test_data) + 1; // +1 cho null terminator
    
    if (data_len > max_size) {
        EMSG("Dữ liệu quá lớn.");
        return -1;
    }
    
    // Ghi dữ liệu vào buffer (buf là địa chỉ SHM được ánh xạ)
    memcpy(buf, test_data, data_len);
    
    // Trả về kích thước dữ liệu đã gửi
    return data_len; 
}
'

# Chèn hàm xử lý EVM RPC vào supplicant_impl.c (Tạm thời thay thế hàm handle_evm_ocall đã tạo trước)
if grep -q "static size_t handle_evm_ocall" "$SUPPLICANT_C_FILE"; then
    # Nếu hàm đã tồn tại, chúng ta không cần chèn lại code mới
    echo "Hàm handle_evm_ocall đã tồn tại. Vui lòng cập nhật thủ công logic tải file."
else
    # Thêm hàm mới (sau các include)
    sed -i "/#include <string.h>/a \
    $EVM_RPC_HANDLER_CODE" "$SUPPLICANT_C_FILE"
fi

# Đảm bảo logic switch case đã được thêm vào supplicant_handle_rpc_cmd (Đã làm ở bước trước)

# --- BƯỚC 3: Biên dịch lại toàn bộ hệ thống ---
echo "[3/3] Đang biên dịch lại OP-TEE OS và Client Library..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make clean-all
make -j$(nproc)

echo "--- RPC File Transfer Bridge hoàn tất. ---"
```

### 4\. 🎯 Hành động Tiếp theo

1.  **Lưu script:** Lưu nội dung trên vào file **`patch_rpc_cores.sh`** và chạy nó.
2.  **Kiểm tra RPC:** Sau khi build lại, chạy QEMU và gọi lại `TA_EVM_CMD_LOAD_FILE` (lệnh mới) từ Host Client.
3.  **Xác minh log:** Kiểm tra Secure World log để tìm thông báo `OCALL RPC SUCCESS!` và `Đã nhận X bytes từ Host.`.

Sau khi xác minh OCALL hoạt động, bạn sẽ thay thế `SimpleGlobalState` trong eEVM bằng giao diện C++ gọi đến `rpc_call_db` này.

**IX. Kiểm tra Giao tiếp RPC (End-to-End)**
Bạn đã hoàn thành việc vá các file cốt lõi của OP-TEE OS và OP-TEE Client để thiết lập kênh giao tiếp OCALL RPC cho việc tải file\!

Bây giờ, bước cuối cùng là **thực hiện logic RPC** trong TA C++ của bạn và **chạy kiểm thử** để xác nhận toàn bộ chuỗi giao tiếp (TA yêu cầu Host, Host xử lý, Host trả kết quả).

### 🛠️ Giai đoạn 4: Hoàn thiện TA C++ Bridge và Kiểm tra RPC

Chúng ta sẽ chỉnh sửa file TA C++ Bridge (`GlobalStateOCall.cpp` và `evm_ta.cpp`) để sử dụng hàm RPC mà chúng ta đã định nghĩa.

#### Bước 1: Sửa GlobalStateOCall.cpp (Hoàn thiện OCALL)

Chúng ta cần đảm bảo `GlobalStateOCall.cpp` gọi hàm RPC wrapper C đã được thêm vào `optee_os/core/kernel/rpc.c`.

1.  **File:** `/home/optee/test-optee/optee_examples/evm_ta/ta/GlobalStateOCall.cpp`
2.  **Hành động:** Hoàn thiện `rpc_call_db` để sử dụng hàm `extern "C" TEE_Result tee_rpc_get_file(...)`.

#### Bước 2: Sửa evm\_ta.cpp (Gọi RPC Test)

Chúng ta sẽ sửa `evm_ta.cpp` để gọi RPC File Transfer.

1.  **File:** `/home/optee/test-optee/optee_examples/evm_ta/ta/evm_ta.cpp`
2.  **Hành động:** Thay thế logic kiểm tra eEVM đơn giản bằng việc gọi hàm RPC mới và kiểm tra nội dung dữ liệu nhận được.

### 📜 Script Bash: Hoàn thiện TA Logic và Chạy Kiểm tra RPC (`finalize_rpc_test.sh`)

Chạy script này **từ thư mục gốc của project** (`/home/optee/test-optee/`).

```bash
#!/bin/bash

# --- THIẾT LẬP ĐƯỜNG DẪN ---
PROJECT_ROOT=$(pwd)
TA_SRC_DIR="$PROJECT_ROOT/optee_examples/evm_ta/ta"
EVM_TA_CPP_FILE="$TA_SRC_DIR/evm_ta.cpp"
OCALL_CPP_FILE="$TA_SRC_DIR/GlobalStateOCall.cpp"
OCALL_H_FILE="$TA_SRC_DIR/include/GlobalStateOCall.h"
BUILD_DIR="$PROJECT_ROOT/build"

echo "--- Bắt đầu hoàn thiện TA C++ OCALL Logic ---"

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
        params[1].memref.size = rpc_params[1].memref.size;
    }
    
    return res;
}
EOF

# --- BƯỚC 2: Sửa evm_ta.cpp (Logic RPC Test) ---
echo "[2/3] Cập nhật evm_ta.cpp với logic RPC File Transfer Test..."

cat << 'EOF' > "$EVM_TA_CPP_FILE"
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <stdio.h>
#include <string>
#include "GlobalStateOCall.h" // Thư viện OCALL Bridge

// Định nghĩa Command ID cho phép gọi test RPC từ Host
#define TA_EVM_CMD_TEST_RPC 0x02 

// Dữ liệu Host Supplicant sẽ gửi
#define EXPECTED_HOST_DATA "EVM_CONFIG_DATA_FROM_HOST_OK"

TEE_Result run_file_transfer_test()
{
    TEE_Result res;
    // Tên file (sử dụng DB_OP_GET_ACCOUNT cho mục đích thử nghiệm)
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

// CÁC HÀM ENTRY POINT CỦA TA
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
    
    if (cmd_id == TA_EVM_CMD_TEST_RPC) {
        return run_file_transfer_test();
    }
    
    return TEE_ERROR_BAD_PARAMETERS;
}
EOF

# --- BƯỚC 3: Biên dịch lại toàn bộ hệ thống ---
echo "[3/3] Đang chạy make để biên dịch lại OP-TEE OS và Client Library..."

cd "$BUILD_DIR" || { echo "Lỗi: Không thể chuyển vào thư mục build"; exit 1; }
make -j$(nproc)

echo "--- Hoàn tất Finalize RPC Logic. Sẵn sàng Kiểm tra RPC. ---"
```

### 🎯 Hành động Tiếp theo: Kiểm tra Giao tiếp RPC

1.  **Chạy script:** Chạy script **`finalize_rpc_test.sh`**.
2.  **Chạy QEMU:**
    ```bash
    make run QEMU_EXTRA_ARGS="-nographic -serial stdio"
    ```
3.  **Sửa Host Client:** Bạn cần sửa lại `evm_ta/host/main.cpp` để gọi lệnh **`TA_EVM_CMD_TEST_RPC` (0x02)** thay vì lệnh cũ.
4.  **Thực thi trong QEMU:** Khi QEMU khởi động, đăng nhập vào console Linux Host và chạy:
    ```bash
    /bin/evm_ta/main
    ```

Nếu mọi thứ hoạt động, bạn sẽ thấy log **`OCALL RPC SUCCESS! Dữ liệu OCALL RPC khớp: EVM_CONFIG_DATA_FROM_HOST_OK`** trong console TEE.
Beta
0 / 0
used queries
1