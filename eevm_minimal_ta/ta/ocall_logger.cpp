#include "ocall_logger.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Buffer tĩnh 8KB lưu trữ log tập trung
#define OCALL_BUFFER_SIZE 8192
static char g_ocall_buffer[OCALL_BUFFER_SIZE];
static size_t g_ocall_pos = 0;

extern "C" {

void ocall_log_init(void) {
    g_ocall_pos = 0;
    g_ocall_buffer[0] = '\0';
}

void ocall_log_print(const char* fmt, ...) {
    char tmp[512];
    va_list args;
    va_start(args, fmt);
    
    // Tạo nội dung log với tiền tố [TA]
    int header_len = snprintf(tmp, sizeof(tmp), "[TA] ");
    int msg_len = vsnprintf(tmp + header_len, sizeof(tmp) - header_len - 2, fmt, args);
    va_end(args);

    if (msg_len > 0) {
        strcat(tmp, "\n"); // Tự động xuống dòng
        size_t total_len = strlen(tmp);
        
        // Kiểm tra tránh tràn buffer g_ocall_buffer
        if (g_ocall_pos + total_len < OCALL_BUFFER_SIZE) {
            memcpy(g_ocall_buffer + g_ocall_pos, tmp, total_len);
            g_ocall_pos += total_len;
            g_ocall_buffer[g_ocall_pos] = '\0';
        }
    }
}

void ocall_log_flush_to_params(void* dest_buffer, size_t* dest_size) {
    if (!dest_buffer || !dest_size || *dest_size == 0) return;

    size_t copy_size = g_ocall_pos;
    // Nếu log dài hơn buffer phía Host cung cấp, thực hiện cắt bớt
    if (copy_size >= *dest_size) {
        copy_size = *dest_size - 1;
    }
    
    memcpy(dest_buffer, g_ocall_buffer, copy_size);
    ((char*)dest_buffer)[copy_size] = '\0';
    *dest_size = copy_size + 1; // Cập nhật lại kích thước thực tế đã copy
}

} // extern "C"