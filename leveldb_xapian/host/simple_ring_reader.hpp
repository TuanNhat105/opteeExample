#pragma once
#include "../include/simple_shared_ring_buffer.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <cstring>
#include <functional>
#include <unistd.h>

// Simple Ring Buffer Reader for Host (Normal World)
// Reads data from ring buffer in real-time
class SimpleRingReader {
private:
    struct SharedRingBuffer* rb;
    std::atomic<bool> running{false};
    std::thread reader_thread;
    std::function<void(const char* data, size_t len)> callback;

    void ReaderLoop() {
        // Buffer to accumulate characters until newline
        char line_buffer[1024];
        size_t line_pos = 0;
        
        while (running.load(std::memory_order_acquire)) {
            // Memory barrier to see latest writes from TA
            __sync_synchronize();
            uint32_t h = rb->head;
            __sync_synchronize();
            uint32_t t = rb->tail;

            // Check if there's new data (head != tail)
            while (h != t) {
                char c = rb->data[t];
                
                // Accumulate characters until newline
                if (c == '\n' || line_pos >= sizeof(line_buffer) - 1) {
                    // End of line or buffer full - output the line
                    if (line_pos > 0) {
                        line_buffer[line_pos] = '\0';
                        
                        if (callback) {
                            callback(line_buffer, line_pos);
                        } else {
                            std::cout << line_buffer;
                            std::cout.flush();
                        }
                        line_pos = 0;
                    }
                    
                    // If it's a newline, also output it
                    if (c == '\n') {
                        if (callback) {
                            callback("\n", 1);
                        } else {
                            std::cout << '\n';
                            std::cout.flush();
                        }
                    }
                } else if (c != '\r') {
                    // Accumulate character (skip carriage return)
                    line_buffer[line_pos++] = c;
                }
                
                // Update tail and wrap around if needed
                t = (t + 1) % rb->size;
                
                // Memory barrier before updating tail
                __sync_synchronize();
                rb->tail = t;
                __sync_synchronize();
                
                // Re-check head (TA might have written more)
                __sync_synchronize();
                h = rb->head;
                __sync_synchronize();
            }
            
            // No data available, sleep a bit to avoid busy waiting
            usleep(100); // 100 microseconds
        }
        
        // Flush any remaining data in buffer
        if (line_pos > 0) {
            line_buffer[line_pos] = '\0';
            if (callback) {
                callback(line_buffer, line_pos);
            } else {
                std::cout << line_buffer << std::endl;
            }
        }
    }

public:
    explicit SimpleRingReader(void* shared_mem) {
        rb = static_cast<struct SharedRingBuffer*>(shared_mem);
    }

    ~SimpleRingReader() {
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
        reader_thread = std::thread(&SimpleRingReader::ReaderLoop, this);
    }

    void Stop() {
        if (!running.load()) {
            return;
        }
        running.store(false, std::memory_order_release);
        if (reader_thread.joinable()) {
            reader_thread.join();
        }
    }

    bool IsRunning() const {
        return running.load(std::memory_order_acquire);
    }
};

