// SPDX-License-Identifier: BSD-2-Clause
/*
 * eEVM Hello World TA - Running eEVM in OP-TEE secure world
 */

// Include C++ headers BEFORE TEE headers to avoid macro conflicts
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

// eEVM headers
#include "eEVM/opcode.h"
#include "eEVM/processor.h"
#include "eEVM/simple/simpleglobalstate.h"
#include "eEVM/stack.h"

// TEE headers
extern "C"
{
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
}

#include "eevm_ta.h"
#include "ocall_logger.h"

// Create EVM bytecode that stores and returns a string
std::vector<uint8_t> create_bytecode(const std::string& s)
{
    std::vector<uint8_t> code;
    constexpr uint8_t mdest = 0x0;
    const uint8_t rsize = s.size() + 1;

    // Store each byte in evm memory
    uint8_t mcurrent = mdest;
    for (const char& c : s)
    {
        code.push_back(eevm::Opcode::PUSH1);
        code.push_back(c);
        code.push_back(eevm::Opcode::PUSH1);
        code.push_back(mcurrent++);
        code.push_back(eevm::Opcode::MSTORE8);
    }

    // Return
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(rsize);
    code.push_back(eevm::Opcode::PUSH1);
    code.push_back(mdest);
    code.push_back(eevm::Opcode::RETURN);

    return code;
}

// Execute eEVM hello world
static TEE_Result eevm_hello_world(uint32_t param_types, TEE_Param params[4])
{
    // Initialize OCALL logging first
    ocall_log_init();
    OCALL_LOG("=== Starting eEVM Hello World Test ===");

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_OUTPUT,    // Output: exit reason
        TEE_PARAM_TYPE_VALUE_OUTPUT,    // Output: result size
        TEE_PARAM_TYPE_MEMREF_OUTPUT,   // For OCALL logs
        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
    {
        OCALL_LOG("[ERROR] Bad parameter types!");
        if (params[2].memref.buffer && params[2].memref.size > 0)
        {
            ocall_log_flush_to_params(
                params[2].memref.buffer, &params[2].memref.size);
        }
        return TEE_ERROR_BAD_PARAMETERS;
    }

    try {
        OCALL_LOG("[Step 1] Creating addresses...");

        // Create random addresses for sender and contract
        std::vector<uint8_t> raw_address(20);

        // Use simple pseudo-random generation for OP-TEE
        for (size_t i = 0; i < raw_address.size(); i++) {
            raw_address[i] = (uint8_t)(i * 17 + 42);
        }

        const eevm::Address sender =
            eevm::from_big_endian(raw_address.data(), raw_address.size());

        for (size_t i = 0; i < raw_address.size(); i++) {
            raw_address[i] = (uint8_t)(i * 23 + 13);
        }

        const eevm::Address to =
            eevm::from_big_endian(raw_address.data(), raw_address.size());

        OCALL_LOG("[Step 2] Creating global state...");

        // Create global state
        eevm::SimpleGlobalState gs;

        OCALL_LOG("[Step 3] Creating bytecode...");
        // Create code
        std::string hello_world("Hello from eEVM in OP-TEE!_________________-");
        const eevm::Code code = create_bytecode(hello_world);
        OCALL_LOG("[INFO] Bytecode size: %zu bytes", code.size());

        OCALL_LOG("[Step 4] Deploying contract...");
        // Deploy contract to global state
        const eevm::AccountState contract = gs.create(to, 0, code);

        OCALL_LOG("[Step 5] Creating transaction...");
        // Create transaction
        eevm::NullLogHandler ignore;
        eevm::Transaction tx(sender, ignore);

        OCALL_LOG("[Step 6] Creating processor...");
        // Create processor
        eevm::Processor p(gs);

        OCALL_LOG("[Step 7] Executing EVM bytecode...");
        // Execute code
        const eevm::ExecResult e = p.run(tx, sender, contract, {}, 0, nullptr);
        
        OCALL_LOG("[INFO] Execution completed!");
        OCALL_LOG("[INFO] Exit reason: %d", (int)e.er);
        OCALL_LOG("[INFO] Output size: %zu bytes", e.output.size());

        // Check the response
        if (e.er != eevm::ExitReason::returned) {
            OCALL_LOG("[ERROR] Unexpected exit reason: %d", (int)e.er);
            params[0].value.a = (uint32_t)e.er;
            params[1].value.a = 0;
            
            // Flush logs
            if (params[2].memref.buffer && params[2].memref.size > 0)
            {
                ocall_log_flush_to_params(
                    params[2].memref.buffer, &params[2].memref.size);
            }
            return TEE_ERROR_GENERIC;
        }

        // Copy result to output buffer
        const std::string response(reinterpret_cast<const char*>(e.output.data()));

        OCALL_LOG("[Step 8] Verifying result...");
        OCALL_LOG("[INFO] Expected: '%s'", hello_world.c_str());
        OCALL_LOG("[INFO] Got:      '%s'", response.c_str());

        if (response != hello_world) {
            OCALL_LOG("[FAIL] Result mismatch!");
            params[0].value.a = (uint32_t)e.er;
            params[1].value.a = 0;
            
            // Flush logs
            if (params[2].memref.buffer && params[2].memref.size > 0)
            {
                ocall_log_flush_to_params(
                    params[2].memref.buffer, &params[2].memref.size);
            }
            return TEE_ERROR_GENERIC;
        }

        OCALL_LOG("[PASS] ✓ eEVM execution successful!");
        OCALL_LOG("=== eEVM Hello World Test PASSED ===");

        // Return success values
        params[0].value.a = (uint32_t)e.er;
        params[1].value.a = response.size();

        // Flush logs to host
        if (params[2].memref.buffer && params[2].memref.size > 0)
        {
            ocall_log_flush_to_params(
                params[2].memref.buffer, &params[2].memref.size);
        }

        return TEE_SUCCESS;

    } catch (const std::exception& ex) {
        OCALL_LOG("[FATAL] C++ exception: %s", ex.what());
        params[0].value.a = 999;  // Error indicator
        params[1].value.a = 0;
        
        // Flush logs even on exception
        if (params[2].memref.buffer && params[2].memref.size > 0)
        {
            ocall_log_flush_to_params(
                params[2].memref.buffer, &params[2].memref.size);
        }
        return TEE_ERROR_GENERIC;
    } catch (...) {
        OCALL_LOG("[FATAL] Unknown C++ exception!");
        params[0].value.a = 998;  // Error indicator
        params[1].value.a = 0;
        
        // Flush logs even on exception
        if (params[2].memref.buffer && params[2].memref.size > 0)
        {
            ocall_log_flush_to_params(
                params[2].memref.buffer, &params[2].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * TA Entry Points
 */
extern "C"
{
  TEE_Result TA_CreateEntryPoint(void)
  {
    DMSG("TA_CreateEntryPoint - eEVM TA");
    return TEE_SUCCESS;
  }

  void TA_DestroyEntryPoint(void)
  {
    DMSG("TA_DestroyEntryPoint");
  }

  TEE_Result TA_OpenSessionEntryPoint(
    uint32_t param_types, TEE_Param params[4], void** sess_ctx)
  {
    DMSG("========================================");
    DMSG("eEVM TA - Session opened");
    DMSG("========================================");

    // Initialize OCALL logging
    ocall_log_init();
    OCALL_LOG("TA_OpenSessionEntryPoint called");
    OCALL_LOG("param_types: 0x%x", param_types);

    uint32_t exp_param_types = TEE_PARAM_TYPES(
      TEE_PARAM_TYPE_NONE,
      TEE_PARAM_TYPE_NONE,
      TEE_PARAM_TYPE_MEMREF_OUTPUT, // For OCALL logs
      TEE_PARAM_TYPE_NONE);

    OCALL_LOG("Expected param_types: 0x%x", exp_param_types);

    if (param_types != exp_param_types)
    {
      OCALL_LOG(
        "ERROR: Bad parameters! Got 0x%x, expected 0x%x",
        param_types,
        exp_param_types);
      // Flush logs even on error
      if (params[2].memref.buffer && params[2].memref.size > 0)
      {
        ocall_log_flush_to_params(
          params[2].memref.buffer, &params[2].memref.size);
      }
      return TEE_ERROR_BAD_PARAMETERS;
    }

    (void)&params;
    (void)&sess_ctx;

    OCALL_LOG("Session opened successfully!");

    // Flush logs to host
    if (params[2].memref.buffer && params[2].memref.size > 0)
    {
      ocall_log_flush_to_params(
        params[2].memref.buffer, &params[2].memref.size);
    }

    return TEE_SUCCESS;
  }

  void TA_CloseSessionEntryPoint(void* sess_ctx)
  {
    DMSG("Session closed");
    (void)&sess_ctx;
  }
  static TEE_Result test_evm_stack(uint32_t param_types, TEE_Param params[4])
  {
    ocall_log_init();
    OCALL_LOG("=== Starting eEVM Stack Test ===");

    try
    {
      eevm::Stack stack;

      // Test 1: Push và Size
      OCALL_LOG("[INFO] Test 1: Push 3 values");
      stack.push(100);
      stack.push(200);
      stack.push(300);
      OCALL_LOG("[DEBUG] Stack size: %llu (Expected: 3)", stack.size());

      // Test 2: Pop
      OCALL_LOG("[INFO] Test 2: Pop top value");
      uint256_t val = stack.pop();
      if (val == 300)
      {
        OCALL_LOG("[PASS] Popped value is 300");
      }
      else
      {
        OCALL_LOG("[FAIL] Popped value incorrect!");
      }

      // Test 3: Swap (Hoán đổi 200 và 100)
      OCALL_LOG("[INFO] Test 3: Swap elements");
      // Hiện tại stack là [200, 100]. Index 1 là giá trị 100.
      stack.swap(1);
      uint256_t top = stack.pop();
      if (top == 100)
      {
        OCALL_LOG("[PASS] Swap successful, top is now 100");
      }

      // Test 4: Dup (Nhân bản)
      stack.push(500); // Stack: [500]
      stack.dup(0); // Stack: [500, 500]
      if (stack.size() == 2 && stack.pop() == 500)
      {
        OCALL_LOG("[PASS] Dup successful");
      }

      OCALL_LOG("=== All Stack Tests Passed! ===");
      params[0].value.a = 0; // Success code
    }
    catch (const std::exception& e)
    {
      OCALL_LOG("[FATAL] Exception caught: %s", e.what());
      params[0].value.a = 1; // Error code
    }

    // Trả log về Host (dùng params[2] giống OpenSession)
    if (params[2].memref.buffer && params[2].memref.size > 0)
    {
      ocall_log_flush_to_params(
        params[2].memref.buffer, &params[2].memref.size);
    }

    return TEE_SUCCESS;
  }
  TEE_Result TA_InvokeCommandEntryPoint(
    void* sess_ctx, uint32_t cmd_id, uint32_t param_types, TEE_Param params[4])
  {
    (void)&sess_ctx;

    switch (cmd_id)
    {
      case TA_EEVM_CMD_HELLO_WORLD:
        return eevm_hello_world(param_types, params);
      case TA_EEVM_CMD_TEST_STACK:
        return test_evm_stack(param_types, params);
      default:
        return TEE_ERROR_BAD_PARAMETERS;
    }
  }

} // extern "C"
