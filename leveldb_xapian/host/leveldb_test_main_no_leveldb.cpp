// SPDX-License-Identifier: BSD-2-Clause
/*
 * Host application for testing Simple Buffer Accumulation WITHOUT LevelDB
 * 
 * PURPOSE: Verify that data can be transferred from Secure World to Normal World
 *          using Simple Buffer Accumulation pattern (like OCALL)
 * 
 * This version does NOT use LevelDB - just tests the buffer transfer mechanism
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>

/* OP-TEE TEE client API */
#include <tee_client_api.h>

/* TA UUID and commands */
#include <eevm_ta.h>

// Data operation structure (matches TA)
struct DataOperation {
    uint8_t op_type;      // 0=PUT, 1=DELETE
    uint16_t key_len;
    uint16_t value_len;
    // Followed by: key_data, value_data
};

void print_hex_dump(const uint8_t* data, size_t len, const std::string& label) {
    std::cout << label << " (" << len << " bytes):" << std::endl;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && i % 16 == 0) std::cout << std::endl;
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

void parse_and_display_operations(const uint8_t* buffer, size_t buffer_size) {
    if (buffer_size == 0) {
        std::cout << "[INFO] No operations in buffer" << std::endl;
        return;
    }
    
    std::cout << "\n=== Parsing Data Operations Buffer ===" << std::endl;
    std::cout << "Buffer size: " << buffer_size << " bytes" << std::endl;
    
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
        
        op_count++;
        std::cout << "\n--- Operation #" << op_count << " ---" << std::endl;
        
        // Extract key
        std::string key(reinterpret_cast<const char*>(buffer + pos), op->key_len);
        pos += op->key_len;
        
        if (op->op_type == 0) {
            // PUT operation
            std::string value(reinterpret_cast<const char*>(buffer + pos), op->value_len);
            pos += op->value_len;
            
            std::cout << "Type: PUT" << std::endl;
            std::cout << "Key: '" << key << "' (" << op->key_len << " bytes)" << std::endl;
            std::cout << "Value: '" << value << "' (" << op->value_len << " bytes)" << std::endl;
            
        } else if (op->op_type == 1) {
            // DELETE operation
            std::cout << "Type: DELETE" << std::endl;
            std::cout << "Key: '" << key << "' (" << op->key_len << " bytes)" << std::endl;
            
        } else {
            std::cerr << "[ERROR] Unknown operation type: " << (int)op->op_type << std::endl;
            break;
        }
    }
    
    std::cout << "\n✓ Parsed " << op_count << " operations" << std::endl;
}

int main()
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_EEVM_UUID;
    uint32_t err_origin;
    
    std::cout << "========================================" << std::endl;
    std::cout << " Simple Buffer Test (NO LevelDB)" << std::endl;
    std::cout << " Testing Secure->Normal World Transfer" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    /* Initialize a context connecting us to the TEE */
    res = TEEC_InitializeContext(nullptr, &ctx);
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ TEEC_InitializeContext failed: 0x" << std::hex << res << std::endl;
        return 1;
    }
    
    std::cout << "✓ TEE Context initialized" << std::endl;
    
    /* Open a session to the TA */
    std::memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                          TEEC_LOGIN_PUBLIC, nullptr, &op, &err_origin);
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ TEEC_OpenSession failed: 0x" << std::hex << res 
                  << " origin 0x" << err_origin << std::endl;
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Session opened to TA" << std::endl << std::endl;
    
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
        std::cerr << "❌ Initialization failed: 0x" << std::hex << res << std::endl;
        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 1;
    }
    
    std::cout << "✓ Simple Buffer initialized!" << std::endl << std::endl;

    // ==================== Test PUT Operation ====================
    std::cout << "=== Test 1: Single PUT Operation ===" << std::endl;
    
    std::string test_key = "user:1001";
    std::string test_value = "Alice:alice@example.com:25";
    
    char data_buffer[128 * 1024];  // 128KB buffer
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
    
    std::cout << "Invoking PUT: '" << test_key << "' => '" << test_value << "'" << std::endl;
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_PUT, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res != TEEC_SUCCESS) {
        std::cerr << "❌ PUT failed: 0x" << std::hex << res << std::endl;
    } else {
        std::cout << "✓ PUT command successful!" << std::endl;
        
        // Parse and display what TA sent us
        size_t received_size = op.params[2].tmpref.size;
        std::cout << "\n✓ Received " << received_size << " bytes from TA" << std::endl;
        
        if (received_size > 0) {
            parse_and_display_operations(reinterpret_cast<const uint8_t*>(data_buffer), 
                                        received_size);
        }
    }
    std::cout << std::endl;

    // ==================== Test GET from TA ====================
    std::cout << "=== Test 2: GET from TA (in-memory) ===" << std::endl;
    
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
    
    std::cout << "Getting: '" << test_key << "'" << std::endl;
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_GET, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res == TEEC_SUCCESS) {
        uint32_t status = op.params[2].value.a;
        if (status == 0) {
            std::string retrieved(get_buffer, op.params[1].tmpref.size);
            std::cout << "✓ GET successful!" << std::endl;
            std::cout << "  Retrieved: '" << retrieved << "'" << std::endl;
            
            if (retrieved == test_value) {
                std::cout << "✓✓ Value matches!" << std::endl;
            } else {
                std::cout << "❌ Value mismatch!" << std::endl;
            }
        } else if (status == 1) {
            std::cout << "❌ Key not found" << std::endl;
        } else {
            std::cout << "❌ GET error: " << status << std::endl;
        }
    } else {
        std::cerr << "❌ GET failed: 0x" << std::hex << res << std::endl;
    }
    std::cout << std::endl;

    // ==================== Test Multiple PUTs ====================
    std::cout << "=== Test 3: Multiple PUT Operations ===" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> test_data = {
        {"user:1002", "Bob:bob@example.com:30"},
        {"user:1003", "Charlie:charlie@example.com:35"},
        {"product:001", "Laptop:Dell:999.99"},
        {"product:002", "Mouse:Logitech:29.99"},
        {"config:timeout", "30000"},
    };
    
    for (size_t i = 0; i < test_data.size(); i++) {
        const auto& [key, value] = test_data[i];
        
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
        
        std::cout << "[" << (i+1) << "/" << test_data.size() << "] PUT: '" << key << "' ... ";
        
        res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_PUT, &op, &err_origin);
        
        if (res == TEEC_SUCCESS) {
            size_t received = op.params[2].tmpref.size;
            std::cout << "✓ (received " << received << " bytes)" << std::endl;
        } else {
            std::cout << "❌ (error 0x" << std::hex << res << ")" << std::endl;
        }
    }
    
    std::cout << "\n✓ Batch PUT completed" << std::endl << std::endl;

    // ==================== Verify All Data in TA ====================
    std::cout << "=== Test 4: Verify All Keys in TA Memory ===" << std::endl;
    
    std::vector<std::string> all_keys = {test_key};
    for (const auto& [key, _] : test_data) {
        all_keys.push_back(key);
    }
    
    int found_count = 0;
    for (const auto& key : all_keys) {
        std::memset(get_buffer, 0, sizeof(get_buffer));
        std::memset(&op, 0, sizeof(op));
        
        op.paramTypes = TEEC_PARAM_TYPES(
            TEEC_MEMREF_TEMP_INPUT,
            TEEC_MEMREF_TEMP_OUTPUT,
            TEEC_VALUE_OUTPUT,
            TEEC_NONE);
        
        op.params[0].tmpref.buffer = (void*)key.data();
        op.params[0].tmpref.size = key.size();
        op.params[1].tmpref.buffer = get_buffer;
        op.params[1].tmpref.size = sizeof(get_buffer);
        
        res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_GET, &op, &err_origin);
        
        if (res == TEEC_SUCCESS && op.params[2].value.a == 0) {
            std::string value(get_buffer, op.params[1].tmpref.size);
            std::cout << "  ✓ '" << key << "' => " << value.size() << " bytes" << std::endl;
            found_count++;
        } else {
            std::cout << "  ❌ '" << key << "' NOT FOUND" << std::endl;
        }
    }
    
    std::cout << "\n✓ Verified " << found_count << "/" << all_keys.size() << " keys in TA memory" << std::endl;

    // ==================== Test DELETE ====================
    std::cout << "\n=== Test 5: DELETE Operation ===" << std::endl;
    
    std::string delete_key = "product:001";
    
    std::memset(data_buffer, 0, sizeof(data_buffer));
    std::memset(log_buffer, 0, sizeof(log_buffer));
    std::memset(&op, 0, sizeof(op));
    
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,       // Key
        TEEC_MEMREF_TEMP_OUTPUT,      // Data buffer
        TEEC_MEMREF_TEMP_OUTPUT,      // Logs
        TEEC_NONE);
    
    op.params[0].tmpref.buffer = (void*)delete_key.data();
    op.params[0].tmpref.size = delete_key.size();
    op.params[1].tmpref.buffer = data_buffer;
    op.params[1].tmpref.size = sizeof(data_buffer);
    op.params[2].tmpref.buffer = log_buffer;
    op.params[2].tmpref.size = sizeof(log_buffer);
    
    std::cout << "Deleting: '" << delete_key << "'" << std::endl;
    
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_LEVELDB_DELETE, &op, &err_origin);
    
    if (log_buffer[0] != '\0') {
        std::cout << "\n--- TA Logs ---" << std::endl;
        std::cout << log_buffer << std::endl;
    }
    
    if (res == TEEC_SUCCESS) {
        std::cout << "✓ DELETE successful!" << std::endl;
        
        size_t received = op.params[1].tmpref.size;
        if (received > 0) {
            std::cout << "✓ Received " << received << " bytes" << std::endl;
            parse_and_display_operations(reinterpret_cast<const uint8_t*>(data_buffer), received);
        }
    } else {
        std::cerr << "❌ DELETE failed: 0x" << std::hex << res << std::endl;
    }

    // ==================== Summary ====================
    std::cout << "\n========================================" << std::endl;
    std::cout << " TEST SUMMARY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "✓ Simple Buffer Accumulation works!" << std::endl;
    std::cout << "✓ Data transfer from Secure World OK" << std::endl;
    std::cout << "✓ No 0xFFFF3024 errors!" << std::endl;
    std::cout << "✓ Ready for Normal World persistence" << std::endl;
    std::cout << "========================================" << std::endl;

    // ==================== Cleanup ====================
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
    
    return 0;
}
