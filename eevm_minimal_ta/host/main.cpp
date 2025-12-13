// SPDX-License-Identifier: BSD-2-Clause
/*
 * Host Application cho Minimal EVM TA (C++ Version)
 * Test: std::vector, std::map, Exceptions, và EVM Bytecode
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>

// C headers cho TEE Client API
extern "C" {
#include <tee_client_api.h>
}

#include "minimal_evm_ta.h"

using namespace std;

// Hàm tiện ích để báo lỗi
void print_error(const char* func, TEEC_Result res, uint32_t origin) {
    cerr << "ERROR: " << func << " failed." << endl;
    cerr << "  Code: 0x" << hex << res << endl;
    cerr << "  Origin: " << (origin == TEEC_ORIGIN_TEE ? "TEE" : "API/COMMS") << endl;
}

int main(void)
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_MINIMAL_EVM_UUID;
    uint32_t err_origin;

    cout << "========================================" << endl;
    cout << "    TEE C++ EVM / STL Container Test    " << endl;
    cout << "========================================" << endl << endl;

    /* 1. Khởi tạo Context */
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS) {
        print_error("TEEC_InitializeContext", res, 0);
        return 1;
    }

    /* 2. Mở Session kết nối tới TA */
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                   TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS) {
        print_error("TEEC_OpenSession", res, err_origin);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }

    // ---------------------------------------------------------
    // TEST 1: std::vector (Real C++ STL)
    // ---------------------------------------------------------
    cout << "[TEST 1] std::vector (libcxx)..." << endl;
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = 1; // Giá trị rác ban đầu

    res = TEEC_InvokeCommand(&sess, TA_MINIMAL_EVM_CMD_TEST_VECTOR, &op, &err_origin);

    if (res == TEEC_SUCCESS && op.params[0].value.a == 0) {
        cout << "  -> [PASS] Vector operations (push, iter, find, pop) OK." << endl;
    } else {
        cout << "  -> [FAIL] Vector test failed." << endl;
        print_error("Invoke TEST_VECTOR", res, err_origin);
    }
    cout << "----------------------------------------" << endl;

    // ---------------------------------------------------------
    // TEST 2: std::map (Real C++ STL)
    // ---------------------------------------------------------
    cout << "[TEST 2] std::map (libcxx)..." << endl;
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = 1; 

    res = TEEC_InvokeCommand(&sess, TA_MINIMAL_EVM_CMD_TEST_MAP, &op, &err_origin);

    if (res == TEEC_SUCCESS && op.params[0].value.a == 0) {
        cout << "  -> [PASS] Map operations (insert, find, erase) OK." << endl;
    } else {
        cout << "  -> [FAIL] Map test failed." << endl;
        print_error("Invoke TEST_MAP", res, err_origin);
    }
    cout << "----------------------------------------" << endl;

    // // ---------------------------------------------------------
    // // TEST 3: C++ Exception Handling
    // // ---------------------------------------------------------
    // cout << "[TEST 3] C++ Exception Handling (try-catch)..." << endl;
    // memset(&op, 0, sizeof(op));
    // op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    // op.params[0].value.a = 1;

    // res = TEEC_InvokeCommand(&sess, TA_MINIMAL_EVM_CMD_TEST_EXCEPTION, &op, &err_origin);

    // if (res == TEEC_SUCCESS && op.params[0].value.a == 0) {
    //     cout << "  -> [PASS] Exceptions thrown and caught correctly inside TEE." << endl;
    // } else {
    //     cout << "  -> [FAIL] Exception test failed (Crash or uncaught exception)." << endl;
    //     print_error("Invoke TEST_EXCEPTION", res, err_origin);
    // }
    // cout << "----------------------------------------" << endl;

    // // ---------------------------------------------------------
    // // TEST 4: Bytecode Execution (Stack Machine)
    // // ---------------------------------------------------------
    // cout << "[TEST 4] EVM Bytecode Execution..." << endl;
    
    // // Logic: PUSH 10, PUSH 20, ADD, RETURN
    // // Stack: [10] -> [10, 20] -> [30] -> Return 30
    // uint8_t bytecode[] = { 
    //     0x01, 10,  // PUSH 10
    //     0x01, 20,  // PUSH 20
    //     0x02,      // ADD
    //     0x03       // RETURN
    // };
    // uint8_t result_buffer[32] = {0};

    // memset(&op, 0, sizeof(op));
    // op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
    //                                  TEEC_MEMREF_TEMP_OUTPUT,
    //                                  TEEC_NONE, TEEC_NONE);
    
    // op.params[0].tmpref.buffer = bytecode;
    // op.params[0].tmpref.size = sizeof(bytecode);
    
    // op.params[1].tmpref.buffer = result_buffer;
    // op.params[1].tmpref.size = sizeof(result_buffer);

    // res = TEEC_InvokeCommand(&sess, TA_MINIMAL_EVM_CMD_EXECUTE_BYTECODE, &op, &err_origin);

    // if (res == TEEC_SUCCESS) {
    //     int result_val = (int)result_buffer[0];
    //     cout << "  Input: 10 + 20" << endl;
    //     cout << "  Output: " << dec << result_val << endl;
        
    //     if (result_val == 30) {
    //         cout << "  -> [PASS] Bytecode logic correct." << endl;
    //     } else {
    //         cout << "  -> [FAIL] Logic incorrect. Expected 30." << endl;
    //     }
    // } else {
    //     cout << "  -> [FAIL] Execution failed." << endl;
    //     print_error("Invoke EXECUTE_BYTECODE", res, err_origin);
    // }

    cout << endl;
    cout << "========================================" << endl;
    
    /* Đóng session và context */
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);

    return 0;
}