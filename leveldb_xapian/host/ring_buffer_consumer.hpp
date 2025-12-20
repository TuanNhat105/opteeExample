#pragma once
#include "../include/shared_ring_buffer.hpp"
#include <thread>
#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <map>
#include <string>
#include <vector>
#include <cstring>

class RingBufferConsumer {
private:
    RingBufferControl* ctrl;
    std::byte* data_ptr;
    std::atomic<bool> running{true};
    std::thread worker_thread;
    
    // Quản lý các file descriptors
    std::map<uint32_t, int> fd_map; // virtual_fd -> real_fd
    std::string base_path;

public:
    explicit RingBufferConsumer(void* shared_mem, const std::string& data_path) 
        : base_path(data_path) {
        ctrl = static_cast<RingBufferControl*>(shared_mem);
        data_ptr = reinterpret_cast<std::byte*>(shared_mem) + sizeof(RingBufferControl);
        
        // Tạo thư mục nếu chưa tồn tại
        mkdir(base_path.c_str(), 0755);
    }

    ~RingBufferConsumer() {
        Stop();
        // Đóng tất cả file descriptors
        for (auto& [vfd, rfd] : fd_map) {
            if (rfd >= 0) {
                close(rfd);
            }
        }
    }

    void Start() {
        worker_thread = std::thread(&RingBufferConsumer::WorkerLoop, this);
    }

    void Stop() {
        running.store(false, std::memory_order_release);
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

    // Đăng ký một virtual file descriptor với tên file thực
    bool RegisterFile(uint32_t virtual_fd, const std::string& filename, int flags = O_RDWR | O_CREAT) {
        std::string full_path = base_path + "/" + filename;
        int fd = open(full_path.c_str(), flags, 0644);
        if (fd < 0) {
            return false;
        }
        fd_map[virtual_fd] = fd;
        return true;
    }

private:
    void WorkerLoop() {
        while (running.load(std::memory_order_acquire)) {
            uint32_t h = ctrl->head.load(std::memory_order_acquire);
            uint32_t t = ctrl->tail.load(std::memory_order_relaxed);

            if (h != t) {
                auto* pkt = reinterpret_cast<RingBufferPacket*>(data_ptr + t);

                if (pkt->type == PacketType::WRAP_MARKER) {
                    // Quay về đầu buffer
                    ctrl->tail.store(0, std::memory_order_release);
                    continue;
                }

                if (pkt->type == PacketType::SYNC_FLUSH) {
                    uint32_t sync_id_to_confirm = pkt->payload_sz;

                    // Flush tất cả các file descriptors
                    for (auto& [vfd, rfd] : fd_map) {
                        if (rfd >= 0) {
                            fsync(rfd);
                        }
                    }

                    // Xác nhận với TA
                    ctrl->last_flushed_id.store(sync_id_to_confirm, std::memory_order_release);
                    
                    // Cập nhật tail
                    ctrl->tail.store(t + sizeof(RingBufferPacket), std::memory_order_release);
                    continue;
                }

                if (pkt->type == PacketType::DATA) {
                    // Xử lý ghi dữ liệu
                    auto it = fd_map.find(pkt->fd_id);
                    if (it != fd_map.end() && it->second >= 0) {
                        const std::byte* payload = data_ptr + t + sizeof(RingBufferPacket);
                        ssize_t written = write(it->second, payload, pkt->payload_sz);
                        
                        if (written < 0) {
                            // TODO: Xử lý lỗi ghi
                        }
                    }
                    
                    uint32_t processed_sz = sizeof(RingBufferPacket) + pkt->payload_sz;
                    ctrl->tail.store(t + processed_sz, std::memory_order_release);
                }
            } else {
                // Không có dữ liệu: Yield CPU
                #if defined(__aarch64__)
                    asm volatile("yield" ::: "memory");
                #else
                    usleep(50); // 50 microseconds
                #endif
            }
        }
    }
};
