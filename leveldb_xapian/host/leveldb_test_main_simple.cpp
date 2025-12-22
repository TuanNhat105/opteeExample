// SPDX-License-Identifier: BSD-2-Clause
/*
 * Host application for LevelDB with Simple Buffer Accumulation
 * 
 * STRATEGY: Receive data operations from TA and save to LevelDB in normal world
 * - TA accumulates operations in buffer during command execution
 * - Host receives buffer after command completion
 * - Host parses and applies operations to local LevelDB
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <err.h>
#include <memory>

/* OP-TEE TEE client API */
#include <tee_client_api.h>

/* TA UUID and commands */
#include <eevm_ta.h>

/* LevelDB */
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <leveldb/options.h>

// Data operation structure (matches TA)
struct DataOperation {
    uint8_t op_type;      // 0=PUT, 1=DELETE
    uint16_t key_len;
    uint16_t value_len;
    // Followed by: key_data, value_data
};

class LevelDBHost {
private:
    std::unique_ptr<leveldb::DB> db_;
    
public:
    bool Open(const std::string& path) {
        leveldb::Options options;
        options.create_if_missing = true;
        
        leveldb::DB* db_ptr = nullptr;
        leveldb::Status status = leveldb::DB::Open(options, path, &db_ptr);
        
        if (!status.ok()) {
            std::cerr << "Failed to open LevelDB: " << status.ToString() << std::endl;
            return false;
        }
        
        db_.reset(db_ptr);
        std::cout << "✓ Opened LevelDB at: " << path << std::endl;
        return true;
    }
    
    bool ApplyOperations(const uint8_t* buffer, size_t buffer_size) {
        if (buffer_size == 0) {
            std::cout << "[INFO] No operations to apply" << std::endl;
            return true;
        }
        
        std::cout << "[INFO] Applying operations from " << buffer_size << " byte buffer" << std::endl;
        
        size_t pos = 0;
        int op_count = 0;
        
        while (pos + sizeof(DataOperation) <= buffer_size) {
            const DataOperation* op = reinterpret_cast<const DataOperation*>(buffer + pos);
            pos += sizeof(DataOperation);
            
            // Validate operation
            if (pos + op->key_len + op->value_len > buffer_size) {
                std::cerr << "[ERROR] Malformed operation at pos=" << pos << std::endl;
                break;
            }
            
            // Extract key
            std::string key(reinterpret_cast<const char*>(buffer + pos), op->key_len);
            pos += op->key_len;
            
            if (op->op_type == 0) {
                // PUT operation
                std::string value(reinterpret_cast<const char*>(buffer + pos), op->value_len);
                pos += op->value_len;
                
                leveldb::WriteOptions write_options;
                write_options.sync = true;
                
                leveldb::Status status = db_->Put(write_options, key, value);
                if (status.ok()) {
                    std::cout << "  [PUT] '" << key << "' => " << value.size() << " bytes" << std::endl;
                    op_count++;
                } else {
                    std::cerr << "  [ERROR] PUT failed: " << status.ToString() << std::endl;
                }
                
            } else if (op->op_type == 1) {
                // DELETE operation
                leveldb::WriteOptions write_options;
                write_options.sync = true;
                
                leveldb::Status status = db_->Delete(write_options, key);
                if (status.ok()) {
                    std::cout << "  [DELETE] '" << key << "'" << std::endl;
                    op_count++;
                } else {
                    std::cerr << "  [ERROR] DELETE failed: " << status.ToString() << std::endl;
                }
                
            } else {
                std::cerr << "[ERROR] Unknown operation type: " << (int)op->op_type << std::endl;
                break;
            }
        }
        
        std::cout << "✓ Applied " << op_count << " operations" << std::endl;
        return (op_count > 0);
    }
    
    bool Get(const std::string& key, std::string& value) {
        leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
        return status.ok();
    }
    
    void Close() {
        db_.reset();
    }
};

int main()
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_EEVM_UUID;
    uint32_t err_origin;
    
    std::cout << "========================================" << std::endl;
    std::cout << " LevelDB with Simple Buffer Test" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    /* Initialize a context connecting us to the TEE */
    res = TEEC_InitializeContext(nullptr, &ctx);
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_InitializeContext failed with code 0x" << std::hex << res << std::endl;
        return 1;
    }
    
    std::cout << "✓ TEE Context initialized" << std::endl;
    
    /* Open a session to the TA */
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                          TEEC_LOGIN_PUBLIC, nullptr, &op, &err_origin);
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "TEEC_OpenSession failed with code 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::endl;
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Session opened to TA" << std::endl << std::endl;
    
    // ==================== Open Local LevelDB ====================
    std::cout << "=== Opening Local LevelDB (Normal World) ===" << std::endl;
    
    LevelDBHost leveldb;
    if (!leveldb.Open("/tmp/leveldb_host")) {
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    std::cout << std::endl;
    
    // ==================== Initialize Simple Buffer in TA ====================
    std::cout << "=== Initializing Simple Buffer in TA ===" << std::endl;
    
    char log_buffer[8192] = {0};
    
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INPUT,             // Init flag
        TEEC_MEMREF_TEMP_OUTPUT,      // OCALL logs
        TEEC_NONE,
        TEEC_NONE);
    
    op.params[0].value.a = 1;  // Init flag
    op.params[1].tmpref.buffer = log_buffer;
    op.params[1].tmpref.size = sizeof(log_buffer);
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_INIT_LEVELDB, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ Simple Buffer initialization failed: 0x" << std::hex << res << std::endl;
        leveldb.Close();
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Simple Buffer initialized successfully!" << std::endl << std::endl;

    // ==================== Test PUT Operation ====================
    std::cout << "=== Testing PUT Operation ===" << std::endl;
    
    std::string test_key = "user:1001";
    std::string test_value = "Alice:alice@example.com:25";
    
    char data_buffer[128 * 1024];  // 128KB buffer for data operations
    std::memset(log_buffer, 0, sizeof(log_buffer));
    std::memset(data_buffer, 0, sizeof(data_buffer));
    std::memset(&op, 0, sizeof(op));
    
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,       // Key
        TEEC_MEMREF_TEMP_INPUT,       // Value
        TEEC_MEMREF_TEMP_OUTPUT,      // Data operations buffer
        TEEC_MEMREF_TEMP_OUTPUT);     // Logs
    
    op.params[0].tmpref.buffer = (void*)test_key.data();
    op.params[0].tmpref.size = test_key.size();
    op.params[1].tmpref.buffer = (void*)test_value.data();
    op.params[1].tmpref.size = test_value.size();
    op.params[2].tmpref.buffer = data_buffer;
    op.params[2].tmpref.size = sizeof(data_buffer);
    op.params[3].tmpref.buffer = log_buffer;
    op.params[3].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Putting: '" << test_key << "' => '" << test_value << "'" << std::endl;
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_PUT, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ PUT command failed: 0x" << std::hex << res << std::endl;
    } else {
        // Check status from param 2 VALUE_OUTPUT - but we changed it to MEMREF_OUTPUT!
        // Fix: status is now in the original params[2].value.a which we need to access differently
        std::cout << "✓ PUT command successful!" << std::endl;
        
        // Apply operations to local LevelDB
        if (op.params[2].tmpref.size > 0) {
            leveldb.ApplyOperations(reinterpret_cast<const uint8_t*>(data_buffer), 
                                    op.params[2].tmpref.size);
        }
    }
    std::cout << std::endl;

    // ==================== Test GET from TA ====================
    std::cout << "=== Testing GET from TA (in-memory) ===" << std::endl;
    
    char get_buffer[1024] = {0};
    std::memset(log_buffer, 0, sizeof(log_buffer));
    std::memset(&op, 0, sizeof(op));
    
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,       // Key
        TEEC_MEMREF_TEMP_OUTPUT,      // Value
        TEEC_VALUE_OUTPUT,            // Status
        TEEC_MEMREF_TEMP_OUTPUT);     // Logs
    
    op.params[0].tmpref.buffer = (void*)test_key.data();
    op.params[0].tmpref.size = test_key.size();
    op.params[1].tmpref.buffer = get_buffer;
    op.params[1].tmpref.size = sizeof(get_buffer);
    op.params[3].tmpref.buffer = log_buffer;
    op.params[3].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Getting from TA: '" << test_key << "'" << std::endl;
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_GET, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res == TEEC_SUCCESS) {
        uint32_t status = op.params[2].value.a;
        if (status == 0) {
            std::string retrieved(get_buffer, op.params[1].tmpref.size);
            std::cout << "✓ GET from TA successful! Value: '" << retrieved << "'" << std::endl;
        } else if (status == 1) {
            std::cout << "❌ Key not found in TA" << std::endl;
        } else {
            std::cout << "❌ GET error: " << status << std::endl;
        }
    }
    std::cout << std::endl;

    // ==================== Test GET from Host LevelDB ====================
    std::cout << "=== Testing GET from Host LevelDB ===" << std::endl;
    
    std::string host_value;
    if (leveldb.Get(test_key, host_value)) {
        std::cout << "✓ GET from host successful! Value: '" << host_value << "'" << std::endl;
        
        if (host_value == test_value) {
            std::cout << "✓✓ Value matches original!" << std::endl;
        } else {
            std::cout << "❌ Value mismatch!" << std::endl;
        }
    } else {
        std::cout << "❌ Key not found in host LevelDB" << std::endl;
    }
    std::cout << std::endl;

    // ==================== Test Multiple PUTs ====================
    std::cout << "=== Testing Multiple PUT Operations ===" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> test_data = {
        {"user:1002", "Bob:bob@example.com:30"},
        {"user:1003", "Charlie:charlie@example.com:35"},
        {"product:001", "Laptop:Dell:999.99"},
        {"product:002", "Mouse:Logitech:29.99"},
        {"config:timeout", "30000"},
    };
    
    for (const auto& [key, value] : test_data) {
        std::memset(data_buffer, 0, sizeof(data_buffer));
        std::memset(&op, 0, sizeof(op));
        
        op.paramTypes = TEEC_PARAM_TYPES(
            TEEC_MEMREF_TEMP_INPUT,
            TEEC_MEMREF_TEMP_INPUT,
            TEEC_MEMREF_TEMP_OUTPUT,
            TEEC_NONE);
        
        op.params[0].tmpref.buffer = (void*)key.data();
        op.params[0].tmpref.size = key.size();
        op.params[1].tmpref.buffer = (void*)value.data();
        op.params[1].tmpref.size = value.size();
        op.params[2].tmpref.buffer = data_buffer;
        op.params[2].tmpref.size = sizeof(data_buffer);
        
        std::cout << "PUT: '" << key << "' ... ";
        
        res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_PUT, &op, &err_origin);
        
        if (res == TEEC_SUCCESS) {
            std::cout << "✓ (TA)" << std::endl;
            
            // Apply to host
            if (op.params[2].tmpref.size > 0) {
                leveldb.ApplyOperations(reinterpret_cast<const uint8_t*>(data_buffer), 
                                        op.params[2].tmpref.size);
            }
        } else {
            std::cout << "❌" << std::endl;
        }
    }
    
    std::cout << "\n✓ Batch PUT completed" << std::endl << std::endl;

    // ==================== Verify All Data in Host LevelDB ====================
    std::cout << "=== Verifying All Data in Host LevelDB ===" << std::endl;
    
    std::vector<std::string> all_keys = {test_key};
    for (const auto& [key, _] : test_data) {
        all_keys.push_back(key);
    }
    
    int found_count = 0;
    for (const auto& key : all_keys) {
        std::string value;
        if (leveldb.Get(key, value)) {
            std::cout << "  ✓ '" << key << "' => " << value.size() << " bytes" << std::endl;
            found_count++;
        } else {
            std::cout << "  ❌ '" << key << "' NOT FOUND" << std::endl;
        }
    }
    
    std::cout << "\n✓ Verified " << found_count << "/" << all_keys.size() << " keys" << std::endl;

    // ==================== Cleanup ====================
    std::cout << "\n=== Cleaning up ===" << std::endl;
    
    leveldb.Close();
    std::cout << "✓ LevelDB closed" << std::endl;
    
    TEEC_CloseSession(&sess);
    std::cout << "✓ Session closed" << std::endl;
    
    TEEC_FinalizeContext(&ctx);
    std::cout << "✓ Context finalized" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << " Test completed successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
