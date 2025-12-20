#ifndef OCALL_LOGGER_H
#define OCALL_LOGGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Khởi tạo hoặc xóa sạch log buffer (thường gọi ở đầu mỗi Command)
void ocall_log_init(void);

// Hàm ghi log vào buffer tĩnh (hàm nội bộ cho Macro)
void ocall_log_print(const char* fmt, ...);

// Copy dữ liệu từ buffer nội bộ sang buffer trả về cho Host (Normal World)
void ocall_log_flush_to_params(void* dest_buffer, size_t* dest_size);

#ifdef __cplusplus
}
#endif

// Macro dùng để log: OCALL_LOG("Giá trị: %d", var);
#define OCALL_LOG(fmt, ...) ocall_log_print(fmt, ##__VA_ARGS__)

#endif // OCALL_LOGGER_H