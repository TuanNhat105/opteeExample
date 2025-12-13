// SPDX-License-Identifier: BSD-2-Clause
/*
 * Minimal EVM TA - Testing REAL std::vector from libcxx
 * Using libcxx from OpenEnclave/LLVM
 */

// C++ STL headers (from libcxx with FULL musl runtime!)
#include <vector>
#include <algorithm>  // For std::find, std::sort
// #include <map>       // NOT working yet (needs exception support)
// #include <iterator>   // Test later

// Include TEE headers as C
extern "C"
{
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
}
#include "minimal_evm_ta.h"
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
 * Test REAL std::vector from libcxx!
 */
static TEE_Result test_real_vector(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    DMSG("========================================");
    DMSG("Testing REAL std::vector from libcxx");
    DMSG("========================================");

    // try
    // {
        // Create std::vector - this is THE REAL STL!
        std::vector<int> vec;

        DMSG("Test 1: Push Fibonacci sequence");
        int fibonacci[] = {1, 2, 3, 5, 8, 13, 21, 34, 55, 89};

        for (int num : fibonacci)
        {
            vec.push_back(num);
            DMSG("  Pushed: %d", num);
        }

        DMSG("Vector size: %zu", vec.size());
        DMSG("Vector capacity: %zu", vec.capacity());

        // Test 2: Range-based for loop
        DMSG("\nTest 2: Iterate using range-based for");
        int index = 0;
        for (int val : vec)
        {
            DMSG("  vec[%d] = %d", index++, val);
        }

        // Test 3: STL algorithms (requires <algorithm>)
        DMSG("\nTest 3: Using STL algorithm (find) - SKIPPED (needs <algorithm>)");
        auto it = std::find(vec.begin(), vec.end(), 13);
        if (it != vec.end())
        {
            DMSG("  Found 13 at position: %zu", std::distance(vec.begin(), it));
        }

        // Test 4: Vector operations
        DMSG("\nTest 4: Vector operations");
        vec.pop_back();
        DMSG("  After pop_back, size: %zu", vec.size());

        vec.clear();
        DMSG("  After clear, size: %zu", vec.size());
        DMSG("  Is empty: %s", vec.empty() ? "yes" : "no");

        params[0].value.a = 0; // Success

        DMSG("========================================");
        DMSG("std::vector TEST PASSED!");
        DMSG("========================================");

        return TEE_SUCCESS;
    // }
    // catch (const std::exception &e)
    // {
    //     EMSG("Exception caught: %s", e.what());
    //     return TEE_ERROR_GENERIC;
    // }
    // catch (...)
    // {
    //     EMSG("Unknown exception caught!");
    //     return TEE_ERROR_GENERIC;
    // }
}

/*
 * Test REAL std::map - NO MORE CUSTOM IMPLEMENTATION!
 */
// static TEE_Result test_real_map(uint32_t param_types, TEE_Param params[4])
// {
//     uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
//                                                TEE_PARAM_TYPE_NONE,
//                                                TEE_PARAM_TYPE_NONE,
//                                                TEE_PARAM_TYPE_NONE);

//     if (param_types != exp_param_types)
//         return TEE_ERROR_BAD_PARAMETERS;

//     DMSG("========================================");
//     DMSG("Testing REAL std::map from libcxx");
//     DMSG("========================================");

//     try
//     {
//         // Create std::map - this is THE REAL STL!
//         std::map<int, int> mymap;

//         DMSG("Test 1: Insert key-value pairs");
//         mymap[1] = 100;
//         mymap[2] = 200;
//         mymap[3] = 300;
//         mymap[5] = 500;
//         mymap[8] = 800;

//         DMSG("Map size: %zu", mymap.size());

//         // Test 2: Iterate through map
//         DMSG("\nTest 2: Iterate through map");
//         for (const auto &pair : mymap)
//         {
//             DMSG("  Key: %d -> Value: %d", pair.first, pair.second);
//         }

//         // Test 3: Find operation
//         DMSG("\nTest 3: Find key 5");
//         auto it = mymap.find(5);
//         if (it != mymap.end())
//         {
//             DMSG("  Found: key=%d, value=%d", it->first, it->second);
//         }

//         // Test 4: Count operation
//         DMSG("\nTest 4: Count occurrences");
//         DMSG("  Count of key 3: %zu", mymap.count(3));
//         DMSG("  Count of key 99: %zu", mymap.count(99));

//         // Test 5: Erase operation
//         DMSG("\nTest 5: Erase key 2");
//         mymap.erase(2);
//         DMSG("  After erase, size: %zu", mymap.size());

//         // Test 6: Clear
//         mymap.clear();
//         DMSG("\nTest 6: After clear, size: %zu", mymap.size());
//         DMSG("  Is empty: %s", mymap.empty() ? "yes" : "no");

//         params[0].value.a = 0; // Success

//         DMSG("========================================");
//         DMSG("std::map TEST PASSED!");
//         DMSG("========================================");

//         return TEE_SUCCESS;
//     }
//     catch (const std::exception &e)
//     {
//         EMSG("Exception caught: %s", e.what());
//         return TEE_ERROR_GENERIC;
//     }
//     catch (...)
//     {
//         EMSG("Unknown exception caught!");
//         return TEE_ERROR_GENERIC;
//     }
// }

// /*
//  * Test Exception Handling
//  */
// static TEE_Result test_exceptions(uint32_t param_types, TEE_Param params[4])
// {
//     uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
//                                                TEE_PARAM_TYPE_NONE,
//                                                TEE_PARAM_TYPE_NONE,
//                                                TEE_PARAM_TYPE_NONE);

//     if (param_types != exp_param_types)
//         return TEE_ERROR_BAD_PARAMETERS;

//     DMSG("========================================");
//     DMSG("Testing Exception Handling");
//     DMSG("========================================");

//     try
//     {
//         DMSG("Test 1: Throwing std::runtime_error");
//         throw std::runtime_error("This is a test exception");
//     }
//     catch (const std::runtime_error &e)
//     {
//         DMSG("  Caught runtime_error: %s", e.what());
//     }
//     catch (const std::exception &e)
//     {
//         DMSG("  Caught exception: %s", e.what());
//     }
//     catch (...)
//     {
//         EMSG("  Caught unknown exception");
//         return TEE_ERROR_GENERIC;
//     }

//     DMSG("\nTest 2: Nested try-catch");
//     try
//     {
//         try
//         {
//             throw std::logic_error("Nested exception");
//         }
//         catch (const std::logic_error &e)
//         {
//             DMSG("  Inner catch: %s", e.what());
//             throw; // Re-throw
//         }
//     }
//     catch (const std::exception &e)
//     {
//         DMSG("  Outer catch: %s", e.what());
//     }

//     params[0].value.a = 0; // Success

//     DMSG("========================================");
//     DMSG("Exception Handling TEST PASSED!");
//     DMSG("========================================");

//     return TEE_SUCCESS;
// }

/*
 * Simple bytecode execution (proof of concept)
 */
static TEE_Result execute_bytecode(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
                                               TEE_PARAM_TYPE_MEMREF_OUTPUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    const uint8_t *bytecode = (const uint8_t *)params[0].memref.buffer;
    size_t bytecode_len = params[0].memref.size;

    DMSG("Executing bytecode (%zu bytes)...", bytecode_len);

    // Simple interpreter using REAL std::vector!
    // Opcodes: 0x01 = PUSH, 0x02 = ADD, 0x03 = RETURN

    std::vector<uint8_t> stack;

    for (size_t i = 0; i < bytecode_len; i++)
    {
        uint8_t opcode = bytecode[i];
        DMSG("PC=%zu, OPCODE=0x%02X", i, opcode);

        switch (opcode)
        {
        case 0x01: // PUSH
            if (i + 1 >= bytecode_len)
            {
                EMSG("PUSH: not enough bytes");
                return TEE_ERROR_BAD_FORMAT;
            }
            i++;
            stack.push_back(bytecode[i]);
            DMSG("  PUSH %u", bytecode[i]);
            break;

        case 0x02: // ADD
            if (stack.size() < 2)
            {
                EMSG("ADD: stack underflow");
                return TEE_ERROR_BAD_STATE;
            }
            {
                uint8_t b = stack[stack.size() - 1];
                uint8_t a = stack[stack.size() - 2];
                stack.pop_back();
                stack.pop_back();
                stack.push_back(a + b);
                DMSG("  ADD: %u + %u = %u", a, b, a + b);
            }
            break;

        case 0x03: // RETURN
            if (stack.empty())
            {
                EMSG("RETURN: empty stack");
                return TEE_ERROR_BAD_STATE;
            }
            {
                uint8_t result = stack.back();
                DMSG("  RETURN: %u", result);

                if (params[1].memref.size >= 1)
                {
                    ((uint8_t *)params[1].memref.buffer)[0] = result;
                    params[1].memref.size = 1;
                }
            }
            return TEE_SUCCESS;

        default:
            EMSG("Unknown opcode: 0x%02X", opcode);
            return TEE_ERROR_NOT_SUPPORTED;
        }
    }

    EMSG("No RETURN opcode found");
    return TEE_ERROR_BAD_FORMAT;
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

    case TA_MINIMAL_EVM_CMD_EXECUTE_BYTECODE:
        return execute_bytecode(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_VECTOR:
        return test_real_vector(param_types, params);

    // case TA_MINIMAL_EVM_CMD_TEST_MAP:
    //     return test_real_map(param_types, params);

    // case TA_MINIMAL_EVM_CMD_TEST_EXCEPTION:
    //     return test_exceptions(param_types, params);

    default:
        EMSG("Unknown command ID: %u", cmd_id);
        return TEE_ERROR_BAD_PARAMETERS;
    }
}
