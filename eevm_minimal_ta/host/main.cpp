// SPDX-License-Identifier: BSD-2-Clause
/*
 * Host Application - C++17 libcxx Feature Test Suite
 * Tests OpenEnclave libcxx compatibility in OP-TEE TA
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

struct TestCase {
    const char* name;
    uint32_t cmd_id;
};

// Hàm tiện ích để báo lỗi
void print_error(const char* func, TEEC_Result res, uint32_t origin) {
    cerr << "ERROR: " << func << " failed." << endl;
    cerr << "  Code: 0x" << hex << res << endl;
    cerr << "  Origin: " << (origin == TEEC_ORIGIN_TEE ? "TEE" : "API/COMMS") << endl;
}

void run_test(TEEC_Session* sess, const char* name, uint32_t cmd_id, 
              int* passed, int* failed) {
    TEEC_Result res;
    TEEC_Operation op;
    uint32_t err_origin;

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, 
                                     TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = 1; // Giá trị ban đầu (FAIL)

    res = TEEC_InvokeCommand(sess, cmd_id, &op, &err_origin);

    if (res == TEEC_SUCCESS && op.params[0].value.a == 0) {
        cout << "  [PASS] " << name << endl;
        (*passed)++;
    } else {
        cout << "  [FAIL] " << name << " (res=0x" << hex << res 
             << ", ret=" << dec << op.params[0].value.a << ")" << endl;
        (*failed)++;
    }
}

int main(void)
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_UUID uuid = TA_MINIMAL_EVM_UUID;
    uint32_t err_origin;

    cout << "========================================" << endl;
    cout << "  C++17 libcxx Feature Test Suite      " << endl;
    cout << "  OpenEnclave Compatibility Check      " << endl;
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

    int passed = 0;
    int failed = 0;

    cout << "Running tests..." << endl;
    cout << "----------------------------------------" << endl;

    // Basic containers
    run_test(&sess, "std::vector", TA_MINIMAL_EVM_CMD_TEST_VECTOR, &passed, &failed);
    run_test(&sess, "std::map<int,int>", TA_MINIMAL_EVM_CMD_TEST_MAP, &passed, &failed);
    run_test(&sess, "std::set", TA_MINIMAL_EVM_CMD_TEST_SET, &passed, &failed);
    run_test(&sess, "std::deque", TA_MINIMAL_EVM_CMD_TEST_DEQUE, &passed, &failed);
    run_test(&sess, "std::list", TA_MINIMAL_EVM_CMD_TEST_LIST, &passed, &failed);
    run_test(&sess, "std::forward_list", TA_MINIMAL_EVM_CMD_TEST_FORWARD_LIST, &passed, &failed);
    run_test(&sess, "std::array", TA_MINIMAL_EVM_CMD_TEST_ARRAY, &passed, &failed);
    
    // Unordered containers - REMOVED (cmath conflicts)
    // run_test(&sess, "std::unordered_map", TA_MINIMAL_EVM_CMD_TEST_UNORDERED_MAP, &passed, &failed);
    
    // Adapters
    run_test(&sess, "std::queue", TA_MINIMAL_EVM_CMD_TEST_QUEUE, &passed, &failed);
    
    // C++17 utilities
    run_test(&sess, "std::optional (C++17)", TA_MINIMAL_EVM_CMD_TEST_OPTIONAL, &passed, &failed);
    run_test(&sess, "std::variant (C++17)", TA_MINIMAL_EVM_CMD_TEST_VARIANT, &passed, &failed);
    // NOTE: any removed - needs RTTI
    // run_test(run_test(&sess, "std::any (C++17)", TA_MINIMAL_EVM_CMD_TEST_ANY, &passed, &failed);sess, "std::any (C++17)", TA_MINIMAL_EVM_CMD_TEST_ANY, &passed, &failed);
    run_test(&sess, "std::tuple", TA_MINIMAL_EVM_CMD_TEST_TUPLE, &passed, &failed);
    
    // Functional
    run_test(&sess, "Lambda expressions (C++11)", TA_MINIMAL_EVM_CMD_TEST_FUNCTIONAL, &passed, &failed);
    
    // Algorithms
    run_test(&sess, "std::algorithm (find/count - no sort)", TA_MINIMAL_EVM_CMD_TEST_ALGORITHM, &passed, &failed);
    // NOTE: numeric removed - includes cmath
    // run_test(&sess, "std::numeric (accumulate)", TA_MINIMAL_EVM_CMD_TEST_NUMERIC, &passed, &failed);
    
    // Memory management
    // NOTE: memory removed - shared_ptr needs RTTI
    // run_test(run_test(&sess, "std::unique_ptr/shared_ptr", TA_MINIMAL_EVM_CMD_TEST_MEMORY, &passed, &failed);sess, "std::unique_ptr/shared_ptr", TA_MINIMAL_EVM_CMD_TEST_MEMORY, &passed, &failed);
    
    // Exception handling - REMOVED (needs -fexceptions in TA)
    // run_test(&sess, "std::exception (try-catch)", TA_MINIMAL_EVM_CMD_TEST_EXCEPTION, &passed, &failed);

    cout << "----------------------------------------" << endl;
    cout << "Test Results:" << endl;
    cout << "  PASSED: " << passed << endl;
    cout << "  FAILED: " << failed << endl;
    cout << "  TOTAL:  " << (passed + failed) << endl;
    cout << "========================================" << endl;

    if (failed == 0) {
        cout << "ALL TESTS PASSED! ✓" << endl;
    } else {
        cout << "SOME TESTS FAILED! ✗" << endl;
    }

    /* 3. Đóng Session và Context */
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);

    return (failed == 0) ? 0 : 1;
}
