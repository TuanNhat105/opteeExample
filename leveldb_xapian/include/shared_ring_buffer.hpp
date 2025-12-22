#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <type_traits>

// Định nghĩa các loại gói tin trong Buffer
enum class PacketType : uint8_t {
    DATA = 0,
    WRAP_MARKER = 1, // Đánh dấu phải quay lại đầu buffer
    SYNC_FLUSH = 2   // Lệnh yêu cầu CA ghi đĩa ngay lập tức
};

#pragma pack(push, 1)
struct RingBufferPacket {
    PacketType type;
    uint32_t fd_id;
    uint32_t payload_sz;
    // Dữ liệu sẽ bắt đầu ngay sau header này
};

// Use std::atomic with proper memory ordering for thread safety
struct alignas(64) RingBufferControl {
    // Đặt head và tail trên các Cache Line khác nhau (64 bytes)
    alignas(64) std::atomic<uint32_t> head;
    alignas(64) std::atomic<uint32_t> tail;
    
    // CA sẽ cập nhật giá trị này sau khi gọi fsync() trên Linux thành công
    alignas(64) std::atomic<uint32_t> last_flushed_id;
    
    // Biến đếm để định danh các yêu cầu Flush
    std::atomic<uint32_t> sync_counter;

    uint32_t buffer_size;

    static constexpr size_t CACHE_LINE = 64;
    uint8_t padding[CACHE_LINE - sizeof(uint32_t)]; 
};
#pragma pack(pop)

// Kiểm tra an toàn tại thời điểm biên dịch
static_assert(std::is_standard_layout_v<RingBufferControl>, "RingBufferControl must be standard layout");
static_assert(sizeof(RingBufferPacket) == 9, "RingBufferPacket size must be 9 bytes");

// Định nghĩa kích thước mặc định của Ring Buffer (4MB)
constexpr size_t DEFAULT_RING_BUFFER_SIZE = 4 * 1024 * 1024; // 4MB - keep within TEE memory limits
