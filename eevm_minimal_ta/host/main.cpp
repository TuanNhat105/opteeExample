// SPDX-License-Identifier: BSD-2-Clause
/*
 * Host Application - C++17 libcxx Feature Test Suite
 * Tests OpenEnclave libcxx compatibility in OP-TEE TA
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// C headers cho TEE Client API
extern "C"
{
#include <tee_client_api.h>
}

#include "minimal_evm_ta.h"

using namespace std;

void run_test(TEEC_Session *sess, const char *name, uint32_t cmd_id,
              int *passed, int *failed)
{
    TEEC_Result res;
    TEEC_Operation op;
    uint32_t err_origin;

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE,
                                     TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = 1; // Giá trị ban đầu (FAIL)

    res = TEEC_InvokeCommand(sess, cmd_id, &op, &err_origin);

    if (res == TEEC_SUCCESS && op.params[0].value.a == 0)
    {
        cout << "  [PASS] " << name << endl;
        (*passed)++;
    }
    else
    {
        cout << "  [FAIL] " << name << " (res=0x" << hex << res
             << ", ret=" << dec << op.params[0].value.a << ")" << endl;
        (*failed)++;
    }
}

void run_test_with_logs(TEEC_Session *sess, const char *name, uint32_t cmd_id,
                        int *passed, int *failed)
{
    TEEC_Result res;
    TEEC_Operation op;
    uint32_t err_origin;
    char log_buffer[8192];

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT, // Log output
        TEEC_NONE,
        TEEC_NONE);

    op.params[0].value.a = 1; // Initial value (FAIL)
    op.params[1].tmpref.buffer = log_buffer;
    op.params[1].tmpref.size = sizeof(log_buffer);

    res = TEEC_InvokeCommand(sess, cmd_id, &op, &err_origin);

    // Print logs from TA
    if (res == TEEC_SUCCESS && op.params[1].tmpref.size > 0)
    {
        cout << "\n"
             << log_buffer << endl;
    }

    if (res == TEEC_SUCCESS && op.params[0].value.a == 0)
    {
        cout << "  [PASS] " << name << endl;
        (*passed)++;
    }
    else
    {
        cout << "  [FAIL] " << name << " (res=0x" << hex << res
             << ", ret=" << dec << op.params[0].value.a << ")" << endl;
        (*failed)++;
    }
}

// Hàm tiện ích để báo lỗi
void print_error(const char *func, TEEC_Result res, uint32_t origin)
{
    cerr << "ERROR: " << func << " failed." << endl;
    cerr << "  Code: 0x" << hex << res << endl;
    cerr << "  Origin: " << (origin == TEEC_ORIGIN_TEE ? "TEE" : "API/COMMS") << endl;
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
    cout << "========================================" << endl
         << endl;

    /* 1. Khởi tạo Context */
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS)
    {
        print_error("TEEC_InitializeContext", res, 0);
        return 1;
    }
    /* 2. Mở Session kết nối tới TA */
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                           TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS)
    {
        print_error("TEEC_OpenSession", res, err_origin);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }

    int passed = 0;
    int failed = 0;

    cout << "----------------------------------------" << endl;

    // String test WITH OCALL logging (logs returned in buffer)
    run_test_with_logs(&sess, "std::string (comprehensive)", TA_MINIMAL_EVM_CMD_TEST_STRING_OCALL, &passed, &failed);
    // Basic string test - also with OCALL now!
    run_test_with_logs(&sess, "std::string (basic)", TA_MINIMAL_EVM_CMD_TEST_STRING, &passed, &failed);

    cout << "----------------------------------------" << endl;
    cout << "Test Results:" << endl;
    cout << "  PASSED: " << passed << endl;
    cout << "  FAILED: " << failed << endl;
    cout << "  TOTAL:  " << (passed + failed) << endl;
    cout << "========================================" << endl;

    if (failed == 0)
    {
        cout << "ALL TESTS PASSED! ✓" << endl;
    }
    else
    {
        cout << "SOME TESTS FAILED! ✗" << endl;
    }

    /* 3. Đóng Session và Context */
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);

    return (failed == 0) ? 0 : 1;
}
