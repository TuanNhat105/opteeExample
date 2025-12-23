#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// Simple Ring Buffer for Real-time Logging
// TA (Secure World) writes logs, Host (Normal World) reads and displays

#pragma pack(push, 1)
struct LogRingBufferControl {
    alignas(64) uint32_t write_pos;   // TA writes here
    alignas(64) uint32_t read_pos;     // Host reads here
    alignas(64) uint32_t buffer_size;  // Size of data buffer
    alignas(64) uint32_t flags;        // Control flags (0=normal, 1=TA active)
    uint8_t padding[64 - (4 * sizeof(uint32_t) % 64)];
};
#pragma pack(pop)

// Log entry structure
struct LogEntry {
    uint32_t timestamp_ms;  // Timestamp in milliseconds
    uint32_t log_len;       // Length of log message
    char message[0];        // Variable length message (null-terminated)
};

// Helper functions for cache management (OP-TEE safe)
static inline void cache_clean_range(void* addr, size_t len) {
    #if defined(__aarch64__)
        char* ptr = static_cast<char*>(addr);
        char* end = ptr + len;
        for (; ptr < end; ptr += 64) {
            __asm__ volatile("dc cvac, %0" : : "r"(ptr) : "memory");
        }
        __asm__ volatile("dsb ish" ::: "memory");
    #elif defined(__arm__)
        char* ptr = static_cast<char*>(addr);
        char* end = ptr + len;
        for (; ptr < end; ptr += 32) {
            __asm__ volatile("mcr p15, 0, %0, c7, c10, 1" : : "r"(ptr) : "memory");
        }
        __asm__ volatile("dsb" ::: "memory");
    #endif
}

static inline void cache_invalidate_range(void* addr, size_t len) {
    #if defined(__aarch64__)
        char* ptr = static_cast<char*>(addr);
        char* end = ptr + len;
        for (; ptr < end; ptr += 64) {
            __asm__ volatile("dc ivac, %0" : : "r"(ptr) : "memory");
        }
        __asm__ volatile("dsb ish" ::: "memory");
    #elif defined(__arm__)
        char* ptr = static_cast<char*>(addr);
        char* end = ptr + len;
        for (; ptr < end; ptr += 32) {
            __asm__ volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(ptr) : "memory");
        }
        __asm__ volatile("dsb" ::: "memory");
    #endif
}

static inline void memory_barrier() {
    __asm__ volatile("dmb ish" ::: "memory");
}

// Producer side (TA - Secure World)
class LogRingBufferProducer {
private:
    LogRingBufferControl* ctrl;
    uint8_t* data_buffer;
    void* shared_mem_base;
    size_t total_size;

    uint32_t GetTimestamp() {
        // Simple timestamp - milliseconds since some reference
        // In real implementation, use TEE_GetSystemTime
        static uint32_t counter = 0;
        return ++counter;
    }

public:
    explicit LogRingBufferProducer(void* shared_mem, size_t total_size_bytes) {
        ctrl = static_cast<LogRingBufferControl*>(shared_mem);
        data_buffer = reinterpret_cast<uint8_t*>(shared_mem) + sizeof(LogRingBufferControl);
        shared_mem_base = shared_mem;
        total_size = total_size_bytes;
        
        // Initialize if not already initialized
        if (ctrl->buffer_size == 0) {
            ctrl->write_pos = 0;
            ctrl->read_pos = 0;
            ctrl->buffer_size = total_size_bytes - sizeof(LogRingBufferControl);
            ctrl->flags = 1; // Mark as active
            memory_barrier();
            cache_clean_range(shared_mem_base, total_size);
        }
    }

    // Write a log message to the ring buffer
    bool WriteLog(const char* message) {
        if (!message || !ctrl) return false;
        
        size_t msg_len = strlen(message);
        size_t entry_size = sizeof(LogEntry) + msg_len + 1; // +1 for null terminator
        
        // Check if we have space
        memory_barrier();
        uint32_t current_write = ctrl->write_pos;
        uint32_t current_read = ctrl->read_pos;
        memory_barrier();
        
        // Calculate available space
        uint32_t available;
        if (current_write >= current_read) {
            available = ctrl->buffer_size - (current_write - current_read);
        } else {
            available = current_read - current_write;
        }
        
        // Need at least entry_size + some margin
        if (available < entry_size + 64) {
            return false; // Buffer too full
        }
        
        // Handle wrap-around
        if (current_write + entry_size > ctrl->buffer_size) {
            // Wrap to beginning
            current_write = 0;
        }
        
        // Write log entry
        LogEntry* entry = reinterpret_cast<LogEntry*>(data_buffer + current_write);
        entry->timestamp_ms = GetTimestamp();
        entry->log_len = static_cast<uint32_t>(msg_len);
        memcpy(entry->message, message, msg_len);
        entry->message[msg_len] = '\0';
        
        // Update write position
        memory_barrier();
        ctrl->write_pos = current_write + static_cast<uint32_t>(entry_size);
        memory_barrier();
        
        // Flush cache so host can see it
        cache_clean_range(shared_mem_base, total_size);
        
        return true;
    }
    
    void SetActive(bool active) {
        memory_barrier();
        ctrl->flags = active ? 1 : 0;
        memory_barrier();
        cache_clean_range(&ctrl->flags, sizeof(uint32_t));
    }
};

// Consumer side (Host - Normal World)
class LogRingBufferConsumer {
private:
    LogRingBufferControl* ctrl;
    uint8_t* data_buffer;
    void* shared_mem_base;
    size_t total_size;

    // Host-side cache operations (Linux)
    void host_cache_invalidate_range(void* addr, size_t len) {
        // On Linux host, we use __sync_synchronize for memory ordering
        // Cache coherence is handled by the kernel for shared memory
        (void)addr; (void)len; // Suppress unused warnings
        __sync_synchronize();
    }

public:
    explicit LogRingBufferConsumer(void* shared_mem, size_t total_size_bytes) {
        ctrl = static_cast<LogRingBufferControl*>(shared_mem);
        data_buffer = reinterpret_cast<uint8_t*>(shared_mem) + sizeof(LogRingBufferControl);
        shared_mem_base = shared_mem;
        total_size = total_size_bytes;
    }

    // Read and process all available log entries
    // Returns number of entries processed
    size_t ReadLogs(void (*log_callback)(const char* msg, uint32_t timestamp)) {
        if (!ctrl || !log_callback) return 0;
        
        // Invalidate cache to see TA's writes (host side)
        host_cache_invalidate_range(shared_mem_base, total_size);
        __sync_synchronize();
        
        uint32_t current_read = ctrl->read_pos;
        uint32_t current_write = ctrl->write_pos;
        __sync_synchronize();
        
        size_t processed = 0;
        
        while (current_read != current_write) {
            // Check if we need to wrap
            if (current_read + sizeof(LogEntry) > ctrl->buffer_size) {
                current_read = 0;
                __sync_synchronize();
                ctrl->read_pos = current_read;
                __sync_synchronize();
                continue;
            }
            
            LogEntry* entry = reinterpret_cast<LogEntry*>(data_buffer + current_read);
            
            // Validate entry
            if (entry->log_len == 0 || entry->log_len > 4096) {
                // Invalid entry, skip
                break;
            }
            
            size_t entry_size = sizeof(LogEntry) + entry->log_len + 1;
            
            // Check bounds
            if (current_read + entry_size > ctrl->buffer_size) {
                // Wrap to beginning
                current_read = 0;
                __sync_synchronize();
                ctrl->read_pos = current_read;
                __sync_synchronize();
                continue;
            }
            
            // Process log entry
            if (entry->message[entry->log_len] == '\0') {
                log_callback(entry->message, entry->timestamp_ms);
                processed++;
            }
            
            // Update read position
            current_read += static_cast<uint32_t>(entry_size);
            __sync_synchronize();
            ctrl->read_pos = current_read;
            __sync_synchronize();
            
            // Re-check write position
            host_cache_invalidate_range(shared_mem_base, total_size);
            __sync_synchronize();
            current_write = ctrl->write_pos;
        }
        
        return processed;
    }
    
    bool IsActive() const {
        __sync_synchronize();
        uint32_t flags = ctrl->flags;
        __sync_synchronize();
        return (flags & 1) != 0;
    }
};

