// SPDX-License-Identifier: BSD-2-Clause
/*
 * LevelDB with Ring Buffer - OP-TEE Trusted Application
 */

// Include C++ headers BEFORE TEE headers
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>

// LevelDB headers
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <leveldb/options.h>

// TEE headers
extern "C"
{
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
}

#include "eevm_ta.h"
#include "ocall_logger.h"
#include "secure_ring_buffer_producer.hpp"
#include "leveldb_ring_buffer_env.hpp"

// Global variables
static std::unique_ptr<leveldb::DB> g_db;
static std::unique_ptr<SecureRingBufferProducer> g_producer;
static std::unique_ptr<RingBufferEnv> g_env;
static void* g_shared_mem = nullptr;

/*
 * Initialize LevelDB with Ring Buffer
 * 
 * params[0] = MEMREF_INOUT - Shared memory for Ring Buffer
 * params[1] = VALUE_INPUT  - Buffer size
 * params[2] = MEMREF_OUTPUT - OCALL logs
 */
static TEE_Result leveldb_init(uint32_t param_types, TEE_Param params[4])
{
    ocall_log_init();
    OCALL_LOG("=== Initializing LevelDB with Ring Buffer ===");

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INOUT,    // Shared memory
        TEE_PARAM_TYPE_VALUE_INPUT,     // Buffer size
        TEE_PARAM_TYPE_MEMREF_OUTPUT,   // OCALL logs
        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
    {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    try {
        // Get shared memory pointer
        g_shared_mem = params[0].memref.buffer;
        uint32_t buffer_size = params[1].value.a;

        if (!g_shared_mem || buffer_size == 0) {
            OCALL_LOG("[ERROR] Invalid shared memory or buffer size");
            return TEE_ERROR_BAD_PARAMETERS;
        }

        OCALL_LOG("[INFO] Shared memory: %p, Buffer size: %u", g_shared_mem, buffer_size);

        // Initialize Ring Buffer Control
        auto* ctrl = static_cast<RingBufferControl*>(g_shared_mem);
        ctrl->head.store(0, std::memory_order_release);
        ctrl->tail.store(0, std::memory_order_release);
        ctrl->last_flushed_id.store(0, std::memory_order_release);
        ctrl->sync_counter.store(0, std::memory_order_release);
        ctrl->buffer_size = buffer_size - sizeof(RingBufferControl);

        OCALL_LOG("[INFO] Ring Buffer initialized - Data buffer size: %u", ctrl->buffer_size);

        // Create Ring Buffer Producer
        g_producer = std::make_unique<SecureRingBufferProducer>(g_shared_mem);
        OCALL_LOG("[INFO] Ring Buffer Producer created");

        // Create custom LevelDB environment
        g_env = std::make_unique<RingBufferEnv>(g_producer.get());
        OCALL_LOG("[INFO] LevelDB Ring Buffer Env created");

        // Open LevelDB with custom environment
        leveldb::Options options;
        options.env = g_env.get();
        options.create_if_missing = true;
        options.write_buffer_size = 1 * 1024 * 1024; // 1MB write buffer
        options.max_open_files = 100;
        
        leveldb::DB* db_ptr = nullptr;
        leveldb::Status status = leveldb::DB::Open(options, "/tmp/leveldb_secure", &db_ptr);
        
        if (!status.ok()) {
            OCALL_LOG("[ERROR] Failed to open LevelDB: %s", status.ToString().c_str());
            
            if (params[2].memref.buffer && params[2].memref.size > 0) {
                ocall_log_flush_to_params(params[2].memref.buffer, &params[2].memref.size);
            }
            return TEE_ERROR_GENERIC;
        }

        g_db.reset(db_ptr);
        OCALL_LOG("[INFO] LevelDB opened successfully!");

        // Flush logs
        if (params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(params[2].memref.buffer, &params[2].memref.size);
        }

        return TEE_SUCCESS;

    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        if (params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(params[2].memref.buffer, &params[2].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Put a key-value pair into LevelDB
 * 
 * params[0] = MEMREF_INPUT  - Key
 * params[1] = MEMREF_INPUT  - Value
 * params[2] = VALUE_OUTPUT  - Status code
 * params[3] = MEMREF_OUTPUT - OCALL logs
 */
static TEE_Result leveldb_put(uint32_t param_types, TEE_Param params[4])
{
    OCALL_LOG("=== LevelDB PUT Operation ===");

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,    // Key
        TEE_PARAM_TYPE_MEMREF_INPUT,    // Value
        TEE_PARAM_TYPE_VALUE_OUTPUT,    // Status
        TEE_PARAM_TYPE_MEMREF_OUTPUT);  // OCALL logs

    if (param_types != exp_param_types) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    if (!g_db) {
        OCALL_LOG("[ERROR] LevelDB not initialized!");
        return TEE_ERROR_BAD_STATE;
    }

    try {
        std::string key(static_cast<char*>(params[0].memref.buffer), params[0].memref.size);
        std::string value(static_cast<char*>(params[1].memref.buffer), params[1].memref.size);

        OCALL_LOG("[INFO] Putting key='%s', value size=%zu", key.c_str(), value.size());

        leveldb::WriteOptions write_options;
        write_options.sync = true; // Force sync để trigger Flush

        leveldb::Status status = g_db->Put(write_options, key, value);
        
        if (status.ok()) {
            OCALL_LOG("[INFO] PUT successful!");
            params[2].value.a = 0; // Success
        } else {
            OCALL_LOG("[ERROR] PUT failed: %s", status.ToString().c_str());
            params[2].value.a = 1; // Failure
        }

        if (params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }

        return TEE_SUCCESS;

    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[2].value.a = 2; // Exception
        
        if (params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Get a value from LevelDB by key
 * 
 * params[0] = MEMREF_INPUT  - Key
 * params[1] = MEMREF_OUTPUT - Value
 * params[2] = VALUE_OUTPUT  - Status code (0=found, 1=not found, 2=error)
 * params[3] = MEMREF_OUTPUT - OCALL logs
 */
static TEE_Result leveldb_get(uint32_t param_types, TEE_Param params[4])
{
    OCALL_LOG("=== LevelDB GET Operation ===");

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,    // Key
        TEE_PARAM_TYPE_MEMREF_OUTPUT,   // Value
        TEE_PARAM_TYPE_VALUE_OUTPUT,    // Status
        TEE_PARAM_TYPE_MEMREF_OUTPUT);  // OCALL logs

    if (param_types != exp_param_types) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    if (!g_db) {
        OCALL_LOG("[ERROR] LevelDB not initialized!");
        return TEE_ERROR_BAD_STATE;
    }

    try {
        std::string key(static_cast<char*>(params[0].memref.buffer), params[0].memref.size);
        std::string value;

        OCALL_LOG("[INFO] Getting key='%s'", key.c_str());

        leveldb::Status status = g_db->Get(leveldb::ReadOptions(), key, &value);
        
        if (status.ok()) {
            OCALL_LOG("[INFO] GET successful! Value size=%zu", value.size());
            
            // Copy value to output buffer
            size_t copy_size = std::min(value.size(), params[1].memref.size);
            memcpy(params[1].memref.buffer, value.data(), copy_size);
            params[1].memref.size = copy_size;
            params[2].value.a = 0; // Found
            
        } else if (status.IsNotFound()) {
            OCALL_LOG("[INFO] Key not found");
            params[1].memref.size = 0;
            params[2].value.a = 1; // Not found
            
        } else {
            OCALL_LOG("[ERROR] GET failed: %s", status.ToString().c_str());
            params[2].value.a = 2; // Error
        }

        if (params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }

        return TEE_SUCCESS;

    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[2].value.a = 2; // Exception
        
        if (params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Delete a key from LevelDB
 */
static TEE_Result leveldb_delete(uint32_t param_types, TEE_Param params[4])
{
    OCALL_LOG("=== LevelDB DELETE Operation ===");

    uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT,    // Key
        TEE_PARAM_TYPE_VALUE_OUTPUT,    // Status
        TEE_PARAM_TYPE_MEMREF_OUTPUT,   // OCALL logs
        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    if (!g_db) {
        OCALL_LOG("[ERROR] LevelDB not initialized!");
        return TEE_ERROR_BAD_STATE;
    }

    try {
        std::string key(static_cast<char*>(params[0].memref.buffer), params[0].memref.size);

        OCALL_LOG("[INFO] Deleting key='%s'", key.c_str());

        leveldb::WriteOptions write_options;
        write_options.sync = true;

        leveldb::Status status = g_db->Delete(write_options, key);
        
        if (status.ok()) {
            OCALL_LOG("[INFO] DELETE successful!");
            params[1].value.a = 0;
        } else {
            OCALL_LOG("[ERROR] DELETE failed: %s", status.ToString().c_str());
            params[1].value.a = 1;
        }

        if (params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(params[2].memref.buffer, &params[2].memref.size);
        }

        return TEE_SUCCESS;

    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[1].value.a = 2;
        
        if (params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(params[2].memref.buffer, &params[2].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Called when the instance of the TA is created
 */
TEE_Result TA_CreateEntryPoint(void)
{
    DMSG("has been called");
    return TEE_SUCCESS;
}

/*
 * Called when the instance of the TA is destroyed
 */
void TA_DestroyEntryPoint(void)
{
    DMSG("has been called");
    
    // Cleanup LevelDB
    if (g_db) {
        DMSG("Closing LevelDB");
        g_db.reset();
    }
    
    g_env.reset();
    g_producer.reset();
}

/*
 * Called when a new session is opened to the TA
 */
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

/*
 * Called when a session is closed
 */
void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
    IMSG("Session closed");
}

/*
 * Called when a TA is invoked
 */
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
                                       uint32_t cmd_id,
                                       uint32_t param_types,
                                       TEE_Param params[4])
{
    switch (cmd_id)
    {
    case TA_EEVM_CMD_INIT_LEVELDB:
        return leveldb_init(param_types, params);
        
    case TA_EEVM_CMD_LEVELDB_PUT:
        return leveldb_put(param_types, params);
        
    case TA_EEVM_CMD_LEVELDB_GET:
        return leveldb_get(param_types, params);
        
    case TA_EEVM_CMD_LEVELDB_DELETE:
        return leveldb_delete(param_types, params);

    default:
        return TEE_ERROR_BAD_PARAMETERS;
    }
}
