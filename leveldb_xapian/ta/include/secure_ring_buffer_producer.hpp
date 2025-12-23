#pragma once
#include "shared_ring_buffer.hpp"
#include <string_view>
#include <cstring>
#include <tee_api.h>

// OP-TEE Safe Cache Management Helpers
// ARM64 cache operations for shared memory between Secure and Normal world
static inline void cache_clean_range(void* addr, size_t len) {
    #if defined(__aarch64__)
        char* ptr = static_cast<char*>(addr);
        char* end = ptr + len;
        for (; ptr < end; ptr += 64) {
            __asm__ volatile("dc cvac, %0" : : "r"(ptr) : "memory");
        }
        __asm__ volatile("dsb ish" ::: "memory");
    #elif defined(__arm__)
        // ARM32: use CP15 cache operations
        char* ptr = static_cast<char*>(addr);
        char* end = ptr + len;
        for (; ptr < end; ptr += 32) {
            __asm__ volatile("mcr p15, 0, %0, c7, c10, 1" : : "r"(ptr) : "memory");
        }
        __asm__ volatile("dsb" ::: "memory");
    #endif
}

static inline void cache_clean_invalidate_range(void* addr, size_t len) {
    #if defined(__aarch64__)
        char* ptr = static_cast<char*>(addr);
        char* end = ptr + len;
        for (; ptr < end; ptr += 64) {
            __asm__ volatile("dc civac, %0" : : "r"(ptr) : "memory");
        }
        __asm__ volatile("dsb ish" ::: "memory");
    #elif defined(__arm__)
        char* ptr = static_cast<char*>(addr);
        char* end = ptr + len;
        for (; ptr < end; ptr += 32) {
            __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" : : "r"(ptr) : "memory");
        }
        __asm__ volatile("dsb" ::: "memory");
    #endif
}

// Memory barrier for ordering
static inline void memory_barrier() {
    __asm__ volatile("dmb ish" ::: "memory");
}

class SecureRingBufferProducer {
private:
    RingBufferControl* ctrl;
    std::byte* data_ptr;
    void* shared_mem_base;  // Store base address for cache operations
    size_t total_shared_size;  // Total size for cache flush

    // Hàm Push nội bộ để gửi các lệnh điều khiển
    bool PushInternal(PacketType type, uint32_t fd, uint32_t val) {
        const uint32_t packet_full_sz = sizeof(RingBufferPacket);
        
        // Đọc tail vào stack để chống TOCTOU (OP-TEE safe: plain read + barrier)
        memory_barrier();  // Ensure we see latest writes from CA
        uint32_t current_tail = ctrl->tail;
        memory_barrier();
        uint32_t current_head = ctrl->head;

        // Kiểm tra xem có cần Wrap-around không?
        if (current_head + packet_full_sz > ctrl->buffer_size) {
            // Không đủ chỗ ở cuối, kiểm tra xem đầu buffer có trống không
            if (packet_full_sz >= current_tail) return false;

            // Ghi Wrap Marker ở vị trí hiện tại
            auto* marker = reinterpret_cast<RingBufferPacket*>(data_ptr + current_head);
            marker->type = PacketType::WRAP_MARKER;
            
            // Memory barrier: ensure marker is written before head update
            memory_barrier();
            
            // Nhảy về đầu
            current_head = 0;
        }

        // Kiểm tra đè vào tail (tràn buffer)
        if (current_head < current_tail && (current_head + packet_full_sz) >= current_tail) {
            return false; 
        }

        // Thực hiện ghi dữ liệu
        auto* pkt = reinterpret_cast<RingBufferPacket*>(data_ptr + current_head);
        pkt->type = type;
        pkt->fd_id = fd;
        pkt->payload_sz = val;

        // Memory barrier: ensure packet is written before head update
        memory_barrier();
        
        // Update head (OP-TEE safe: plain write)
        uint32_t next_head = current_head + packet_full_sz;
        ctrl->head = next_head;
        
        // Cache flush: ensure CA can see the writes
        cache_clean_range(shared_mem_base, total_shared_size);
        
        return true;
    }

public:
    explicit SecureRingBufferProducer(void* shared_mem, size_t total_size = 0) {
        ctrl = static_cast<RingBufferControl*>(shared_mem);
        data_ptr = static_cast<std::byte*>(shared_mem) + sizeof(RingBufferControl);
        shared_mem_base = shared_mem;
        total_shared_size = total_size > 0 ? total_size : (sizeof(RingBufferControl) + ctrl->buffer_size);
    }

    [[nodiscard]] bool Push(uint32_t fd, std::string_view encrypted_data) {
        const uint32_t payload_sz = static_cast<uint32_t>(encrypted_data.size());
        const uint32_t packet_full_sz = sizeof(RingBufferPacket) + payload_sz;
        
        // Đọc tail vào stack để chống TOCTOU (OP-TEE safe: plain read + barrier)
        memory_barrier();  // Ensure we see latest writes from CA
        uint32_t current_tail = ctrl->tail;
        memory_barrier();
        uint32_t current_head = ctrl->head;

        // Kiểm tra xem có cần Wrap-around không?
        if (current_head + packet_full_sz > ctrl->buffer_size) {
            // Không đủ chỗ ở cuối, kiểm tra xem đầu buffer có trống không
            if (packet_full_sz >= current_tail) return false; // Buffer thực sự đầy

            // Ghi Wrap Marker ở vị trí hiện tại
            auto* marker = reinterpret_cast<RingBufferPacket*>(data_ptr + current_head);
            marker->type = PacketType::WRAP_MARKER;
            
            // Memory barrier: ensure marker is written before head update
            memory_barrier();
            
            // Nhảy về đầu
            current_head = 0;
        }

        // Kiểm tra đè vào tail (tràn buffer)
        // Khoảng cách giữa head và tail phải đủ cho packet
        if (current_head < current_tail && (current_head + packet_full_sz) >= current_tail) {
            return false; 
        }

        // Thực hiện ghi dữ liệu
        auto* pkt = reinterpret_cast<RingBufferPacket*>(data_ptr + current_head);
        pkt->type = PacketType::DATA;
        pkt->fd_id = fd;
        pkt->payload_sz = payload_sz;
        
        std::memcpy(data_ptr + current_head + sizeof(RingBufferPacket), 
                    encrypted_data.data(), payload_sz);

        // Memory barrier: ensure data is written before head update
        memory_barrier();
        
        // Update head (OP-TEE safe: plain write)
        uint32_t next_head = current_head + packet_full_sz;
        ctrl->head = next_head;
        
        // Cache flush: ensure CA can see the writes
        cache_clean_range(shared_mem_base, total_shared_size);
        
        return true;
    }

    [[nodiscard]] bool Flush() {
        // 1. Lấy một ID duy nhất cho đợt Flush này (OP-TEE safe: plain read-modify-write)
        memory_barrier();
        uint32_t current_sync_id = ctrl->sync_counter + 1;
        ctrl->sync_counter = current_sync_id;
        memory_barrier();
        cache_clean_range(&ctrl->sync_counter, sizeof(uint32_t));

        // 2. Gửi gói tin SYNC_FLUSH vào Ring Buffer
        if (!PushInternal(PacketType::SYNC_FLUSH, 0, current_sync_id)) {
            return false; 
        }

        // 3. Đợi CA xác nhận (Blocking Wait với timeout ngắn)
        // ⚠️ QUAN TRỌNG: Giảm timeout để tránh hang trong quá trình init
        // Consumer có thể chưa sẵn sàng ngay lập tức
        uint32_t attempts = 0;
        const uint32_t MAX_ATTEMPTS = 100000; // Giảm từ 1M xuống 100K (~100ms)
        
        while (true) {
            // Invalidate cache before reading CA's write
            cache_clean_invalidate_range(&ctrl->last_flushed_id, sizeof(uint32_t));
            memory_barrier();
            uint32_t last_flushed = ctrl->last_flushed_id;
            
            if (last_flushed >= current_sync_id) {
                return true; // Success
            }
            
            if (++attempts >= MAX_ATTEMPTS) {
                // Timeout - consumer may not be ready or too slow
                // Caller should handle this appropriately
                return false;
            }
            
            #if defined(__aarch64__)
                asm volatile("yield" ::: "memory");
            #endif
        }

        return true;
    }

    // Lấy số byte còn trống trong buffer
    uint32_t GetAvailableSpace() const {
        // Invalidate cache before reading CA's writes
        cache_clean_invalidate_range(shared_mem_base, total_shared_size);
        memory_barrier();
        
        uint32_t h = ctrl->head;
        uint32_t t = ctrl->tail;
        
        if (h >= t) {
            return ctrl->buffer_size - (h - t);
        } else {
            return t - h;
        }
    }
};
