// SPDX-License-Identifier: BSD-2-Clause
/*
 * LevelDB with Simple Buffer Accumulation - OP-TEE Trusted Application
 * 
 * STRATEGY: Based on OCALL implementation experience
 * - Problem: Persistent shared memory causes 0xFFFF3024 (TEE_ERROR_TARGET_DEAD)
 * - Solution: Simple Buffer Accumulation
 *   1. TA accumulates data writes in local buffer during command execution
 *   2. At command completion, copy buffer to temp params
 *   3. Host receives buffer and saves to LevelDB in normal world
 * 
 * This avoids the persistent shared memory issue that causes crashes.
 */

// Include C++ headers BEFORE TEE headers
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <cstring>

// TEE headers
extern "C"
{
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
}

#include "eevm_ta.h"
#include "ocall_logger.h"

// =====================================================================
// SIMPLE BUFFER ACCUMULATION - Similar to OCALL Pattern
// =====================================================================

#define DATA_BUFFER_SIZE (128 * 1024)  // 128KB for accumulated data operations

// Structure for data operation
struct DataOperation {
    uint8_t op_type;      // 0=PUT, 1=DELETE
    uint16_t key_len;
    uint16_t value_len;
    // Followed by: key_data, value_data
};

// Global buffer for accumulating data operations
static uint8_t g_data_buffer[DATA_BUFFER_SIZE];
static size_t g_data_buffer_pos = 0;

// In-memory key-value store for testing (since we can't use LevelDB persistently)
struct KeyValue {
    std::string key;
    std::string value;
};
static std::vector<KeyValue> g_kv_store;

/*
 * Append a data operation to the accumulation buffer
 */
static bool append_data_operation(uint8_t op_type, const char* key, size_t key_len,
                                   const char* value, size_t value_len)
{
    size_t needed = sizeof(DataOperation) + key_len + value_len;
    
    if (g_data_buffer_pos + needed > DATA_BUFFER_SIZE) {
        OCALL_LOG("[ERROR] Data buffer full! pos=%zu, needed=%zu", g_data_buffer_pos, needed);
        return false;
    }
    
    DataOperation* op = reinterpret_cast<DataOperation*>(g_data_buffer + g_data_buffer_pos);
    op->op_type = op_type;
    op->key_len = static_cast<uint16_t>(key_len);
    op->value_len = static_cast<uint16_t>(value_len);
    
    uint8_t* data_ptr = g_data_buffer + g_data_buffer_pos + sizeof(DataOperation);
    
    // Copy key
    memcpy(data_ptr, key, key_len);
    data_ptr += key_len;
    
    // Copy value (if any)
    if (value_len > 0) {
        memcpy(data_ptr, value, value_len);
    }
    
    g_data_buffer_pos += needed;
    
    OCALL_LOG("[DEBUG] Appended %s operation: key_len=%zu, value_len=%zu, buffer_pos=%zu",
              op_type == 0 ? "PUT" : "DELETE", key_len, value_len, g_data_buffer_pos);
    
    return true;
}

/*
 * Initialize the Simple Buffer system
 * 
 * params[0] = VALUE_INPUT   - Init flag (not used, kept for compatibility)
 * params[1] = MEMREF_OUTPUT - OCALL logs
 */
static TEE_Result simple_buffer_init(uint32_t param_types, TEE_Param params[4])
{
    ocall_log_init();
    OCALL_LOG("=== Initializing Simple Buffer System ===");

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,     // Init flag
        TEE_PARAM_TYPE_MEMREF_OUTPUT,   // OCALL logs
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
    {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    // Reset buffer
    g_data_buffer_pos = 0;
    
    // Clear in-memory store
    g_kv_store.clear();
    
    OCALL_LOG("[INFO] Simple Buffer initialized successfully!");
    OCALL_LOG("[INFO] - Data buffer size: %d KB", DATA_BUFFER_SIZE / 1024);
    OCALL_LOG("[INFO] - OCALL log buffer: %d KB", OCALL_BUFFER_SIZE / 1024);
    
    // Flush logs
    if (params[1].memref.buffer && params[1].memref.size > 0) {
        ocall_log_flush_to_params(params[1].memref.buffer, &params[1].memref.size);
    }

    return TEE_SUCCESS;
}

/*
 * Put a key-value pair
 * - Stores in TA's in-memory store
 * - Accumulates operation in buffer for host to replicate
 * 
 * params[0] = MEMREF_INPUT  - Key
 * params[1] = MEMREF_INPUT  - Value
 * params[2] = VALUE_OUTPUT  - Status code
 * params[3] = MEMREF_OUTPUT - Data operations buffer
 * params[4] = MEMREF_OUTPUT - OCALL logs (optional)
 */
static TEE_Result simple_buffer_put(uint32_t param_types, TEE_Param params[4])
{
    OCALL_LOG("=== Simple Buffer PUT Operation ===");

    // Check parameters - allow both with and without logs
    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    uint32_t p2 = TEE_PARAM_TYPE_GET(param_types, 2);
    uint32_t p3 = TEE_PARAM_TYPE_GET(param_types, 3);
    
    bool has_logs = (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT);
    
    OCALL_LOG("[DEBUG] Param types: p0=%d, p1=%d, p2=%d, p3=%d", p0, p1, p2, p3);
    
    if (p0 != TEE_PARAM_TYPE_MEMREF_INPUT || 
        p1 != TEE_PARAM_TYPE_MEMREF_INPUT ||
        p2 != TEE_PARAM_TYPE_VALUE_OUTPUT) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        if (has_logs && params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_BAD_PARAMETERS;
    }

    // Validate buffers
    if (!params[0].memref.buffer || params[0].memref.size == 0) {
        OCALL_LOG("[ERROR] Invalid key buffer!");
        if (has_logs) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_BAD_PARAMETERS;
    }

    if (!params[1].memref.buffer || params[1].memref.size == 0) {
        OCALL_LOG("[ERROR] Invalid value buffer!");
        if (has_logs) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_BAD_PARAMETERS;
    }

    try {
        // Reset buffer for this operation
        g_data_buffer_pos = 0;
        
        const char* key_ptr = static_cast<const char*>(params[0].memref.buffer);
        size_t key_size = params[0].memref.size;
        const char* val_ptr = static_cast<const char*>(params[1].memref.buffer);
        size_t val_size = params[1].memref.size;
        
        std::string key(key_ptr, key_size);
        std::string value(val_ptr, val_size);

        OCALL_LOG("[INFO] Putting key='%s', value size=%zu", key.c_str(), value.size());

        // 1. Store in TA's in-memory store
        bool found = false;
        for (auto& kv : g_kv_store) {
            if (kv.key == key) {
                kv.value = value;
                found = true;
                OCALL_LOG("[DEBUG] Updated existing key in memory");
                break;
            }
        }
        
        if (!found) {
            g_kv_store.push_back({key, value});
            OCALL_LOG("[DEBUG] Added new key to memory, total keys=%zu", g_kv_store.size());
        }

        // 2. Append operation to buffer for host replication
        if (!append_data_operation(0, key_ptr, key_size, val_ptr, val_size)) {
            OCALL_LOG("[ERROR] Failed to append data operation");
            params[2].value.a = 2;  // Buffer full
        } else {
            OCALL_LOG("[INFO] PUT successful! Buffer pos=%zu", g_data_buffer_pos);
            params[2].value.a = 0;  // Success
        }

        // 3. Copy accumulated data buffer to host (THIS IS THE KEY!)
        // Similar to OCALL pattern - return buffer at command completion
        if (params[2].memref.buffer && params[2].memref.size >= g_data_buffer_pos) {
            memcpy(params[2].memref.buffer, g_data_buffer, g_data_buffer_pos);
            params[2].memref.size = g_data_buffer_pos;
            OCALL_LOG("[DEBUG] Copied %zu bytes to host data buffer", g_data_buffer_pos);
        } else {
            OCALL_LOG("[WARN] Host data buffer too small: have=%zu, need=%zu",
                      params[2].memref.size, g_data_buffer_pos);
        }

        // 4. Flush logs if requested
        if (has_logs && params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }

        return TEE_SUCCESS;

    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[2].value.a = 3;  // Exception
        
        if (has_logs && params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Get a value by key from TA's in-memory store
 * 
 * params[0] = MEMREF_INPUT  - Key
 * params[1] = MEMREF_OUTPUT - Value
 * params[2] = VALUE_OUTPUT  - Status code (0=found, 1=not found, 2=error)
 * params[3] = MEMREF_OUTPUT - OCALL logs (optional)
 */
static TEE_Result simple_buffer_get(uint32_t param_types, TEE_Param params[4])
{
    OCALL_LOG("=== Simple Buffer GET Operation ===");

    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    uint32_t p2 = TEE_PARAM_TYPE_GET(param_types, 2);
    uint32_t p3 = TEE_PARAM_TYPE_GET(param_types, 3);
    
    bool has_logs = (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT);
    
    if (p0 != TEE_PARAM_TYPE_MEMREF_INPUT || 
        p1 != TEE_PARAM_TYPE_MEMREF_OUTPUT ||
        p2 != TEE_PARAM_TYPE_VALUE_OUTPUT) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        if (has_logs) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_BAD_PARAMETERS;
    }

    try {
        std::string key(static_cast<const char*>(params[0].memref.buffer), params[0].memref.size);

        OCALL_LOG("[INFO] Getting key='%s' from %zu stored keys", key.c_str(), g_kv_store.size());

        // Search in TA's in-memory store
        for (const auto& kv : g_kv_store) {
            if (kv.key == key) {
                OCALL_LOG("[INFO] Key found! Value size=%zu", kv.value.size());
                
                size_t copy_size = std::min(kv.value.size(), params[1].memref.size);
                memcpy(params[1].memref.buffer, kv.value.data(), copy_size);
                params[1].memref.size = copy_size;
                params[2].value.a = 0;  // Found
                
                if (has_logs) {
                    ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
                }
                return TEE_SUCCESS;
            }
        }

        // Not found
        OCALL_LOG("[INFO] Key not found");
        params[1].memref.size = 0;
        params[2].value.a = 1;  // Not found

        if (has_logs) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_SUCCESS;

    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[2].value.a = 2;  // Error
        
        if (has_logs) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Delete a key
 * 
 * params[0] = MEMREF_INPUT  - Key
 * params[1] = VALUE_OUTPUT  - Status
 * params[2] = MEMREF_OUTPUT - Data operations buffer
 * params[3] = MEMREF_OUTPUT - OCALL logs (optional)
 */
static TEE_Result simple_buffer_delete(uint32_t param_types, TEE_Param params[4])
{
    OCALL_LOG("=== Simple Buffer DELETE Operation ===");

    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    uint32_t p2 = TEE_PARAM_TYPE_GET(param_types, 2);
    uint32_t p3 = TEE_PARAM_TYPE_GET(param_types, 3);
    
    bool has_logs = (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT);

    if (p0 != TEE_PARAM_TYPE_MEMREF_INPUT || 
        p1 != TEE_PARAM_TYPE_VALUE_OUTPUT ||
        p2 != TEE_PARAM_TYPE_MEMREF_OUTPUT) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    try {
        // Reset buffer
        g_data_buffer_pos = 0;
        
        const char* key_ptr = static_cast<const char*>(params[0].memref.buffer);
        size_t key_size = params[0].memref.size;
        std::string key(key_ptr, key_size);

        OCALL_LOG("[INFO] Deleting key='%s'", key.c_str());

        // 1. Delete from TA's in-memory store
        auto it = std::remove_if(g_kv_store.begin(), g_kv_store.end(),
                                  [&key](const KeyValue& kv) { return kv.key == key; });
        
        bool found = (it != g_kv_store.end());
        g_kv_store.erase(it, g_kv_store.end());
        
        if (found) {
            OCALL_LOG("[DEBUG] Deleted from memory, remaining keys=%zu", g_kv_store.size());
        } else {
            OCALL_LOG("[WARN] Key not found in memory");
        }

        // 2. Append delete operation to buffer
        if (!append_data_operation(1, key_ptr, key_size, nullptr, 0)) {
            OCALL_LOG("[ERROR] Failed to append delete operation");
            params[1].value.a = 2;
        } else {
            params[1].value.a = 0;
        }

        // 3. Copy data buffer to host
        if (params[2].memref.buffer && params[2].memref.size >= g_data_buffer_pos) {
            memcpy(params[2].memref.buffer, g_data_buffer, g_data_buffer_pos);
            params[2].memref.size = g_data_buffer_pos;
        }

        // 4. Flush logs
        if (has_logs && params[3].memref.buffer) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }

        return TEE_SUCCESS;

    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[1].value.a = 2;
        
        if (has_logs) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

// =====================================================================
// COMMENTED OUT: LevelDB with Ring Buffer (causes 0xFFFF3024)
// =====================================================================
/*
// LevelDB headers - COMMENTED OUT
// #include <leveldb/db.h>
// #include <leveldb/write_batch.h>
// #include <leveldb/options.h>

// Ring buffer headers - COMMENTED OUT
// #include "secure_ring_buffer_producer.hpp"
// #include "leveldb_ring_buffer_env_complete.hpp"

// Global variables - COMMENTED OUT
// static std::unique_ptr<leveldb::DB> g_db;
// static std::unique_ptr<SecureRingBufferProducer> g_producer;
// static std::unique_ptr<RingBufferEnv> g_env;
// static void* g_shared_mem = nullptr;

// REASON FOR COMMENTING OUT:
// Using persistent shared memory with ring buffer causes TEE_ERROR_TARGET_DEAD (0xFFFF3024)
// This is the same issue encountered in OCALL implementation.
// 
// ROOT CAUSE:
// - OP-TEE unmaps shared memory after command returns
// - Attempting to access it from another command or thread causes crash
// - Ring buffer requires persistent memory mapping across commands
// 
// SOLUTION:
// Use Simple Buffer Accumulation pattern instead:
// 1. Accumulate operations in TA-local buffer during command
// 2. Return buffer to host at command completion
// 3. Host saves to LevelDB in normal world
// 
// This matches the successful OCALL logging pattern from OCALL_IMPLEMENTATION.md
*/

// =====================================================================
// TA Entry Points
// =====================================================================

TEE_Result TA_CreateEntryPoint(void)
{
    DMSG("has been called");
    ocall_log_init();
    return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
    DMSG("has been called");
    g_kv_store.clear();
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
                                     TEE_Param __maybe_unused params[4],
                                     void __maybe_unused **sess_ctx)
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    IMSG("Session opened!");
    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
    IMSG("Session closed");
}

TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
                                       uint32_t cmd_id,
                                       uint32_t param_types,
                                       TEE_Param params[4])
{
    switch (cmd_id)
    {
    case TA_EEVM_CMD_INIT_LEVELDB:
        return simple_buffer_init(param_types, params);
        
    case TA_EEVM_CMD_LEVELDB_PUT:
        return simple_buffer_put(param_types, params);
        
    case TA_EEVM_CMD_LEVELDB_GET:
        return simple_buffer_get(param_types, params);
        
    case TA_EEVM_CMD_LEVELDB_DELETE:
        return simple_buffer_delete(param_types, params);

    default:
        return TEE_ERROR_BAD_PARAMETERS;
    }
}
