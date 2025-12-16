// SPDX-License-Identifier: BSD-2-Clause
/*
 * Minimal EVM TA - Testing REAL std::vector from libcxx
 * Using libcxx from OpenEnclave/LLVM
 */

// C++ STL headers (from libcxx with FULL musl runtime!)
// MUST be included BEFORE TEE headers to avoid __section macro conflicts
// Note: NULL redefinition warning is fixed by patching musl headers
#include <vector>
#include <algorithm> // For std::find, std::sort
#include <map>
#include <set>
#include <deque>
#include <list>
#include <forward_list>
#include <queue>
#include <stack>
// REMOVED: unordered_map, unordered_set (needs cmath which conflicts with musl)
#include <array>
#include <optional>
#include <variant>
// REMOVED: any (needs RTTI - typeinfo for built-in types)
#include <tuple>
// REMOVED: functional (std::function needs RTTI for vtables)
// REMOVED: numeric (includes cmath which conflicts with musl)
#include <memory>
#include <utility>
#include <exception>
// Include compatibility header to undefine conflicting macros
#include "libcxx_compat.h"
// Include TEE headers AFTER libcxx to avoid macro conflicts
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
}

/*
 * Test REAL std::map - NO MORE CUSTOM IMPLEMENTATION!
 */
static TEE_Result test_real_map(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    DMSG("========================================");
    DMSG("Testing REAL std::map from libcxx");
    DMSG("========================================");

    // try
    // {
    // Create std::map - this is THE REAL STL!
    std::map<int, int> mymap;

    DMSG("Test 1: Insert key-value pairs");
    mymap[1] = 100;
    mymap[2] = 200;
    mymap[3] = 300;
    mymap[5] = 500;
    mymap[8] = 800;

    DMSG("Map size: %zu", mymap.size());

    // Test 2: Iterate through map
    DMSG("\nTest 2: Iterate through map");
    for (const auto &pair : mymap)
    {
        DMSG("  Key: %d -> Value: %d", pair.first, pair.second);
    }

    // Test 3: Find operation
    DMSG("\nTest 3: Find key 5");
    auto it = mymap.find(5);
    if (it != mymap.end())
    {
        DMSG("  Found: key=%d, value=%d", it->first, it->second);
    }

    // Test 4: Count operation
    DMSG("\nTest 4: Count occurrences");
    DMSG("  Count of key 3: %zu", mymap.count(3));
    DMSG("  Count of key 99: %zu", mymap.count(99));

    // Test 5: Erase operation
    DMSG("\nTest 5: Erase key 2");
    mymap.erase(2);
    DMSG("  After erase, size: %zu", mymap.size());

    // Test 6: Clear
    mymap.clear();
    DMSG("\nTest 6: After clear, size: %zu", mymap.size());
    DMSG("  Is empty: %s", mymap.empty() ? "yes" : "no");

    params[0].value.a = 0; // Success

    DMSG("========================================");
    DMSG("std::map TEST PASSED!");
    DMSG("========================================");

    return TEE_SUCCESS;
    // }
    // // catch (const std::exception &e)
    // // {
    // //     EMSG("Exception caught: %s", e.what());
    // //     return TEE_ERROR_GENERIC;
    // // }
    // catch (...)
    // {
    //     EMSG("Unknown exception caught!");
    //     return TEE_ERROR_GENERIC;
    // }
}

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
 * ============================================
 * C++17 LIBCXX FEATURE TESTS - OpenEnclave
 * Return 0=PASS in params[0].value.a
 * ============================================
 */

// Test 1: std::optional (C++17)
static TEE_Result test_optional(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::optional<int> opt1;
    std::optional<int> opt2 = 42;

    if (opt1.has_value())
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }
    if (!opt2.has_value())
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }
    if (opt2.value() != 42)
    {
        params[0].value.a = 3;
        return TEE_ERROR_GENERIC;
    }

    opt1 = 100;
    if (opt1.value() != 100)
    {
        params[0].value.a = 4;
        return TEE_ERROR_GENERIC;
    }

    opt1.reset();
    if (opt1.has_value())
    {
        params[0].value.a = 5;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 2: std::variant (C++17)
static TEE_Result test_variant(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::variant<int, long, double> var1;
    var1 = 42;
    if (std::get<int>(var1) != 42)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }

    var1 = 3.14;
    if (std::get<double>(var1) != 3.14)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }

    if (var1.index() != 2)
    {
        params[0].value.a = 3;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 3: std::any - REMOVED (needs RTTI)

// Test 4: std::tuple
static TEE_Result test_tuple(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::tuple<int, double, int> t1(10, 3.14, 20);

    if (std::get<0>(t1) != 10)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }
    if (std::get<1>(t1) != 3.14)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }
    if (std::get<2>(t1) != 20)
    {
        params[0].value.a = 3;
        return TEE_ERROR_GENERIC;
    }

    auto t2 = std::make_tuple(42, 2.71, 100);
    if (std::get<0>(t2) != 42)
    {
        params[0].value.a = 4;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 5: Lambda expressions (C++11) - std::function removed (needs RTTI)
static TEE_Result test_functional(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    // Lambda expressions work without std::function
    auto add = [](int a, int b)
    { return a + b; };
    if (add(10, 20) != 30)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }

    auto multiply = [](int a, int b)
    { return a * b; };
    if (multiply(5, 6) != 30)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 6: std::algorithm (find, count - no sort due to missing algorithm.cpp)
static TEE_Result test_algorithm(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::vector<int> vec = {5, 2, 8, 1, 9};

    // std::find
    auto it = std::find(vec.begin(), vec.end(), 8);
    if (it == vec.end() || *it != 8)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }

    // std::count
    vec.push_back(8);
    if (std::count(vec.begin(), vec.end(), 8) != 2)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 7: std::numeric - REMOVED (includes cmath which conflicts with musl)
/*
static TEE_Result test_numeric(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::vector<int> vec = {1, 2, 3, 4, 5};

    // std::accumulate
    int sum = std::accumulate(vec.begin(), vec.end(), 0);
    if (sum != 15) { params[0].value.a = 1; return TEE_ERROR_GENERIC; }

    // std::inner_product
    int product = std::inner_product(vec.begin(), vec.end(), vec.begin(), 0);
    if (product != 55) { params[0].value.a = 2; return TEE_ERROR_GENERIC; }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}
*/

// Test 8: std::memory - REMOVED (shared_ptr needs RTTI)

// Test 9: std::deque
static TEE_Result test_deque(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::deque<int> dq;
    dq.push_back(1);
    dq.push_front(0);
    dq.push_back(2);

    if (dq.size() != 3)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }
    if (dq[0] != 0 || dq[1] != 1 || dq[2] != 2)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }

    dq.pop_front();
    if (dq.front() != 1)
    {
        params[0].value.a = 3;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 10: std::list
static TEE_Result test_list(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_front(0);

    if (lst.size() != 3)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }
    if (lst.front() != 0)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }
    if (lst.back() != 2)
    {
        params[0].value.a = 3;
        return TEE_ERROR_GENERIC;
    }

    lst.remove(1);
    if (lst.size() != 2)
    {
        params[0].value.a = 4;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 11: std::set
static TEE_Result test_set(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::set<int> s;
    s.insert(5);
    s.insert(2);
    s.insert(8);
    s.insert(2); // duplicate

    if (s.size() != 3)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }
    if (s.count(2) != 1)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }
    if (s.count(99) != 0)
    {
        params[0].value.a = 3;
        return TEE_ERROR_GENERIC;
    }

    auto it = s.find(5);
    if (it == s.end() || *it != 5)
    {
        params[0].value.a = 4;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 12: std::unordered_map - REMOVED (conflicts with cmath/musl)
/*
static TEE_Result test_unordered_map(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::unordered_map<int, int> umap;
    umap[10] = 100;
    umap[20] = 200;
    umap[30] = 300;

    if (umap.size() != 3) { params[0].value.a = 1; return TEE_ERROR_GENERIC; }
    if (umap[10] != 100) { params[0].value.a = 2; return TEE_ERROR_GENERIC; }

    auto it = umap.find(20);
    if (it == umap.end() || it->second != 200) { params[0].value.a = 3; return TEE_ERROR_GENERIC; }

    umap.erase(10);
    if (umap.size() != 2) { params[0].value.a = 4; return TEE_ERROR_GENERIC; }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}
*/

// Test 13: std::queue
static TEE_Result test_queue(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::queue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);

    if (q.size() != 3)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }
    if (q.front() != 1)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }
    if (q.back() != 3)
    {
        params[0].value.a = 3;
        return TEE_ERROR_GENERIC;
    }

    q.pop();
    if (q.front() != 2)
    {
        params[0].value.a = 4;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 14: std::exception - REMOVED (needs -fexceptions)

// Test 15: std::array (C++11)
static TEE_Result test_array(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::array<int, 5> arr = {1, 2, 3, 4, 5};

    if (arr.size() != 5)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }
    if (arr[0] != 1)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }
    if (arr.front() != 1)
    {
        params[0].value.a = 3;
        return TEE_ERROR_GENERIC;
    }
    if (arr.back() != 5)
    {
        params[0].value.a = 4;
        return TEE_ERROR_GENERIC;
    }

    arr.fill(10);
    if (arr[0] != 10 || arr[4] != 10)
    {
        params[0].value.a = 5;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
    return TEE_SUCCESS;
}

// Test 16: std::forward_list (C++11)
static TEE_Result test_forward_list(uint32_t param_types, TEE_Param params[4])
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE,
                                               TEE_PARAM_TYPE_NONE);
    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    std::forward_list<int> flist;
    flist.push_front(3);
    flist.push_front(2);
    flist.push_front(1);

    if (flist.front() != 1)
    {
        params[0].value.a = 1;
        return TEE_ERROR_GENERIC;
    }

    int count = 0;
    for (auto val : flist)
    {
        count++;
        (void)val;
    }
    if (count != 3)
    {
        params[0].value.a = 2;
        return TEE_ERROR_GENERIC;
    }

    params[0].value.a = 0; // PASS
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
    case TA_MINIMAL_EVM_CMD_EXECUTE_BYTECODE:
        return execute_bytecode(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_VECTOR:
        return test_real_vector(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_MAP:
        return test_real_map(param_types, params);

    // C++17 libcxx feature tests
    case TA_MINIMAL_EVM_CMD_TEST_OPTIONAL:
        return test_optional(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_VARIANT:
        return test_variant(param_types, params);
    // NOTE: any test removed - needs RTTI
    // case TA_MINIMAL_EVM_CMD_TEST_ANY:
    //     return test_any(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_TUPLE:
        return test_tuple(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_FUNCTIONAL:
        return test_functional(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_ALGORITHM:
        return test_algorithm(param_types, params);
    // NOTE: numeric removed - includes cmath
    // case TA_MINIMAL_EVM_CMD_TEST_NUMERIC:
    //     return test_numeric(param_types, params);
    // NOTE: memory test removed - shared_ptr needs RTTI
    // case TA_MINIMAL_EVM_CMD_TEST_MEMORY:
    //     return test_memory(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_DEQUE:
        return test_deque(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_LIST:
        return test_list(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_SET:
        return test_set(param_types, params);
    // NOTE: unordered_map removed - conflicts with cmath
    // case TA_MINIMAL_EVM_CMD_TEST_UNORDERED_MAP:
    //     return test_unordered_map(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_QUEUE:
        return test_queue(param_types, params);
    // NOTE: Exception test removed - requires -fexceptions
    case TA_MINIMAL_EVM_CMD_TEST_ARRAY:
        return test_array(param_types, params);
    case TA_MINIMAL_EVM_CMD_TEST_FORWARD_LIST:
        return test_forward_list(param_types, params);

    default:
        EMSG("Unknown command ID: %u", cmd_id);
        return TEE_ERROR_BAD_PARAMETERS;
    }
}
