#pragma once
#include "shared_ring_buffer.hpp"
#include <string_view>
#include <cstring>

class SecureRingBufferProducer {
private:
    RingBufferControl* ctrl;
    std::byte* data_ptr;

    // Hàm Push nội bộ để gửi các lệnh điều khiển
    bool PushInternal(PacketType type, uint32_t fd, uint32_t val) {
        const uint32_t packet_full_sz = sizeof(RingBufferPacket);
        
        // Đọc tail vào stack để chống TOCTOU
        uint32_t current_tail = ctrl->tail.load(std::memory_order_acquire);
        uint32_t current_head = ctrl->head.load(std::memory_order_relaxed);

        // Kiểm tra xem có cần Wrap-around không?
        if (current_head + packet_full_sz > ctrl->buffer_size) {
            // Không đủ chỗ ở cuối, kiểm tra xem đầu buffer có trống không
            if (packet_full_sz >= current_tail) return false;

            // Ghi Wrap Marker ở vị trí hiện tại
            auto* marker = reinterpret_cast<RingBufferPacket*>(data_ptr + current_head);
            marker->type = PacketType::WRAP_MARKER;
            
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

        // Đảm bảo dữ liệu ghi xong trước khi update head (Memory Barrier)
        uint32_t next_head = current_head + packet_full_sz;
        ctrl->head.store(next_head, std::memory_order_release);
        
        return true;
    }

public:
    explicit SecureRingBufferProducer(void* shared_mem) {
        ctrl = static_cast<RingBufferControl*>(shared_mem);
        data_ptr = static_cast<std::byte*>(shared_mem) + sizeof(RingBufferControl);
    }

    [[nodiscard]] bool Push(uint32_t fd, std::string_view encrypted_data) {
        const uint32_t payload_sz = static_cast<uint32_t>(encrypted_data.size());
        const uint32_t packet_full_sz = sizeof(RingBufferPacket) + payload_sz;
        
        // Đọc tail vào stack để chống TOCTOU
        uint32_t current_tail = ctrl->tail.load(std::memory_order_acquire);
        uint32_t current_head = ctrl->head.load(std::memory_order_relaxed);

        // Kiểm tra xem có cần Wrap-around không?
        if (current_head + packet_full_sz > ctrl->buffer_size) {
            // Không đủ chỗ ở cuối, kiểm tra xem đầu buffer có trống không
            if (packet_full_sz >= current_tail) return false; // Buffer thực sự đầy

            // Ghi Wrap Marker ở vị trí hiện tại
            auto* marker = reinterpret_cast<RingBufferPacket*>(data_ptr + current_head);
            marker->type = PacketType::WRAP_MARKER;
            
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

        // Đảm bảo dữ liệu ghi xong trước khi update head (Memory Barrier)
        uint32_t next_head = current_head + packet_full_sz;
        ctrl->head.store(next_head, std::memory_order_release);
        
        return true;
    }

    [[nodiscard]] bool Flush() {
        // 1. Lấy một ID duy nhất cho đợt Flush này
        uint32_t current_sync_id = ctrl->sync_counter.fetch_add(1, std::memory_order_relaxed) + 1;

        // 2. Gửi gói tin SYNC_FLUSH vào Ring Buffer
        if (!PushInternal(PacketType::SYNC_FLUSH, 0, current_sync_id)) {
            return false; 
        }

        // 3. Đợi CA xác nhận (Blocking Wait)
        // Trong môi trường TEE, ta không nên loop vô tận mà nên có timeout
        uint32_t attempts = 0;
        while (ctrl->last_flushed_id.load(std::memory_order_acquire) < current_sync_id) {
            if (++attempts > 1000000) return false; // Timeout sau ~1 giây
            
            #if defined(__aarch64__)
                asm volatile("yield" ::: "memory");
            #endif
        }

        return true;
    }

    // Lấy số byte còn trống trong buffer
    uint32_t GetAvailableSpace() const {
        uint32_t h = ctrl->head.load(std::memory_order_relaxed);
        uint32_t t = ctrl->tail.load(std::memory_order_acquire);
        
        if (h >= t) {
            return ctrl->buffer_size - (h - t);
        } else {
            return t - h;
        }
    }
};
