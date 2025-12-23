// SPDX-License-Identifier: BSD-2-Clause
/*
 * Simple test for TEEC_InvokeCommand with Shared Memory
 * NO LevelDB - just test basic shared memory communication
 * 
 * NEW: Real-time logging via Ring Buffer in Shared Memory
 * - TA writes logs to ring buffer in shared memory
 * - Host has a consumer thread that reads and displays logs in real-time
 */

#include <iostream>
#include <cstring>
#include <cstdint>
/* OP-TEE TEE client API */
#include <tee_client_api.h>

/* TA UUID and commands */
#include <eevm_ta.h>
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
    
    const size_t SHM_SIZE = 4096;
    
    std::cout << "Shared Memory Size: " << SHM_SIZE << " bytes" << std::endl;
    
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
    
    /* Prepare operation */
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_WHOLE,            // Shared memory (WHOLE for allocated memory)
        TEEC_VALUE_INPUT,             // Test value
        TEEC_MEMREF_TEMP_OUTPUT,      // OCALL logs
        TEEC_NONE);
    
    op.params[0].memref.parent = &shm;
    op.params[0].memref.offset = 0;
    op.params[0].memref.size = shm.size;
    op.params[1].value.a = 12345;  // Test value
    op.params[2].tmpref.buffer = log_buffer;
    op.params[2].tmpref.size = sizeof(log_buffer);
    
    std::cout << "\n=== Invoking TEST_SIMPLE_SHM Command ===" << std::endl;
    std::cout << "[DEBUG] Parameter types: 0x" << std::hex << op.paramTypes << std::dec << std::endl;
    std::cout << "[DEBUG] Shared memory: ptr=" << std::hex << shm.buffer << std::dec 
              << ", size=" << shm.size << std::endl;
    std::cout << "[DEBUG] Test value: " << op.params[1].value.a << std::endl;
    std::cout.flush();
    
    std::cout << "[DEBUG] Calling TEEC_InvokeCommand..." << std::endl;
    std::cout.flush();
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_TEST_SIMPLE_SHM, &op, &err_origin);
    
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
    
    /* Verify shared memory was written by TA */
    uint32_t* shm_ptr = static_cast<uint32_t*>(shm.buffer);
    uint32_t pattern = *shm_ptr;
    
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
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    TEEC_ReleaseSharedMemory(&shm);
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return (pattern == 0xDEADBEEF) ? 0 : 1;
}

