// SPDX-License-Identifier: BSD-2-Clause
/*
 * Simple test for TEEC_InvokeCommand with Shared Memory
 * NO LevelDB - just test basic shared memory communication
 * 
 * NEW: Real-time data transfer via Ring Buffer in Shared Memory
 * - TA writes data to ring buffer in shared memory (không cần đợi command kết thúc)
 * - Host has a consumer thread that reads and displays data in real-time
 */

#include <iostream>
#include <cstring>
#include <cstdint>
#include <thread>
#include <atomic>
#include <chrono>

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
    std::cout << " Simple Shared Memory Test" << std::endl;
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
    
    /* Allocate shared memory for ring buffer (large enough to be detected as ring buffer) */
    std::cout << "=== Allocating Shared Memory for Ring Buffer ===" << std::endl;
    
    const size_t RING_BUFFER_SIZE = 64 * 1024; // 64KB for ring buffer (must be > 4KB)
    
    std::cout << "Ring Buffer Size: " << RING_BUFFER_SIZE << " bytes" << std::endl;
    
    // Allocate shared memory for ring buffer
    TEEC_SharedMemory ring_shm;
    ring_shm.size = RING_BUFFER_SIZE;
    ring_shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
    
    res = TEEC_AllocateSharedMemory(&ctx, &ring_shm);
    if (res != TEEC_SUCCESS) {
        std::cerr << "Failed to allocate ring buffer shared memory: 0x" 
                  << std::hex << res << std::dec << std::endl;
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Ring buffer allocated at " << std::hex << ring_shm.buffer 
              << std::dec << std::endl;
    
    // Initialize ring buffer structure in Host (CRITICAL: Do this BEFORE passing to TA)
    struct SharedRingBuffer* rb = static_cast<struct SharedRingBuffer*>(ring_shm.buffer);
    rb->head = 0;
    rb->tail = 0;
    rb->size = RING_BUFFER_SIZE - sizeof(struct SharedRingBuffer);
    
    // Memory barrier to ensure initialization is visible
    __sync_synchronize();
    
    std::cout << "✓ Ring buffer structure initialized (head=0, tail=0, size=" 
              << rb->size << ")" << std::endl;
    
    // Start reader thread for real-time data reading (BEFORE invoking TA command)
    std::cout << "\n=== Starting Real-time Ring Buffer Reader Thread ===" << std::endl;
    g_ring_reader = std::make_unique<SimpleRingReader>(ring_shm.buffer);
    
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
    
    /* Allocate shared memory for data (separate from ring buffer) */
    std::cout << "\n=== Allocating Shared Memory for Data ===" << std::endl;
    
    const size_t SHM_SIZE = 4096; // 4KB for data
    std::cout << "Data Shared Memory Size: " << SHM_SIZE << " bytes" << std::endl;
    
    TEEC_SharedMemory shm;
    shm.size = SHM_SIZE;
    shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
    
    res = TEEC_AllocateSharedMemory(&ctx, &shm);
    if (res != TEEC_SUCCESS) {
        std::cerr << "Failed to allocate shared memory: 0x" << std::hex << res << std::endl;
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Shared memory allocated at " << std::hex << shm.buffer << std::dec << std::endl;
    
    // Initialize shared memory with zeros
    std::memset(shm.buffer, 0, shm.size);
    
    /* Prepare log buffer */
    char log_buffer[8192] = {0};
    
    /* Prepare operation - Pass RING BUFFER as param 0 for real-time logging */
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_WHOLE,            // Ring buffer (for real-time logging)
        TEEC_MEMREF_WHOLE,             // Data shared memory
        TEEC_VALUE_INPUT,              // Test value
        TEEC_MEMREF_TEMP_OUTPUT);      // OCALL logs
    
    // Pass ring buffer as param 0 (TA will write real-time logs here)
    op.params[0].memref.parent = &ring_shm;
    op.params[0].memref.offset = 0;
    op.params[0].memref.size = ring_shm.size;
    
    // Pass data shared memory as param 1
    op.params[1].memref.parent = &shm;
    op.params[1].memref.offset = 0;
    op.params[1].memref.size = shm.size;
    
    op.params[2].value.a = 12345;  // Test value
    op.params[3].tmpref.buffer = log_buffer;
    op.params[3].tmpref.size = sizeof(log_buffer);
    
    std::cout << "\n=== Invoking TEST_SIMPLE_SHM Command ===" << std::endl;
    std::cout << "[DEBUG] Parameter types: 0x" << std::hex << op.paramTypes << std::dec << std::endl;
    std::cout << "[DEBUG] Ring buffer: ptr=" << std::hex << ring_shm.buffer << std::dec 
              << ", size=" << ring_shm.size << std::endl;
    std::cout << "[DEBUG] Data shared memory: ptr=" << std::hex << shm.buffer << std::dec 
              << ", size=" << shm.size << std::endl;
    std::cout << "[DEBUG] Test value: " << op.params[2].value.a << std::endl;
    std::cout << "[INFO] Watch for real-time logs from ring buffer above!" << std::endl;
    std::cout.flush();
    
    // Temporarily pause reader thread to avoid conflicts during command execution
    // (We'll resume it after a short delay)
    if (g_ring_reader && g_ring_reader->IsRunning()) {
        // Reader thread can continue, but we'll add a small delay after invoke
        // to ensure TA has time to write initial data
    }
    
    // CRITICAL: Ensure ring buffer structure is properly initialized and visible to TA
    // Flush cache to ensure TA can see the initialization
    __sync_synchronize();
    
    std::cout << "[DEBUG] Calling TEEC_InvokeCommand..." << std::endl;
    std::cout.flush();
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_TEST_SIMPLE_SHM, &op, &err_origin);
    
    // Wait a bit for all ring buffer messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "[DEBUG] TEEC_InvokeCommand returned with code: 0x" << std::hex << res << std::dec << std::endl;
    std::cout << "[DEBUG] Error origin: 0x" << std::hex << err_origin << std::dec << std::endl;
    std::cout.flush();
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_InvokeCommand failed with code 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::dec << std::endl;
        
        if (log_buffer[0] != '\0') {
            std::cout << "\n--- TA Error Logs ---" << std::endl;
            std::cout << log_buffer << std::endl;
        }
        
        TEEC_ReleaseSharedMemory(&shm);
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    std::cout << "✓ Command executed successfully!" << std::endl;
    /* Check TA logs */
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    /* Verify data shared memory was written by TA */
    uint32_t* data_shm_ptr = static_cast<uint32_t*>(shm.buffer);
    uint32_t pattern = *data_shm_ptr;
    
    std::cout << "\n=== Verifying Shared Memory ===" << std::endl;
    std::cout << "Pattern written by TA: 0x" << std::hex << pattern << std::dec << std::endl;
    
    if (pattern == 0xDEADBEEF) {
        std::cout << "✓ Shared memory test PASSED!" << std::endl;
    } else {
        std::cout << "✗ Shared memory test FAILED! Expected 0xDEADBEEF, got 0x" 
                  << std::hex << pattern << std::dec << std::endl;
    }
    /* 
     * NEW TEST: String via Shared Memory
     */
    std::cout << "\n========================================" << std::endl;
    std::cout << " Testing String via Shared Memory" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Clear shared memory
    std::memset(shm.buffer, 0, shm.size);
    std::memset(log_buffer, 0, sizeof(log_buffer));
    
    // Prepare operation
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_WHOLE,            // Shared memory
        TEEC_MEMREF_TEMP_OUTPUT,      // OCALL logs
        TEEC_NONE,
        TEEC_NONE);
    
    op.params[0].memref.parent = &shm;
    op.params[0].memref.offset = 0;
    op.params[0].memref.size = shm.size;
    op.params[1].tmpref.buffer = log_buffer;
    op.params[1].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Invoking TA_EEVM_CMD_TEST_SHM_STRING..." << std::endl;
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_TEST_SHM_STRING, &op, &err_origin);
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_InvokeCommand failed with code 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::dec << std::endl;
    } else {
        std::cout << "✓ Command executed successfully!" << std::endl;
        
        // Read string from shared memory
        char* str_ptr = static_cast<char*>(shm.buffer);
        std::cout << "String from TA: \"" << str_ptr << "\"" << std::endl;
        
        if (std::string(str_ptr) == "Hello from Secure World via Shared Memory!") {
             std::cout << "✓ String test PASSED!" << std::endl;
        } else {
             std::cout << "✗ String test FAILED!" << std::endl;
        }
    }
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs (via parameter) ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    /* Cleanup */
    std::cout << "\n=== Stopping Ring Buffer Reader Thread ===" << std::endl;
    g_reader_running = false;
    if (g_ring_reader) {
        g_ring_reader->Stop();
        g_ring_reader.reset();
    }
    std::cout << "✓ Reader thread stopped" << std::endl;
    
    TEEC_ReleaseSharedMemory(&shm);
    TEEC_ReleaseSharedMemory(&ring_shm);
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return (pattern == 0xDEADBEEF) ? 0 : 1;
}

