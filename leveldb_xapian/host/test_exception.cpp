// SPDX-License-Identifier: BSD-2-Clause
// Test exception handling and std::map in TA

#include <iostream>
#include <cstring>
#include <tee_client_api.h>
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
    std::cout << "Testing Exception Handling & std::map" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    // Initialize context
    res = TEEC_InitializeContext(nullptr, &ctx);
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_InitializeContext failed: 0x" << std::hex << res << std::endl;
        return 1;
    }
    
    std::cout << "✓ TEE Context initialized" << std::endl;
    
    // Open session
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, 
                                      TEEC_NONE, TEEC_MEMREF_TEMP_OUTPUT);
    
    char log_buffer[8192] = {0};
    op.params[3].tmpref.buffer = log_buffer;
    op.params[3].tmpref.size = sizeof(log_buffer);
    
    res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, nullptr, &op, &err_origin);
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_OpenSession failed: 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::endl;
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Session opened" << std::endl << std::endl;
    
    // Test exception handling
    std::cout << "=== Testing Exception Handling ===" << std::endl;
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
                                      TEEC_NONE, TEEC_MEMREF_TEMP_OUTPUT);
    op.params[3].tmpref.buffer = log_buffer;
    op.params[3].tmpref.size = sizeof(log_buffer);
    std::memset(log_buffer, 0, sizeof(log_buffer));
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_TEST_EXCEPTION, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res == TEEC_SUCCESS) {
        std::cout << "\n✓ Exception handling test PASSED!" << std::endl;
    } else {
        std::cerr << "\n✗ Exception handling test FAILED: 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::endl;
    }
    
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
    
    return (res == TEEC_SUCCESS) ? 0 : 1;
}

