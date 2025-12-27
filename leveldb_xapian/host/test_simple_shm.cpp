// SPDX-License-Identifier: BSD-2-Clause
/*
 * LevelDB Test with Ring Buffer for Real-time Logging
 * 
 * - Uses ring buffer in shared memory for real-time logging from TA
 * - TA writes logs to ring buffer (không cần đợi command kết thúc)
 * - Host has a consumer thread that reads and displays data in real-time
 * - LevelDB operations: INIT, PUT, GET
 */

#include <iostream>
#include <cstring>
#include <cstdint>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>

/* OP-TEE TEE client API */
#include <tee_client_api.h>

/* TA UUID and commands */
#include <eevm_ta.h>
#include "simple_ring_reader.hpp"

// Global reader for real-time data reading
static std::unique_ptr<SimpleRingReader> g_ring_reader;
static std::atomic<bool> g_reader_running{false};
int main()
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_EEVM_UUID;
    uint32_t err_origin;
    
    std::cout << "========================================" << std::endl;
    std::cout << " LevelDB Test with Ring Buffer" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    /* Initialize a context connecting us to the TEE */
    res = TEEC_InitializeContext(nullptr, &ctx);
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_InitializeContext failed with code 0x" << std::hex << res << std::endl;
        return 1;
    }
    
    std::cout << "✓ TEE Context initialized" << std::endl;
    
    /* Open a session to the TA */
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                          TEEC_LOGIN_PUBLIC, nullptr, &op, &err_origin);
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_OpenSession failed with code 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::endl;
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    std::cout << "✓ Session opened to TA" << std::endl << std::endl;
    /* Allocate shared memory for ring buffer (for logging) */
    std::cout << "=== Allocating Shared Memory for Logging Ring Buffer ===" << std::endl;
    const size_t LOG_RING_BUFFER_SIZE = 64 * 1024; // 64KB for logging
    std::cout << "Logging Ring Buffer Size: " << LOG_RING_BUFFER_SIZE << " bytes" << std::endl;
    // Allocate shared memory for logging ring buffer
    TEEC_SharedMemory log_ring_shm;
    log_ring_shm.size = LOG_RING_BUFFER_SIZE;
    log_ring_shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
    
    res = TEEC_AllocateSharedMemory(&ctx, &log_ring_shm);
    if (res != TEEC_SUCCESS) {
        std::cerr << "Failed to allocate logging ring buffer shared memory: 0x" 
                  << std::hex << res << std::dec << std::endl;
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Logging ring buffer allocated at " << std::hex << log_ring_shm.buffer 
              << std::dec << std::endl;
    
    // Initialize logging ring buffer structure in Host
    struct SharedRingBuffer* log_rb = static_cast<struct SharedRingBuffer*>(log_ring_shm.buffer);
    log_rb->head = 0;
    log_rb->tail = 0;
    log_rb->size = LOG_RING_BUFFER_SIZE - sizeof(struct SharedRingBuffer);
    __sync_synchronize();
    std::cout << "✓ Logging ring buffer initialized (head=0, tail=0, size=" 
              << log_rb->size << ")" << std::endl;
    
    /* Allocate shared memory for LevelDB data ring buffer (larger) */
    std::cout << "\n=== Allocating Shared Memory for LevelDB Data Ring Buffer ===" << std::endl;
    const size_t LEVELDB_RING_BUFFER_SIZE = 4 * 1024 * 1024; // 4MB for LevelDB data
    std::cout << "LevelDB Data Ring Buffer Size: " << LEVELDB_RING_BUFFER_SIZE / 1024 << " KB" << std::endl;
    TEEC_SharedMemory leveldb_ring_shm;
    leveldb_ring_shm.size = LEVELDB_RING_BUFFER_SIZE;
    leveldb_ring_shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
    
    res = TEEC_AllocateSharedMemory(&ctx, &leveldb_ring_shm);
    if (res != TEEC_SUCCESS) {
        std::cerr << "Failed to allocate LevelDB ring buffer shared memory: 0x" 
                  << std::hex << res << std::dec << std::endl;
        TEEC_ReleaseSharedMemory(&log_ring_shm);
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ LevelDB ring buffer allocated at " << std::hex << leveldb_ring_shm.buffer 
              << std::dec << std::endl;
    
    // Initialize LevelDB ring buffer (RingBufferControl structure)
    // Note: This uses RingBufferControl, not SharedRingBuffer
    std::memset(leveldb_ring_shm.buffer, 0, leveldb_ring_shm.size);
    __sync_synchronize();
    std::cout << "✓ LevelDB ring buffer initialized" << std::endl;
    
    // Start reader thread for real-time logging (BEFORE invoking TA command)
    std::cout << "\n=== Starting Real-time Logging Ring Buffer Reader Thread ===" << std::endl;
    g_ring_reader = std::make_unique<SimpleRingReader>(log_ring_shm.buffer);
    
    // Set callback to display data (line by line)
    g_ring_reader->SetCallback([](const char* data, size_t len) {
        std::cout << "[REALTIME] ";
        std::cout.write(data, len);
        if (len > 0 && data[len-1] != '\n') {
            std::cout << std::endl; // Add newline if not present
        }
        std::cout.flush();
    });
    
    g_reader_running = true;
    g_ring_reader->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give thread time to start
    std::cout << "✓ Reader thread started - monitoring ring buffer..." << std::endl;
    
    /* Prepare log buffer */
    char log_buffer[8192] = {0};
    
    // ==================== Initialize LevelDB in TA ====================
    std::cout << "\n=== Initializing LevelDB in TA ===" << std::endl;
    
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_WHOLE,            // LevelDB data ring buffer
        TEEC_MEMREF_WHOLE,            // Logging ring buffer
        TEEC_VALUE_INPUT,             // LevelDB buffer size
        TEEC_MEMREF_TEMP_OUTPUT);    // OCALL logs
    
    // Pass LevelDB data ring buffer as param 0
    op.params[0].memref.parent = &leveldb_ring_shm;
    op.params[0].memref.offset = 0;
    op.params[0].memref.size = leveldb_ring_shm.size;
    
    // Pass logging ring buffer as param 1
    op.params[1].memref.parent = &log_ring_shm;
    op.params[1].memref.offset = 0;
    op.params[1].memref.size = log_ring_shm.size;
    
    op.params[2].value.a = LEVELDB_RING_BUFFER_SIZE;
    op.params[3].tmpref.buffer = log_buffer;
    op.params[3].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Invoking INIT_LEVELDB with LevelDB buffer size: " << LEVELDB_RING_BUFFER_SIZE << std::endl;
    std::cout.flush();
    
    // CRITICAL: Ensure ring buffer structure is properly initialized and visible to TA
    __sync_synchronize();
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_INIT_LEVELDB, &op, &err_origin);
    
    // Wait a bit for all ring buffer messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ LevelDB initialization failed: 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::dec << std::endl;
        if (log_buffer[0] != '\0') {
            std::cout << "\n--- TA Error Logs ---" << std::endl;
            std::cout << log_buffer << std::endl;
        }
        g_reader_running = false;
        if (g_ring_reader) {
            g_ring_reader->Stop();
            g_ring_reader.reset();
        }
        TEEC_ReleaseSharedMemory(&log_ring_shm);
        TEEC_ReleaseSharedMemory(&leveldb_ring_shm);
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ LevelDB initialized successfully!" << std::endl;
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    // ==================== Test PUT Operation ====================
    std::cout << "\n=== Testing PUT Operation ===" << std::endl;
    
    std::string test_key = "user:1001";
    std::string test_value = "Alice:alice@example.com:25";
    
    std::memset(log_buffer, 0, sizeof(log_buffer));
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,       // Key
        TEEC_MEMREF_TEMP_INPUT,       // Value
        TEEC_VALUE_OUTPUT,            // Status
        TEEC_MEMREF_TEMP_OUTPUT);     // Logs
    
    op.params[0].tmpref.buffer = (void*)test_key.data();
    op.params[0].tmpref.size = test_key.size();
    op.params[1].tmpref.buffer = (void*)test_value.data();
    op.params[1].tmpref.size = test_value.size();
    op.params[3].tmpref.buffer = log_buffer;
    op.params[3].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Putting: '" << test_key << "' => '" << test_value << "'" << std::endl;
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_PUT, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ PUT failed: 0x" << std::hex << res << std::dec << std::endl;
    } else if (op.params[2].value.a != 0) {
        std::cerr << "❌ PUT returned error status: " << op.params[2].value.a << std::endl;
    } else {
        std::cout << "✓ PUT successful!" << std::endl;
    }
    
    // Give reader time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // ==================== Test GET Operation ====================
    std::cout << "\n=== Testing GET Operation ===" << std::endl;
    
    char get_buffer[1024] = {0};
    std::memset(log_buffer, 0, sizeof(log_buffer));
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,       // Key
        TEEC_MEMREF_TEMP_OUTPUT,      // Value
        TEEC_VALUE_OUTPUT,            // Status
        TEEC_MEMREF_TEMP_OUTPUT);     // Logs
    
    op.params[0].tmpref.buffer = (void*)test_key.data();
    op.params[0].tmpref.size = test_key.size();
    op.params[1].tmpref.buffer = get_buffer;
    op.params[1].tmpref.size = sizeof(get_buffer);
    op.params[3].tmpref.buffer = log_buffer;
    op.params[3].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Getting: '" << test_key << "'" << std::endl;
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_GET, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ GET failed: 0x" << std::hex << res << std::dec << std::endl;
    } else {
        uint32_t status = op.params[2].value.a;
        if (status == 0) {
            size_t value_size = op.params[1].tmpref.size;
            if (value_size > 0) {
                std::string retrieved(static_cast<const char*>(op.params[1].tmpref.buffer), value_size);
                std::cout << "✓ GET successful! Value: '" << retrieved << "'" << std::endl;
                
                if (retrieved == test_value) {
                    std::cout << "✓✓ Value matches!" << std::endl;
                } else {
                    std::cerr << "❌ Value mismatch! Expected: '" << test_value 
                              << "', Got: '" << retrieved << "'" << std::endl;
                }
            } else {
                std::cerr << "❌ GET returned empty value (size=0)!" << std::endl;
            }
        } else if (status == 1) {
            std::cout << "❌ Key not found" << std::endl;
        } else {
            std::cout << "❌ GET error: " << status << std::endl;
        }
    }
    
    // ==================== Test Multiple PUTs ====================
    std::cout << "\n=== Testing Multiple PUT Operations ===" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> test_data = {
        {"user:1002", "Bob:bob@example.com:30"},
        {"user:1003", "Charlie:charlie@example.com:35"},
        {"product:001", "Laptop:Dell:999.99"},
        {"product:002", "Mouse:Logitech:29.99"},
    };
    
    for (const auto& [key, value] : test_data) {
        std::memset(&op, 0, sizeof(op));
        op.paramTypes = TEEC_PARAM_TYPES(
            TEEC_MEMREF_TEMP_INPUT,
            TEEC_MEMREF_TEMP_INPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE);
        
        op.params[0].tmpref.buffer = (void*)key.data();
        op.params[0].tmpref.size = key.size();
        op.params[1].tmpref.buffer = (void*)value.data();
        op.params[1].tmpref.size = value.size();
        
        std::cout << "PUT: '" << key << "' ... ";
        
        res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_PUT, &op, &err_origin);
        
        if (res == TEEC_SUCCESS && op.params[2].value.a == 0) {
            std::cout << "✓" << std::endl;
        } else {
            std::cout << "❌" << std::endl;
        }
    }
    
    std::cout << "\n✓ Batch PUT completed" << std::endl;
    
    // Give reader time to process remaining logs
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    /* Cleanup */
    std::cout << "\n=== Stopping Ring Buffer Reader Thread ===" << std::endl;
    g_reader_running = false;
    if (g_ring_reader) {
        g_ring_reader->Stop();
        g_ring_reader.reset();
    }
    std::cout << "✓ Reader thread stopped" << std::endl;
    
    TEEC_ReleaseSharedMemory(&log_ring_shm);
    TEEC_ReleaseSharedMemory(&leveldb_ring_shm);
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}

