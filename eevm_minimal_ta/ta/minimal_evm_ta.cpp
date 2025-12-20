// SPDX-License-Identifier: BSD-2-Clause
/*
 * Minimal EVM TA - Testing REAL std::vector from libcxx
 * Using libcxx from OpenEnclave/LLVM
 */
#include <string> // Test string with object linking
// Include compatibility header to undefine conflicting macros
#include "libcxx_compat.h"
#include "ocall_logger.h" // Sử dụng logger mới
// Include TEE headers AFTER libcxx to avoid macro conflicts
extern "C"
{
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
}

#include "minimal_evm_ta.h"

/*
 * Simple OCALL - accumulate logs in local buffer, return to host
 */
// #define OCALL_BUFFER_SIZE 8192
// static char g_ocall_buffer[OCALL_BUFFER_SIZE];
// static size_t g_ocall_pos = 0;

// // Append log to buffer
// static void ocall_append(const char* msg) {
//     size_t msg_len = strlen(msg);
//     if (g_ocall_pos + msg_len + 1 < OCALL_BUFFER_SIZE) {
//         memcpy(g_ocall_buffer + g_ocall_pos, msg, msg_len);
//         g_ocall_pos += msg_len;
//         g_ocall_buffer[g_ocall_pos] = '\0';
//     }
// }

// // OCALL LOG macro
// #define OCALL_LOG(fmt, ...) do { \
//     char ocall_tmp[512]; \
//     int len = snprintf(ocall_tmp, sizeof(ocall_tmp), "[TA] " fmt "\n", ##__VA_ARGS__); \
//     if (len > 0) { \
//         ocall_append(ocall_tmp); \
//     } \
// } while(0)

/*
 * Called when the instance of the TA is created.
 */
TEE_Result TA_CreateEntryPoint(void)
{
    DMSG("TA_CreateEntryPoint - Testing std::vector from libcxx");
    return TEE_SUCCESS;
}

/*
 * Called when the instance of the TA is destroyed.
 */
void TA_DestroyEntryPoint(void)
{
    DMSG("TA_DestroyEntryPoint");
}

/*
 * Called when a new session is opened to the TA.
 */
TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
                                    TEE_Param __maybe_unused params[4],
                                    void __maybe_unused **sess_ctx)
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);

    DMSG("========================================");
    DMSG("Session opened - Minimal EVM TA");
    DMSG("Testing custom Vector and Map");
    DMSG("========================================");

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    (void)&params;
    (void)&sess_ctx;

    return TEE_SUCCESS;
}

/*
 * Called when a session is closed.
 */
void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
    DMSG("Session closed");
    (void)&sess_ctx;
}

/*
 * Test std::string with OCALL logging - comprehensive test
 */
static TEE_Result test_string_with_ocall(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INOUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types) return TEE_ERROR_BAD_PARAMETERS;

    // 1. Reset log cho lượt chạy này
    ocall_log_init();

    OCALL_LOG("=== String Test with OCALL ===");
    
    {  // Scope for goto
        // Test 1: Empty string
        OCALL_LOG("[INFO] Test 1: Empty string construction");
        std::string str1;
        OCALL_LOG("[VERBOSE]   Created empty string, size=%zu", str1.size());
        if (!str1.empty()) {
            OCALL_LOG("[ERROR]   FAIL: empty string not empty!");
            params[0].value.a = 1;
            goto done;
        }
        OCALL_LOG("[INFO]   PASS");
        
        // Test 2: String from literal
        OCALL_LOG("[INFO] Test 2: String from literal");
        std::string str2("Hello from Secure World!");
        OCALL_LOG("[VERBOSE]   Created string, size=%zu", str2.size());
        if (str2.size() != 24) {
            OCALL_LOG("[ERROR]   FAIL: size=%zu (expected 24)", str2.size());
            params[0].value.a = 2;
            goto done;
        }
        OCALL_LOG("[INFO]   PASS");
        
        // Test 3: String copy
        OCALL_LOG("[INFO] Test 3: String copy");
        std::string str3 = str2;
        if (str3.size() != str2.size()) {
            OCALL_LOG("[ERROR]   FAIL: copy size mismatch");
            params[0].value.a = 3;
            goto done;
        }
        OCALL_LOG("[VERBOSE]   Copied %zu bytes", str3.size());
        OCALL_LOG("[INFO]   PASS");
        
        // Test 4: String concatenation
        OCALL_LOG("[INFO] Test 4: String concatenation");
        str3 += " OCALL works!";
        OCALL_LOG("[VERBOSE]   After concat, size=%zu", str3.size());
        if (str3.size() != 37) {  // 24 + 13 = 37
            OCALL_LOG("[ERROR]   FAIL: size=%zu (expected 37)", str3.size());
            params[0].value.a = 4;
            goto done;
        }
        OCALL_LOG("[INFO]   PASS");
        
        // Test 5: String clear
        OCALL_LOG("[INFO] Test 5: String clear");
        str3.clear();
        if (!str3.empty()) {
            OCALL_LOG("[ERROR]   FAIL: not empty after clear");
            params[0].value.a = 5;
            goto done;
        }
        OCALL_LOG("[VERBOSE]   Cleared, size=%zu", str3.size());
        OCALL_LOG("[INFO]   PASS");
        
        OCALL_LOG("=== All 5 tests passed! ===");
        params[0].value.a = 0;
    }

done:
    // 2. Đẩy toàn bộ log tích lũy được về phía Host
    if (params[1].memref.buffer) {
        ocall_log_flush_to_params(params[1].memref.buffer, &params[1].memref.size);
    }
    
    return TEE_SUCCESS;
}

/*
 * Test std::string - basic test with dual-mode OCALL support
 */
static TEE_Result test_string(uint32_t param_types, TEE_Param params[4])
{
    // Support both modes: with or without log output
    bool has_log_output = false;
    
    uint32_t exp_param_types_no_log = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INOUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);
    
    uint32_t exp_param_types_with_log = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INOUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);
    
    if (param_types == exp_param_types_with_log) {
        has_log_output = true;
        ocall_log_init();
    } else if (param_types != exp_param_types_no_log) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    OCALL_LOG("=== Basic String Test ===");
    {
        // Test 1: Empty string
        OCALL_LOG("[INFO] Test 1: Empty string");
        std::string str1;
        if (!str1.empty())
        {
            OCALL_LOG("[ERROR]   FAIL: not empty");
            params[0].value.a = 1;
            goto done;
        }
        OCALL_LOG("[INFO]   PASS");

        // Test 2: String from literal
        OCALL_LOG("[INFO] Test 2: String literal");
        std::string str2("Hello");
        if (str2.size() != 5)
        {
            OCALL_LOG("[ERROR]   FAIL: size=%zu (expected 5)", str2.size());
            params[0].value.a = 2;
            goto done;
        }
        if (str2[0] != 'H' || str2[4] != 'o')
        {
            OCALL_LOG("[ERROR]   FAIL: content check");
            params[0].value.a = 3;
            goto done;
        }
        OCALL_LOG("[INFO]   PASS");

        // Test 3: String copy
        OCALL_LOG("[INFO] Test 3: String copy");
        std::string str3 = str2;
        if (str3.size() != str2.size())
        {
            OCALL_LOG("[ERROR]   FAIL: copy size mismatch");
            params[0].value.a = 4;
            goto done;
        }
        OCALL_LOG("[INFO]   PASS");

        // Test 4: String concatenation
        OCALL_LOG("[INFO] Test 4: Concatenation");
        str3 += " World";
        if (str3.size() != 11)
        {
            OCALL_LOG("[ERROR]   FAIL: size=%zu (expected 11)", str3.size());
            params[0].value.a = 5;
            goto done;
        }
        OCALL_LOG("[INFO]   PASS");

        // Test 5: Clear
        OCALL_LOG("[INFO] Test 5: Clear");
        str3.clear();
        if (!str3.empty())
        {
            OCALL_LOG("[ERROR]   FAIL: not empty after clear");
            params[0].value.a = 6;
            goto done;
        }
        OCALL_LOG("[INFO]   PASS");
        
        OCALL_LOG("=== All tests passed! ===");
        params[0].value.a = 0; // PASS
    }

done:
    // Copy logs to output buffer if requested
    if (has_log_output && params[1].memref.buffer) {
        ocall_log_flush_to_params(params[1].memref.buffer, &params[1].memref.size);
    }
    
    return TEE_SUCCESS;
}

/*
 * Called when a TA is invoked.
 */
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
                                      uint32_t cmd_id,
                                      uint32_t param_types, TEE_Param params[4])
{
    (void)&sess_ctx;

    switch (cmd_id)
    {
    case TA_MINIMAL_EVM_CMD_TEST_STRING:
        return test_string(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_STRING_OCALL:
        return test_string_with_ocall(param_types, params);
    default:
        EMSG("Unknown command ID: %u", cmd_id);
        return TEE_ERROR_BAD_PARAMETERS;
    }
}
