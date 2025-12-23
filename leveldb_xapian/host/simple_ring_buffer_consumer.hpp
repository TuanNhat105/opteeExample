0    #pragma once
#include "../include/shared_ring_buffer.hpp"
#include <thread>
#include <atomic>
#include <iostream>
#include <cstring>
#include <functional>
#include <unistd.h>

// Simple Ring Buffer Consumer for real-time log display
// Reads data from ring buffer and calls callback function
class SimpleRingBufferConsumer {
private:
    RingBufferControl* ctrl;
    std::byte* data_ptr;
    std::atomic<bool> running{false};
    std::thread worker_thread;
    std::function<void(const char* data, size_t len)> callback;

    void WorkerLoop() {
        while (running.load(std::memory_order_acquire)) {
            // Memory barrier to ensure we see latest writes from TA
            __sync_synchronize();
            uint32_t h = ctrl->head;
            __sync_synchronize();
            uint32_t t = ctrl->tail;

            if (h != t) {
                auto* pkt = reinterpret_cast<RingBufferPacket*>(data_ptr + t);

                if (pkt->type == PacketType::WRAP_MARKER) {
                    // Wrap around to beginning
                    __sync_synchronize();
                    ctrl->tail = 0;
                    __sync_synchronize();
                    continue;
                }

                if (pkt->type == PacketType::SYNC_FLUSH) {
                    // Handle sync flush - just update tail
                    uint32_t processed_sz = sizeof(RingBufferPacket);
                    __sync_synchronize();
                    ctrl->tail = t + processed_sz;
                    __sync_synchronize();
                    continue;
                }

                if (pkt->type == PacketType::DATA) {
                    // Process data packet
                    const std::byte* payload = data_ptr + t + sizeof(RingBufferPacket);
                    
                    if (callback) {
                        // Call callback with payload data
                        const char* payload_str = reinterpret_cast<const char*>(payload);
                        callback(payload_str, pkt->payload_sz);
                    } else {
                        // Default: print to stdout
                        std::cout.write(reinterpret_cast<const char*>(payload), pkt->payload_sz);
                        std::cout << std::endl;
                    }
                    
                    uint32_t processed_sz = sizeof(RingBufferPacket) + pkt->payload_sz;
                    __sync_synchronize();
                    ctrl->tail = t + processed_sz;
                    __sync_synchronize();
                }
            } else {
                // No data available, yield CPU
                #if defined(__aarch64__)
                    asm volatile("yield" ::: "memory");
                #else
                    usleep(10); // 10 microseconds
                #endif
            }
        }
    }

public:
    explicit SimpleRingBufferConsumer(void* shared_mem) {
        ctrl = static_cast<RingBufferControl*>(shared_mem);
        data_ptr = reinterpret_cast<std::byte*>(shared_mem) + sizeof(RingBufferControl);
    }

    ~SimpleRingBufferConsumer() {
        Stop();
    }

    void SetCallback(std::function<void(const char* data, size_t len)> cb) {
        callback = cb;
    }

    void Start() {
        if (running.load()) {
            return; // Already running
        }
        running.store(true, std::memory_order_release);
        worker_thread = std::thread(&SimpleRingBufferConsumer::WorkerLoop, this);
    }

    void Stop() {
        if (!running.load()) {
            return;
        }
        running.store(false, std::memory_order_release);
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

    bool IsRunning() const {
        return running.load(std::memory_order_acquire);
    }
};

