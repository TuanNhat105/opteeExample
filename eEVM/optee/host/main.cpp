// SPDX-License-Identifier: BSD-2-Clause
/*
 * Host application for eEVM TA - C++ Version with OCALL logging
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <err.h>

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
    
    /* Initialize a context connecting us to the TEE */
    res = TEEC_InitializeContext(nullptr, &ctx);
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_InitializeContext failed with code 0x" << std::hex << res << std::endl;
        return 1;
    }
    
    /* Prepare OCALL log buffer for OpenSession */
    char log_buffer[8192] = {0};
    
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_NONE,
        TEEC_NONE,
        TEEC_MEMREF_TEMP_OUTPUT,  // For OCALL logs
        TEEC_NONE);
    
    op.params[2].tmpref.buffer = log_buffer;
    op.params[2].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Opening session to eEVM TA..." << std::endl;
    
    /* Open a session to the TA with OCALL logging */
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                          TEEC_LOGIN_PUBLIC, nullptr, &op, &err_origin);
    /* Print OCALL logs from OpenSession */
    if (log_buffer[0] != '\0') {
        std::cout << "\n=== OCALL Logs from TA_OpenSessionEntryPoint ===" << std::endl;
        std::cout << log_buffer;
        std::cout << "=== End of OCALL Logs ===" << std::endl << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_OpenSession failed with code 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::endl;
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "eEVM TA Session Opened Successfully!" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    /* Test eEVM Hello World */
    std::cout << "=== Testing eEVM Hello World ===" << std::endl;
    memset(log_buffer, 0, sizeof(log_buffer));
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_OUTPUT,            // Exit reason
        TEEC_VALUE_OUTPUT,            // Result size
        TEEC_MEMREF_TEMP_OUTPUT,      // For OCALL logs
        TEEC_NONE);
    
    op.params[2].tmpref.buffer = log_buffer;
    op.params[2].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Invoking TA_EEVM_CMD_HELLO_WORLD..." << std::endl;
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_HELLO_WORLD, &op, &err_origin);
    
    /* Print OCALL logs from Hello World test */
    if (log_buffer[0] != '\0') {
        std::cout << "\n=== OCALL Logs from eEVM Hello World ===" << std::endl;
        std::cout << log_buffer;
        std::cout << "=== End of OCALL Logs ===" << std::endl << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ eEVM Hello World test failed with code 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::endl;
        std::cerr << "Exit reason: " << std::dec << op.params[0].value.a << std::endl;
    } else {
        std::cout << "✓ eEVM Hello World test completed successfully!" << std::endl;
        std::cout << "Exit reason: " << op.params[0].value.a << std::endl;
        std::cout << "Result size: " << op.params[1].value.a << " bytes" << std::endl;
    }
    std::cout << std::endl;

    /* Test Stack Operations */
    std::cout << "=== Testing eEVM Stack ===" << std::endl;
    memset(log_buffer, 0, sizeof(log_buffer));
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_NONE,
        TEEC_NONE,
        TEEC_MEMREF_TEMP_OUTPUT,  // For OCALL logs
        TEEC_NONE);
    
    op.params[2].tmpref.buffer = log_buffer;
    op.params[2].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Invoking TA_EEVM_CMD_TEST_STACK..." << std::endl;
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_TEST_STACK, &op, &err_origin);
    
    /* Print OCALL logs from stack test */
    if (log_buffer[0] != '\0') {
        std::cout << "\n=== OCALL Logs from Stack Test ===" << std::endl;
        std::cout << log_buffer;
        std::cout << "=== End of Stack Test Logs ===" << std::endl << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ Stack test failed with code 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::endl;
    } else {
        std::cout << "✓ Stack test completed successfully!" << std::endl;
    }

    /* Close session and finalize context */
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);

    return 0;
}
