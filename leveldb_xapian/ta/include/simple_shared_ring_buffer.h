#pragma once
#include <stdint.h>
#include <stddef.h>

// Simple Ring Buffer Structure for Real-time Data Transfer
// Shared between TA (Producer) and Host (Consumer)
// 
// CRITICAL: Must use volatile for head/tail to prevent compiler optimization
// These values can change from the other world (Normal/Secure)
#pragma pack(push, 1)
struct SharedRingBuffer {
    volatile uint32_t head;  // TA writes here (Write Index)
    volatile uint32_t tail;  // Host reads here (Read Index)
    uint32_t size;           // Size of data buffer
    uint8_t data[];          // Flexible array member - actual data buffer
};
#pragma pack(pop)

// Helper function to calculate data buffer size
static inline size_t get_data_buffer_size(size_t total_size) {
    return total_size - sizeof(SharedRingBuffer);
}

