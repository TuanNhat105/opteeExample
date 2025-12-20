// Example: Simple usage of LevelDB Ring Buffer TA

#include <iostream>
#include <tee_client_api.h>
#include <eevm_ta.h>
#include "ring_buffer_consumer.hpp"

void example_simple_put_get() {
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_EEVM_UUID;
    uint32_t err_origin;
    
    // 1. Initialize TEE context and session
    TEEC_InitializeContext(nullptr, &ctx);
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, nullptr, &op, &err_origin);
    
    // 2. Allocate shared memory for Ring Buffer
    TEEC_SharedMemory shm;
    shm.size = 4 * 1024 * 1024; // 4MB
    shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
    TEEC_AllocateSharedMemory(&ctx, &shm);
    memset(shm.buffer, 0, shm.size);
    
    // 3. Start consumer thread
    RingBufferConsumer consumer(shm.buffer, "/tmp/leveldb_secure");
    consumer.RegisterFile(1000, "MANIFEST-000001");
    consumer.RegisterFile(1001, "CURRENT");
    consumer.Start();
    
    // 4. Initialize LevelDB in TA
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_PARTIAL_INOUT,
        TEEC_VALUE_INPUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_NONE);
    op.params[0].memref.parent = &shm;
    op.params[0].memref.size = shm.size;
    op.params[1].value.a = shm.size;
    
    char log_buf[4096];
    op.params[2].tmpref.buffer = log_buf;
    op.params[2].tmpref.size = sizeof(log_buf);
    
    TEEC_InvokeCommand(&sess, TA_EEVM_CMD_INIT_LEVELDB, &op, &err_origin);
    
    // 5. PUT operation
    std::string key = "hello";
    std::string value = "world from TEE!";
    
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,  // key
        TEEC_MEMREF_TEMP_INPUT,  // value
        TEEC_VALUE_OUTPUT,       // status
        TEEC_NONE);
    
    op.params[0].tmpref.buffer = (void*)key.data();
    op.params[0].tmpref.size = key.size();
    op.params[1].tmpref.buffer = (void*)value.data();
    op.params[1].tmpref.size = value.size();
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_PUT, &op, &err_origin);
    
    if (res == TEEC_SUCCESS && op.params[2].value.a == 0) {
        std::cout << "✓ PUT successful!" << std::endl;
    }
    
    // Give consumer time to flush
    sleep(1);
    
    // 6. GET operation
    char get_buf[256];
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,   // key
        TEEC_MEMREF_TEMP_OUTPUT,  // value
        TEEC_VALUE_OUTPUT,        // status
        TEEC_NONE);
    
    op.params[0].tmpref.buffer = (void*)key.data();
    op.params[0].tmpref.size = key.size();
    op.params[1].tmpref.buffer = get_buf;
    op.params[1].tmpref.size = sizeof(get_buf);
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_GET, &op, &err_origin);
    
    if (res == TEEC_SUCCESS && op.params[2].value.a == 0) {
        std::string retrieved(get_buf, op.params[1].tmpref.size);
        std::cout << "✓ GET successful! Value: " << retrieved << std::endl;
    }
    
    // 7. Cleanup
    consumer.Stop();
    TEEC_ReleaseSharedMemory(&shm);
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
}

// Example: Batch operations
void example_batch_operations() {
    // Similar setup as above...
    
    // Batch PUT
    std::vector<std::pair<std::string, std::string>> data = {
        {"user:1", "Alice"},
        {"user:2", "Bob"},
        {"user:3", "Charlie"},
    };
    
    for (const auto& [key, value] : data) {
        // PUT each key-value pair
        // (same code as above PUT operation)
    }
    
    std::cout << "✓ Batch PUT completed" << std::endl;
}

// Example: Error handling
void example_error_handling() {
    // Try to GET a non-existent key
    
    // Setup...
    
    TEEC_Operation op;
    char get_buf[256];
    std::string key = "non_existent_key";
    
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_VALUE_OUTPUT,
        TEEC_NONE);
    
    op.params[0].tmpref.buffer = (void*)key.data();
    op.params[0].tmpref.size = key.size();
    op.params[1].tmpref.buffer = get_buf;
    op.params[1].tmpref.size = sizeof(get_buf);
    
    TEEC_Result res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_GET, &op, &err_origin);
    
    if (res == TEEC_SUCCESS) {
        uint32_t status = op.params[2].value.a;
        switch (status) {
            case 0:
                std::cout << "Key found" << std::endl;
                break;
            case 1:
                std::cout << "Key not found (expected)" << std::endl;
                break;
            case 2:
                std::cerr << "Error occurred during GET" << std::endl;
                break;
        }
    }
}

int main() {
    std::cout << "=== LevelDB Ring Buffer Examples ===" << std::endl;
    
    std::cout << "\n1. Simple PUT and GET:" << std::endl;
    example_simple_put_get();
    
    // Uncomment to run other examples
    // std::cout << "\n2. Batch Operations:" << std::endl;
    // example_batch_operations();
    
    // std::cout << "\n3. Error Handling:" << std::endl;
    // example_error_handling();
    
    return 0;
}
