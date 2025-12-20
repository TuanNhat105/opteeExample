// SPDX-License-Identifier: BSD-2-Clause
/*
 * Host application for LevelDB TA with Ring Buffer
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <err.h>
#include <sys/mman.h>
#include <thread>
#include <chrono>

/* OP-TEE TEE client API */
#include <tee_client_api.h>

/* TA UUID and commands */
#include <eevm_ta.h>

/* Ring Buffer Consumer */
#include "ring_buffer_consumer.hpp"

int main()
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_EEVM_UUID;
    uint32_t err_origin;
    
    std::cout << "========================================" << std::endl;
    std::cout << " LevelDB with Ring Buffer Test" << std::endl;
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

    // ==================== Setup Shared Memory ====================
    std::cout << "=== Setting up Shared Memory Ring Buffer ===" << std::endl;
    
    const size_t RING_BUFFER_SIZE = DEFAULT_RING_BUFFER_SIZE; // 4MB
    std::cout << "Ring Buffer Size: " << RING_BUFFER_SIZE / 1024 << " KB" << std::endl;
    
    TEEC_SharedMemory shm;
    shm.size = RING_BUFFER_SIZE;
    shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
    
    res = TEEC_AllocateSharedMemory(&ctx, &shm);
    if (res != TEEC_SUCCESS) {
        std::cerr << "Failed to allocate shared memory: 0x" << std::hex << res << std::endl;
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Shared memory allocated at " << shm.buffer << std::endl;
    
    // Initialize shared memory
    std::memset(shm.buffer, 0, shm.size);
    
    // ==================== Start Ring Buffer Consumer ====================
    std::cout << "✓ Starting Ring Buffer Consumer thread..." << std::endl;
    
    RingBufferConsumer consumer(shm.buffer, "/tmp/leveldb_secure");
    
    // Register some virtual file descriptors
    consumer.RegisterFile(1000, "MANIFEST-000001");
    consumer.RegisterFile(1001, "CURRENT");
    consumer.RegisterFile(1002, "000003.log");
    consumer.RegisterFile(1003, "000002.ldb");
    
    consumer.Start();
    std::cout << "✓ Consumer thread started" << std::endl << std::endl;
    
    // Give consumer thread time to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ==================== Initialize LevelDB in TA ====================
    std::cout << "=== Initializing LevelDB in TA ===" << std::endl;
    
    char log_buffer[8192] = {0};
    
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_PARTIAL_INOUT,    // Shared memory
        TEEC_VALUE_INPUT,             // Buffer size
        TEEC_MEMREF_TEMP_OUTPUT,      // OCALL logs
        TEEC_NONE);
    
    op.params[0].memref.parent = &shm;
    op.params[0].memref.offset = 0;
    op.params[0].memref.size = shm.size;
    op.params[1].value.a = RING_BUFFER_SIZE;
    op.params[2].tmpref.buffer = log_buffer;
    op.params[2].tmpref.size = sizeof(log_buffer);
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_INIT_LEVELDB, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ LevelDB initialization failed: 0x" << std::hex << res << std::endl;
        consumer.Stop();
        TEEC_ReleaseSharedMemory(&shm);
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ LevelDB initialized successfully!" << std::endl << std::endl;

    // ==================== Test PUT Operation ====================
    std::cout << "=== Testing PUT Operation ===" << std::endl;
    
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
        std::cerr << "❌ PUT failed: 0x" << std::hex << res << std::endl;
    } else if (op.params[2].value.a != 0) {
        std::cerr << "❌ PUT returned error status: " << op.params[2].value.a << std::endl;
    } else {
        std::cout << "✓ PUT successful!" << std::endl;
    }
    
    // Give consumer time to process
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
        std::cerr << "❌ GET failed: 0x" << std::hex << res << std::endl;
    } else {
        uint32_t status = op.params[2].value.a;
        if (status == 0) {
            std::string retrieved(get_buffer, op.params[1].tmpref.size);
            std::cout << "✓ GET successful! Value: '" << retrieved << "'" << std::endl;
            
            if (retrieved == test_value) {
                std::cout << "✓✓ Value matches!" << std::endl;
            } else {
                std::cout << "❌ Value mismatch!" << std::endl;
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
        {"config:timeout", "30000"},
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
    
    // Give consumer time to flush
    std::cout << "Waiting for consumer to flush data..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ==================== Cleanup ====================
    std::cout << "\n=== Cleaning up ===" << std::endl;
    
    consumer.Stop();
    std::cout << "✓ Consumer stopped" << std::endl;
    
    TEEC_ReleaseSharedMemory(&shm);
    std::cout << "✓ Shared memory released" << std::endl;
    
    TEEC_CloseSession(&sess);
    std::cout << "✓ Session closed" << std::endl;
    
    TEEC_FinalizeContext(&ctx);
    std::cout << "✓ Context finalized" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << " Test completed successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
