// SPDX-License-Identifier: BSD-2-Clause
/*
 * LevelDB with Ring Buffer - OP-TEE Trusted Application
 * 
 * Uses real LevelDB from Google with custom RingBufferEnv:
 * - LevelDB data stored in RAM (MemoryFile) - no disk I/O
 * - Data sent to Normal World via ring buffer
 * - Simple ring buffer for real-time logging
 */

// Include C++ headers BEFORE TEE headers
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <stdexcept>
#include <cstring>
#include <cstdio>

// LevelDB headers
#include <leveldb/db.h>
#include <leveldb/options.h>
#include <leveldb/write_batch.h>

// TEE headers
extern "C"
{
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
}

#include "eevm_ta.h"
#include "ocall_logger.h"
#include "simple_shared_ring_buffer.h"
#include "secure_ring_buffer_producer.hpp"
#include "leveldb_ring_buffer_env_wrapper.hpp"
#include "../include/shared_ring_buffer.hpp"

// Helper function to write to ring buffer (TA side - Producer)
static void write_to_ring_buffer(struct SharedRingBuffer* rb, const char* data, size_t len) {
    if (!rb || !data || len == 0) return;
    
    // Validate ring buffer structure
    if (rb->size == 0 || rb->size > 1024 * 1024) { // Sanity check: 0 < size <= 1MB
        // Invalid size - cannot log via ring buffer, skip
        return;
    }
    
    for (size_t i = 0; i < len; i++) {
        // Read current head and tail (with memory barrier)
        __asm__ volatile("dmb ish" ::: "memory");
        uint32_t current_head = rb->head;
        uint32_t current_tail = rb->tail;
        __asm__ volatile("dmb ish" ::: "memory");
        
        uint32_t next_head = (current_head + 1) % rb->size;
        
        // Check if buffer is full (head would catch up to tail)
        if (next_head == current_tail) {
            // Buffer full - skip this byte
            // DMSG("WARN: Ring buffer full, skipping byte");
            continue;
        }
        
        // Write byte to buffer
        rb->data[current_head] = data[i];
        
        // Memory barrier: Ensure data is written to RAM before updating head
        __asm__ volatile("dmb ish" ::: "memory");
        
        // Update head
        rb->head = next_head;
        // Memory barrier - cache clean may cause issues, use simple barrier for now
        __asm__ volatile("dmb ish" ::: "memory");
    }
}


// =====================================================================
// TEST: Exception Handling and std::map
// =====================================================================

// Test virtual functions and inheritance (similar to leveldb::Env inheritance)
static TEE_Result test_virtual_functions(uint32_t param_types, TEE_Param params[4])
{
    ocall_log_init();
    OCALL_LOG("=== Testing Virtual Functions and Inheritance ===");
    
    // Test 1: Simple base class with virtual function
    OCALL_LOG("Test 1: Simple virtual function");
    try {
        struct Base {
            virtual ~Base() = default;
            virtual int getValue() { return 10; }
        };
        
        struct Derived : public Base {
            virtual int getValue() override { return 20; }
        };
        
        OCALL_LOG("  Creating Base object on stack...");
        Base base_obj;
        OCALL_LOG("  Base object created, calling getValue()...");
        int val1 = base_obj.getValue();
        OCALL_LOG("  ✓ Base::getValue() = %d", val1);
        
        OCALL_LOG("  Creating Derived object on stack...");
        Derived derived_obj;
        OCALL_LOG("  Derived object created, calling getValue()...");
        int val2 = derived_obj.getValue();
        OCALL_LOG("  ✓ Derived::getValue() = %d", val2);
        
        OCALL_LOG("  Testing virtual call through base pointer...");
        Base* ptr = &derived_obj;
        int val3 = ptr->getValue();
        OCALL_LOG("  ✓ Base*->getValue() = %d (should be 20)", val3);
        
        if (val3 != 20) {
            OCALL_LOG("  ✗ Virtual call failed! Expected 20, got %d", val3);
            return TEE_ERROR_GENERIC;
        }
        
        OCALL_LOG("  ✓ Virtual functions work correctly!");
    } catch (const std::exception& e) {
        OCALL_LOG("  ✗ Exception in Test 1: %s", e.what());
        return TEE_ERROR_GENERIC;
    } catch (...) {
        OCALL_LOG("  ✗ Unknown exception in Test 1");
        return TEE_ERROR_GENERIC;
    }
    
    // Test 2: Virtual function with inheritance (like leveldb::Env)
    OCALL_LOG("Test 2: Virtual inheritance (like leveldb::Env)");
    try {
        struct SimpleBase {
            SimpleBase() {
                OCALL_LOG("    SimpleBase constructor called");
            }
            virtual ~SimpleBase() {
                OCALL_LOG("    SimpleBase destructor called");
            }
            virtual int test() { return 100; }
        };
        
        struct SimpleDerived : public SimpleBase {
            SimpleDerived() {
                OCALL_LOG("    SimpleDerived constructor called");
            }
            virtual ~SimpleDerived() {
                OCALL_LOG("    SimpleDerived destructor called");
            }
            virtual int test() override { return 200; }
        };
        
        OCALL_LOG("  Creating SimpleDerived on stack...");
        SimpleDerived obj;
        OCALL_LOG("  SimpleDerived created, calling test()...");
        int val = obj.test();
        OCALL_LOG("  ✓ SimpleDerived::test() = %d", val);
        
        OCALL_LOG("  Testing through base pointer...");
        SimpleBase* base_ptr = &obj;
        int val2 = base_ptr->test();
        OCALL_LOG("  ✓ Base*->test() = %d (should be 200)", val2);
        
        if (val2 != 200) {
            OCALL_LOG("  ✗ Virtual call failed! Expected 200, got %d", val2);
            return TEE_ERROR_GENERIC;
        }
        
        OCALL_LOG("  ✓ Virtual inheritance works correctly!");
    } catch (const std::exception& e) {
        OCALL_LOG("  ✗ Exception in Test 2: %s", e.what());
        return TEE_ERROR_GENERIC;
    } catch (...) {
        OCALL_LOG("  ✗ Unknown exception in Test 2");
        return TEE_ERROR_GENERIC;
    }
    
    // Test 3: Test with static storage (like RingBufferEnv)
    OCALL_LOG("Test 3: Virtual inheritance with static storage");
    try {
        struct TestBase {
            TestBase() {
                OCALL_LOG("    TestBase constructor called");
            }
            virtual ~TestBase() {
                OCALL_LOG("    TestBase destructor called");
            }
            virtual int test() { return 300; }
        };
        
        struct TestDerived : public TestBase {
            TestDerived() {
                OCALL_LOG("    TestDerived constructor called");
            }
            virtual ~TestDerived() {
                OCALL_LOG("    TestDerived destructor called");
            }
            virtual int test() override { return 400; }
        };
        
        OCALL_LOG("  Allocating static storage...");
        alignas(TestDerived) static char storage[sizeof(TestDerived)];
        OCALL_LOG("  Constructing TestDerived in static storage...");
        TestDerived* obj = new (storage) TestDerived();
        OCALL_LOG("  TestDerived created at %p, calling test()...", obj);
        int val = obj->test();
        OCALL_LOG("  ✓ TestDerived::test() = %d", val);
        
        OCALL_LOG("  Testing through base pointer...");
        TestBase* base_ptr = obj;
        int val2 = base_ptr->test();
        OCALL_LOG("  ✓ Base*->test() = %d (should be 400)", val2);
        
        if (val2 != 400) {
            OCALL_LOG("  ✗ Virtual call failed! Expected 400, got %d", val2);
            obj->~TestDerived();
            return TEE_ERROR_GENERIC;
        }
        
        OCALL_LOG("  Cleaning up...");
        obj->~TestDerived();
        OCALL_LOG("  ✓ Static storage virtual inheritance works correctly!");
    } catch (const std::exception& e) {
        OCALL_LOG("  ✗ Exception in Test 3: %s", e.what());
        return TEE_ERROR_GENERIC;
    } catch (...) {
        OCALL_LOG("  ✗ Unknown exception in Test 3");
        return TEE_ERROR_GENERIC;
    }
    
    OCALL_LOG("=== All Virtual Function Tests Passed! ===");
    return TEE_SUCCESS;
}

static TEE_Result test_exception_handling(uint32_t param_types, TEE_Param params[4])
{
    ocall_log_init();
    OCALL_LOG("=== Testing Exception Handling ===");
    
    // Test 1: Simple try-catch
    OCALL_LOG("Test 1: Simple try-catch");
    try {
        OCALL_LOG("  Inside try block");
        throw std::runtime_error("Test exception message");
    } catch (const std::exception& e) {
        OCALL_LOG("  ✓ Caught std::exception: %s", e.what());
    } catch (...) {
        OCALL_LOG("  ✗ Caught unknown exception (should not happen)");
        return TEE_ERROR_GENERIC;
    }
    
    // Test 2: std::map creation (empty)
    OCALL_LOG("Test 2: Creating empty std::map");
    try {
        std::map<std::string, int> test_map;
        OCALL_LOG("  ✓ Empty std::map created successfully");
    } catch (const std::exception& e) {
        OCALL_LOG("  ✗ Exception creating empty std::map: %s", e.what());
        return TEE_ERROR_GENERIC;
    } catch (...) {
        OCALL_LOG("  ✗ Unknown exception creating empty std::map");
        return TEE_ERROR_GENERIC;
    }
    
    // Test 3: std::map insertion
    OCALL_LOG("Test 3: Inserting into std::map");
    try {
        std::map<std::string, int> test_map;
        OCALL_LOG("  About to insert first element...");
        test_map["key1"] = 100;
        OCALL_LOG("  ✓ First insertion successful");
        
        OCALL_LOG("  About to insert second element...");
        test_map["key2"] = 200;
        OCALL_LOG("  ✓ Second insertion successful");
        
        OCALL_LOG("  Map size: %zu", test_map.size());
        OCALL_LOG("  ✓ std::map operations successful!");
    } catch (const std::exception& e) {
        OCALL_LOG("  ✗ Exception in std::map operations: %s", e.what());
        return TEE_ERROR_GENERIC;
    } catch (...) {
        OCALL_LOG("  ✗ Unknown exception in std::map operations");
        return TEE_ERROR_GENERIC;
    }
    
    // Test 4: std::map in class member (like RingBufferEnv)
    OCALL_LOG("Test 4: std::map as class member");
    try {
        struct TestClass {
            std::map<std::string, int> member_map;
            TestClass() {
                OCALL_LOG("    TestClass constructor called");
            }
        };
        
        OCALL_LOG("  About to create TestClass...");
        TestClass test_obj;
        OCALL_LOG("  ✓ TestClass created successfully");
        
        OCALL_LOG("  About to insert into member_map...");
        test_obj.member_map["test"] = 42;
        OCALL_LOG("  ✓ Member map insertion successful");
    } catch (const std::exception& e) {
        OCALL_LOG("  ✗ Exception with std::map member: %s", e.what());
        return TEE_ERROR_GENERIC;
    } catch (...) {
        OCALL_LOG("  ✗ Unknown exception with std::map member");
        return TEE_ERROR_GENERIC;
    }
    
    OCALL_LOG("=== All Exception Tests Passed! ===");
    return TEE_SUCCESS;
}

// =====================================================================
// SIMPLE TEST: Test TEEC_InvokeCommand with Shared Memory (NO LevelDB)
// =====================================================================

/*
 * Simple test command with ring buffer
 * 
 * params[0] = MEMREF_INOUT - Ring buffer (for real-time logging)
 * params[1] = VALUE_INPUT  - Test value (not used, kept for compatibility)
 * params[2] = VALUE_INPUT  - Test value
 * params[3] = MEMREF_OUTPUT - OCALL logs
 */
static TEE_Result test_simple_shm(uint32_t param_types, TEE_Param params[4])
{
    
    // Initialize logging
    ocall_log_init();
    
    // Check parameter types
    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    uint32_t p2 = TEE_PARAM_TYPE_GET(param_types, 2);
    uint32_t p3 = TEE_PARAM_TYPE_GET(param_types, 3);
    
    OCALL_LOG("p0=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", p0, p1, p2, p3);
    
    // Param 0: Ring buffer (MEMREF)
    if (p0 != TEE_PARAM_TYPE_MEMREF_INOUT && 
        p0 != TEE_PARAM_TYPE_MEMREF_INPUT && 
        p0 != TEE_PARAM_TYPE_MEMREF_OUTPUT) {
        OCALL_LOG("ERROR: Invalid param 0 type: 0x%X", p0);
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Param 1: Test value (VALUE_INPUT) - optional, kept for compatibility
    // Param 2: Test value (VALUE_INPUT)
    if (p2 != TEE_PARAM_TYPE_VALUE_INPUT) {
        OCALL_LOG("ERROR: Invalid param 2 type: 0x%X", p2);
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Validate ring buffer
    if (!params[0].memref.buffer || params[0].memref.size == 0) {
        OCALL_LOG("ERROR: Invalid ring buffer");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    OCALL_LOG("Ring buffer: buffer=%p, size=%u", 
         params[0].memref.buffer, params[0].memref.size);
    OCALL_LOG("Test value: %u", params[2].value.a);
    
    // Validate ring buffer structure first
    if (params[0].memref.size < sizeof(struct SharedRingBuffer)) {
        OCALL_LOG("ERROR: Ring buffer too small: %u < %zu", 
             params[0].memref.size, sizeof(struct SharedRingBuffer));
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Get ring buffer and data shared memory
    // Use simple memory barrier instead of cache invalidation to avoid crashes
    __asm__ volatile("dmb ish" ::: "memory");
    
    struct SharedRingBuffer* rb = static_cast<struct SharedRingBuffer*>(params[0].memref.buffer);
    
    // Initialize ring buffer structure if needed (simple approach)
    // Calculate data buffer size
    uint32_t data_buffer_size = params[0].memref.size - sizeof(struct SharedRingBuffer);
    
    // Read current values with memory barrier
    __asm__ volatile("dmb ish" ::: "memory");
    uint32_t rb_size = rb->size;
    uint32_t rb_head = rb->head;
    uint32_t rb_tail = rb->tail;
    __asm__ volatile("dmb ish" ::: "memory");
    
    OCALL_LOG("Ring buffer initial state: head=%u, tail=%u, size=%u, data_buffer_size=%u", 
         rb_head, rb_tail, rb_size, data_buffer_size);
    
    // Initialize if not initialized or invalid
    if (rb_size == 0 || rb_size > data_buffer_size || rb_size > 1024 * 1024) {
        OCALL_LOG("Initializing ring buffer: old_size=%u, new_size=%u", rb_size, data_buffer_size);
        
        // Simple initialization - just set size, head and tail should already be 0
        rb->size = data_buffer_size;
        
        // Memory barrier
        __asm__ volatile("dmb ish" ::: "memory");
        
        OCALL_LOG("Ring buffer initialized: size=%u", rb->size);
    }
    
    
    // Write real-time logs to ring buffer (không cần đợi command kết thúc)
    // Only write if ring buffer is valid and large enough
    bool use_ring_buffer = (rb != nullptr) && 
                           (params[0].memref.size >= sizeof(struct SharedRingBuffer) + 1024) &&
                           (rb->size > 0) && 
                           (rb->size <= params[0].memref.size - sizeof(struct SharedRingBuffer));
    
    if (use_ring_buffer) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), 
                 "[RING] test_simple_shm: Starting...\n");
        write_to_ring_buffer(rb, log_msg, strlen(log_msg));
        
        // Simulate some processing with real-time updates
        for (int i = 0; i < 5; i++) {
            snprintf(log_msg, sizeof(log_msg), 
                     "[RING] Processing step %d/5, test_value=%u\n", i+1, params[2].value.a);
            write_to_ring_buffer(rb, log_msg, strlen(log_msg));
            
            // Small delay to simulate work
            TEE_Time start, end;
            TEE_GetSystemTime(&start);
            do {
                TEE_GetSystemTime(&end);
            } while ((end.seconds - start.seconds) * 1000 + 
                     (end.millis - start.millis) < 50); // ~50ms delay
        }
        
        
        write_to_ring_buffer(rb, "[RING] test_simple_shm: SUCCESS\n", 33);
    } else {
        OCALL_LOG("WARN: Ring buffer invalid, skipping real-time logging");
    }
    
    OCALL_LOG("=== test_simple_shm SUCCESS ===");
    OCALL_LOG("Test value received: %u", params[2].value.a);
    
    // Flush logs
    if (params[3].memref.buffer && params[3].memref.size > 0) {
        ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
    }
    return TEE_SUCCESS;
}

// =====================================================================
// LEVELDB OPERATIONS WITH RING BUFFER
// =====================================================================

// Global variables for LevelDB
static std::unique_ptr<leveldb::DB> g_db;
static std::unique_ptr<SecureRingBufferProducer> g_producer;
// Static storage for RingBufferEnv (to avoid heap allocation issues)
alignas(RingBufferEnv) static char g_env_storage[sizeof(RingBufferEnv)];
static bool g_env_storage_used = false;
// Custom deleter for static storage
static void delete_env_static(RingBufferEnv* p) {
    if (p) {
        p->~RingBufferEnv();
        g_env_storage_used = false;
    }
}
static std::unique_ptr<RingBufferEnv, void(*)(RingBufferEnv*)> g_env(nullptr, delete_env_static);
static void* g_leveldb_shared_mem = nullptr;

// Simple ring buffer for logging (separate from LevelDB data ring buffer)
static struct SharedRingBuffer* g_log_ring_buffer = nullptr;

/*
 * Initialize LevelDB with Ring Buffer
 * 
 * params[0] = MEMREF_WHOLE - LevelDB data ring buffer (RingBufferControl)
 * params[1] = MEMREF_WHOLE - Logging ring buffer (SharedRingBuffer)
 * params[2] = VALUE_INPUT  - LevelDB buffer size
 * params[3] = MEMREF_TEMP_OUTPUT - OCALL logs
 */
static TEE_Result leveldb_init(uint32_t param_types, TEE_Param params[4])
{
    ocall_log_init();
    OCALL_LOG("=== Initializing LevelDB with Ring Buffer ===");
    
    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    uint32_t p2 = TEE_PARAM_TYPE_GET(param_types, 2);
    uint32_t p3 = TEE_PARAM_TYPE_GET(param_types, 3);
    
    if (p0 != TEE_PARAM_TYPE_MEMREF_INOUT && 
        p0 != TEE_PARAM_TYPE_MEMREF_INPUT && 
        p0 != TEE_PARAM_TYPE_MEMREF_OUTPUT) {
        OCALL_LOG("[ERROR] Invalid param 0 type: 0x%X", p0);
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (p1 != TEE_PARAM_TYPE_MEMREF_INOUT && 
        p1 != TEE_PARAM_TYPE_MEMREF_INPUT && 
        p1 != TEE_PARAM_TYPE_MEMREF_OUTPUT) {
        OCALL_LOG("[ERROR] Invalid param 1 type: 0x%X", p1);
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (p2 != TEE_PARAM_TYPE_VALUE_INPUT) {
        OCALL_LOG("[ERROR] Invalid param 2 type: 0x%X", p2);
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!params[0].memref.buffer || params[0].memref.size == 0) {
        OCALL_LOG("[ERROR] Invalid LevelDB ring buffer");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!params[1].memref.buffer || params[1].memref.size == 0) {
        OCALL_LOG("[ERROR] Invalid logging ring buffer");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    try {
        // Setup logging ring buffer (simple)
        __asm__ volatile("dmb ish" ::: "memory");
        g_log_ring_buffer = static_cast<struct SharedRingBuffer*>(params[1].memref.buffer);
        
        if (params[1].memref.size < sizeof(struct SharedRingBuffer)) {
            OCALL_LOG("[ERROR] Logging ring buffer too small");
            return TEE_ERROR_BAD_PARAMETERS;
        }
        
        uint32_t log_data_size = params[1].memref.size - sizeof(struct SharedRingBuffer);
        if (g_log_ring_buffer->size == 0 || g_log_ring_buffer->size > log_data_size) {
            g_log_ring_buffer->size = log_data_size;
            __asm__ volatile("dmb ish" ::: "memory");
        }
        
        // Setup LevelDB data ring buffer (complex - RingBufferControl)
        g_leveldb_shared_mem = params[0].memref.buffer;
        uint32_t leveldb_buffer_size = params[2].value.a;
        
        if (params[0].memref.size < sizeof(RingBufferControl)) {
            OCALL_LOG("[ERROR] LevelDB ring buffer too small: %u < %zu", 
                     params[0].memref.size, sizeof(RingBufferControl));
            return TEE_ERROR_BAD_PARAMETERS;
        }
        
        OCALL_LOG("[INFO] LevelDB shared memory: %p, Buffer size: %u", 
                 g_leveldb_shared_mem, leveldb_buffer_size);
        
        // Initialize Ring Buffer Control
        auto* ctrl = static_cast<RingBufferControl*>(g_leveldb_shared_mem);
        ctrl->head = 0;
        ctrl->tail = 0;
        ctrl->last_flushed_id = 0;
        ctrl->sync_counter = 0;
        ctrl->buffer_size = leveldb_buffer_size - sizeof(RingBufferControl);
        
        __asm__ volatile("dmb ish" ::: "memory");
        
        OCALL_LOG("[INFO] LevelDB ring buffer initialized - Data buffer size: %u", ctrl->buffer_size);
        
        // Create Ring Buffer Producer
        OCALL_LOG("[DEBUG] Creating SecureRingBufferProducer...");
        try {
            g_producer = std::make_unique<SecureRingBufferProducer>(g_leveldb_shared_mem, leveldb_buffer_size);
            OCALL_LOG("[INFO] Ring Buffer Producer created");
        } catch (const std::exception& e) {
            OCALL_LOG("[EXCEPTION] Failed to create SecureRingBufferProducer: %s", e.what());
            return TEE_ERROR_GENERIC;
        } catch (...) {
            OCALL_LOG("[EXCEPTION] Unknown exception creating SecureRingBufferProducer");
            return TEE_ERROR_GENERIC;
        }
        
        // Create custom LevelDB environment (in-memory, no disk)
        OCALL_LOG("[DEBUG] Creating RingBufferEnv...");
        
        // Flush logs before creating RingBufferEnv
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        OCALL_LOG("[DEBUG] Logs flushed, about to create RingBufferEnv");
        // Create RingBufferEnv (using std::map - supported in your environment)
        // Note: If exception is thrown, it may not be caught if exception handling is not fully supported
        // But std::map itself should work if you have exception support
        try {
            OCALL_LOG("[DEBUG] About to call make_unique<RingBufferEnv> with producer=%p", g_producer.get());
            
            // Flush logs before constructor call
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            
            OCALL_LOG("[DEBUG] Calling make_unique<RingBufferEnv> constructor");
            OCALL_LOG("[DEBUG] Producer pointer: %p", g_producer.get());
            
            // Flush logs before constructor to ensure we see where it crashes
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            
            // Try to create RingBufferEnv with detailed logging
            OCALL_LOG("[DEBUG] About to allocate RingBufferEnv object...");
            
            // Flush again
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            
            // NEW APPROACH: Use static storage instead of heap to avoid vtable issues
            OCALL_LOG("[DEBUG] Attempting static storage allocation for RingBufferEnv...");
            OCALL_LOG("[DEBUG] RingBufferEnv size: %zu bytes", sizeof(RingBufferEnv));
            
            if (g_env_storage_used) {
                OCALL_LOG("[ERROR] g_env_storage already used!");
                return TEE_ERROR_GENERIC;
            }
            
            // Flush logs before construction
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            
            RingBufferEnv* env_ptr = nullptr;
            
            try {
                OCALL_LOG("[DEBUG] About to create RingBufferEnv using factory function...");
                OCALL_LOG("[DEBUG] g_producer: %p", g_producer.get());
                OCALL_LOG("[DEBUG] g_env_storage: %p, size: %zu", g_env_storage, sizeof(RingBufferEnv));
                
                // Flush logs before factory call
                if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                    params[3].memref.buffer && params[3].memref.size > 0) {
                    ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
                }
                
                OCALL_LOG("[DEBUG] About to call RingBufferEnv::Create() static function...");
                
                // Use factory function - it handles initialization internally
                env_ptr = RingBufferEnv::Create(g_producer.get(), g_env_storage);
                g_env_storage_used = true;
                OCALL_LOG("[DEBUG] RingBufferEnv factory creation completed, pointer: %p", env_ptr);
                
                // Flush logs after creation
                if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                    params[3].memref.buffer && params[3].memref.size > 0) {
                    ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
                }
            } catch (const std::bad_alloc& e) {
                OCALL_LOG("[EXCEPTION] bad_alloc in static construction RingBufferEnv");
                throw;
            } catch (const std::exception& e) {
                OCALL_LOG("[EXCEPTION] std::exception in static construction RingBufferEnv: %s", e.what());
                throw;
            } catch (...) {
                OCALL_LOG("[EXCEPTION] Unknown exception in static construction RingBufferEnv");
                throw;
            }
            
            // Flush after construction
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            
            // Store pointer with custom deleter (no free needed for static storage)
            OCALL_LOG("[DEBUG] Storing RingBufferEnv pointer...");
            g_env = std::unique_ptr<RingBufferEnv, void(*)(RingBufferEnv*)>(env_ptr, delete_env_static);
            
            OCALL_LOG("[DEBUG] RingBufferEnv constructor completed");
            OCALL_LOG("[INFO] LevelDB Ring Buffer Env created (in-memory + ring buffer)");
        } catch (const std::bad_alloc& e) {
            OCALL_LOG("[EXCEPTION] Out of memory creating RingBufferEnv");
            g_producer.reset();
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            return TEE_ERROR_OUT_OF_MEMORY;
        } catch (const std::exception& e) {
            OCALL_LOG("[EXCEPTION] Failed to create RingBufferEnv: %s", e.what());
            g_producer.reset();
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            return TEE_ERROR_GENERIC;
        } catch (...) {
            OCALL_LOG("[EXCEPTION] Unknown exception creating RingBufferEnv");
            g_producer.reset();
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            return TEE_ERROR_GENERIC;
        }
        
        // Flush logs after RingBufferEnv creation
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        
        // Open LevelDB with custom environment
        OCALL_LOG("[DEBUG] Setting up LevelDB options");
        leveldb::Options options;
        options.env = g_env.get();
        options.create_if_missing = true;
        options.write_buffer_size = 512 * 1024; // 512KB
        options.max_open_files = 50;
        options.block_cache = nullptr; // Disable cache to save memory
        options.compression = leveldb::kNoCompression;
        
        OCALL_LOG("[DEBUG] About to call leveldb::DB::Open()...");
        
        // Flush logs before DB::Open (may take time)
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        
        leveldb::DB* db_ptr = nullptr;
        leveldb::Status status;
        
        try {
            OCALL_LOG("[DEBUG] Calling leveldb::DB::Open()");
            status = leveldb::DB::Open(options, "/tmp/leveldb_secure", &db_ptr);
            OCALL_LOG("[DEBUG] DB::Open() returned");
        } catch (const std::bad_alloc& e) {
            OCALL_LOG("[EXCEPTION] Out of memory in DB::Open()");
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            return TEE_ERROR_OUT_OF_MEMORY;
        } catch (const std::exception& e) {
            OCALL_LOG("[EXCEPTION] Exception in DB::Open(): %s", e.what());
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            return TEE_ERROR_GENERIC;
        } catch (...) {
            OCALL_LOG("[EXCEPTION] Unknown exception in DB::Open()");
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            return TEE_ERROR_GENERIC;
        }
        
        if (!status.ok()) {
            OCALL_LOG("[ERROR] Failed to open LevelDB: %s", status.ToString().c_str());
            if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
                params[3].memref.buffer && params[3].memref.size > 0) {
                ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
            }
            return TEE_ERROR_GENERIC;
        }
        
        g_db.reset(db_ptr);
        OCALL_LOG("[INFO] LevelDB opened successfully!");
        
        // Log to simple ring buffer
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), 
                 "[RING] LevelDB initialized successfully (in-memory, no disk)\n");
        write_to_ring_buffer(g_log_ring_buffer, log_msg, strlen(log_msg));
        
        // Flush logs
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        
        return TEE_SUCCESS;
        
    } catch (const std::bad_alloc& e) {
        OCALL_LOG("[EXCEPTION] Out of memory: %s", e.what());
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    } catch (...) {
        OCALL_LOG("[EXCEPTION] Unknown exception during init!");
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Put a key-value pair into LevelDB
 * 
 * params[0] = MEMREF_TEMP_INPUT  - Key
 * params[1] = MEMREF_TEMP_INPUT  - Value
 * params[2] = VALUE_OUTPUT       - Status code (0=success, 1=error)
 * params[3] = MEMREF_TEMP_OUTPUT - OCALL logs (optional)
 */
static TEE_Result leveldb_put(uint32_t param_types, TEE_Param params[4])
{
    ocall_log_init();
    
    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    uint32_t p2 = TEE_PARAM_TYPE_GET(param_types, 2);
    uint32_t p3 = TEE_PARAM_TYPE_GET(param_types, 3);
    
    if (p0 != TEE_PARAM_TYPE_MEMREF_INPUT || 
        p1 != TEE_PARAM_TYPE_MEMREF_INPUT ||
        p2 != TEE_PARAM_TYPE_VALUE_OUTPUT) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!params[0].memref.buffer || params[0].memref.size == 0 ||
        !params[1].memref.buffer || params[1].memref.size == 0) {
        OCALL_LOG("[ERROR] Invalid key or value buffer!");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!g_db) {
        OCALL_LOG("[ERROR] LevelDB not initialized!");
        return TEE_ERROR_BAD_STATE;
    }
    
    try {
        std::string key(static_cast<const char*>(params[0].memref.buffer), 
                        params[0].memref.size);
        std::string value(static_cast<const char*>(params[1].memref.buffer), 
                          params[1].memref.size);
        
        // Log to simple ring buffer
        if (g_log_ring_buffer) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), 
                     "[RING] PUT: key='%s', value_size=%zu\n", 
                     key.c_str(), value.size());
            write_to_ring_buffer(g_log_ring_buffer, log_msg, strlen(log_msg));
        }
        
        // Use LevelDB thật
        leveldb::WriteOptions write_options;
        write_options.sync = false; // No sync needed (in-memory)
        
        leveldb::Status status = g_db->Put(write_options, key, value);
        
        if (status.ok()) {
            OCALL_LOG("[INFO] PUT successful: key='%s'", key.c_str());
            params[2].value.a = 0;
        } else {
            OCALL_LOG("[ERROR] PUT failed: %s", status.ToString().c_str());
            params[2].value.a = 1;
        }
        
        // Flush logs
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        
        return TEE_SUCCESS;
        
    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[2].value.a = 1;
        
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Get a value from LevelDB by key
 * 
 * params[0] = MEMREF_TEMP_INPUT  - Key
 * params[1] = MEMREF_TEMP_OUTPUT - Value
 * params[2] = VALUE_OUTPUT       - Status code (0=found, 1=not found, 2=error)
 * params[3] = MEMREF_TEMP_OUTPUT - OCALL logs (optional)
 */
static TEE_Result leveldb_get(uint32_t param_types, TEE_Param params[4])
{
    ocall_log_init();
    
    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    uint32_t p2 = TEE_PARAM_TYPE_GET(param_types, 2);
    uint32_t p3 = TEE_PARAM_TYPE_GET(param_types, 3);
    
    if (p0 != TEE_PARAM_TYPE_MEMREF_INPUT || 
        p1 != TEE_PARAM_TYPE_MEMREF_OUTPUT ||
        p2 != TEE_PARAM_TYPE_VALUE_OUTPUT) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!params[0].memref.buffer || params[0].memref.size == 0 ||
        !params[1].memref.buffer || params[1].memref.size == 0) {
        OCALL_LOG("[ERROR] Invalid key or value buffer!");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!g_db) {
        OCALL_LOG("[ERROR] LevelDB not initialized!");
        return TEE_ERROR_BAD_STATE;
    }
    
    try {
        std::string key(static_cast<const char*>(params[0].memref.buffer), 
                        params[0].memref.size);
        
        // Log to simple ring buffer
        if (g_log_ring_buffer) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "[RING] GET: key='%s'\n", key.c_str());
            write_to_ring_buffer(g_log_ring_buffer, log_msg, strlen(log_msg));
        }
        
        // Use LevelDB thật
        std::string value;
        leveldb::Status status = g_db->Get(leveldb::ReadOptions(), key, &value);
        
        if (status.ok()) {
            // Found - copy value to output buffer
            size_t copy_size = std::min(value.size(), 
                                       static_cast<size_t>(params[1].memref.size));
            if (copy_size > 0) {
                memcpy(params[1].memref.buffer, value.data(), copy_size);
            }
            params[1].memref.size = static_cast<uint32_t>(copy_size);
            params[2].value.a = 0;
            
            OCALL_LOG("[INFO] GET successful: key='%s', value_size=%zu", 
                     key.c_str(), copy_size);
        } else if (status.IsNotFound()) {
            // Not found
            params[1].memref.size = 0;
            params[2].value.a = 1;
            
            OCALL_LOG("[INFO] Key not found: '%s'", key.c_str());
        } else {
            // Error
            params[2].value.a = 2;
            OCALL_LOG("[ERROR] GET failed: %s", status.ToString().c_str());
        }
        
        // Flush logs
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        
        return TEE_SUCCESS;
        
    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[2].value.a = 2;
        
        if (p3 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[3].memref.buffer && params[3].memref.size > 0) {
            ocall_log_flush_to_params(params[3].memref.buffer, &params[3].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Delete a key from LevelDB
 * 
 * params[0] = MEMREF_TEMP_INPUT  - Key
 * params[1] = VALUE_OUTPUT       - Status code (0=success, 1=error)
 * params[2] = MEMREF_TEMP_OUTPUT - OCALL logs (optional)
 * params[3] = NONE
 */
static TEE_Result leveldb_delete(uint32_t param_types, TEE_Param params[4])
{
    ocall_log_init();
    
    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    uint32_t p2 = TEE_PARAM_TYPE_GET(param_types, 2);
    
    if (p0 != TEE_PARAM_TYPE_MEMREF_INPUT || 
        p1 != TEE_PARAM_TYPE_VALUE_OUTPUT) {
        OCALL_LOG("[ERROR] Bad parameter types!");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!params[0].memref.buffer || params[0].memref.size == 0) {
        OCALL_LOG("[ERROR] Invalid key buffer!");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!g_db) {
        OCALL_LOG("[ERROR] LevelDB not initialized!");
        return TEE_ERROR_BAD_STATE;
    }
    
    try {
        std::string key(static_cast<const char*>(params[0].memref.buffer), 
                        params[0].memref.size);
        
        // Log to simple ring buffer
        if (g_log_ring_buffer) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "[RING] DELETE: key='%s'\n", key.c_str());
            write_to_ring_buffer(g_log_ring_buffer, log_msg, strlen(log_msg));
        }
        
        // Use LevelDB thật
        leveldb::WriteOptions write_options;
        write_options.sync = false;
        
        leveldb::Status status = g_db->Delete(write_options, key);
        
        if (status.ok()) {
            OCALL_LOG("[INFO] DELETE successful: key='%s'", key.c_str());
            params[1].value.a = 0;
        } else {
            OCALL_LOG("[ERROR] DELETE failed: %s", status.ToString().c_str());
            params[1].value.a = 1;
        }
        
        // Flush logs
        if (p2 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(params[2].memref.buffer, &params[2].memref.size);
        }
        
        return TEE_SUCCESS;
        
    } catch (const std::exception& e) {
        OCALL_LOG("[EXCEPTION] %s", e.what());
        params[1].value.a = 1;
        
        if (p2 == TEE_PARAM_TYPE_MEMREF_OUTPUT && 
            params[2].memref.buffer && params[2].memref.size > 0) {
            ocall_log_flush_to_params(params[2].memref.buffer, &params[2].memref.size);
        }
        return TEE_ERROR_GENERIC;
    }
}

/*
 * Test writing a string to shared memory
 * 
 * params[0] = MEMREF_INOUT - Shared memory buffer
 * params[1] = MEMREF_OUTPUT - OCALL logs (optional)
 */
static TEE_Result test_shm_string(uint32_t param_types, TEE_Param params[4])
{
    OCALL_LOG("=== test_shm_string CALLED ===");
    
    uint32_t p0 = TEE_PARAM_TYPE_GET(param_types, 0);
    uint32_t p1 = TEE_PARAM_TYPE_GET(param_types, 1);
    
    OCALL_LOG("p0=0x%X, p1=0x%X", p0, p1);

    if (p0 != TEE_PARAM_TYPE_MEMREF_INOUT && p0 != TEE_PARAM_TYPE_MEMREF_OUTPUT) {
        OCALL_LOG("ERROR: Invalid param 0 type: 0x%X", p0);
        if (p1 == TEE_PARAM_TYPE_MEMREF_OUTPUT && params[1].memref.buffer) {
            ocall_log_flush_to_params(params[1].memref.buffer, &params[1].memref.size);
        }
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    if (!params[0].memref.buffer) {
        OCALL_LOG("ERROR: Invalid shared memory buffer");
        if (p1 == TEE_PARAM_TYPE_MEMREF_OUTPUT && params[1].memref.buffer) {
            ocall_log_flush_to_params(params[1].memref.buffer, &params[1].memref.size);
        }
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    const char* msg = "Hello from Secure World via Shared Memory!";
    size_t msg_len = strlen(msg) + 1; // Include null terminator
    
    if (params[0].memref.size < msg_len) {
        OCALL_LOG("ERROR: Shared memory too small! Need %zu, got %u", msg_len, params[0].memref.size);
        if (p1 == TEE_PARAM_TYPE_MEMREF_OUTPUT && params[1].memref.buffer) {
            ocall_log_flush_to_params(params[1].memref.buffer, &params[1].memref.size);
        }
        return TEE_ERROR_SHORT_BUFFER;
    }
    
    memcpy(params[0].memref.buffer, msg, msg_len);
    params[0].memref.size = msg_len;
    
    OCALL_LOG("Wrote message to shared memory: %s", msg);
    
    // Also write to ring buffer if available (check if it's large enough to be ring buffer)
    if (params[0].memref.size > 4096) {
        struct SharedRingBuffer* rb = static_cast<struct SharedRingBuffer*>(params[0].memref.buffer);
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "[RING] test_shm_string: %s\n", msg);
        write_to_ring_buffer(rb, log_msg, strlen(log_msg));
    }
    
    // Flush logs if requested
    if (p1 == TEE_PARAM_TYPE_MEMREF_OUTPUT && params[1].memref.buffer) {
        ocall_log_flush_to_params(params[1].memref.buffer, &params[1].memref.size);
    }
    
    return TEE_SUCCESS;
}
// =====================================================================
// TA Entry Points
// =====================================================================

TEE_Result TA_CreateEntryPoint(void)
{
    ocall_log_init();
    OCALL_LOG("TA_CreateEntryPoint called");
    return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
    ocall_log_init();
    OCALL_LOG("TA_DestroyEntryPoint called");
    
    // Cleanup LevelDB
    if (g_db) {
        OCALL_LOG("Closing LevelDB");
        g_db.reset();
    }
    
    g_env.reset();
    g_producer.reset();
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

    ocall_log_init();
    OCALL_LOG("Session opened!");
    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
    ocall_log_init();
    OCALL_LOG("Session closed");
}

TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
                                       uint32_t cmd_id,
                                       uint32_t param_types,
                                       TEE_Param params[4])
{
    // Add try-catch to prevent crashes from propagating
    ocall_log_init();
    try {
        switch (cmd_id)
        {
        case TA_EEVM_CMD_TEST_SIMPLE_SHM:
            OCALL_LOG("Dispatching to test_simple_shm");
            return test_simple_shm(param_types, params);
        
        case TA_EEVM_CMD_INIT_LEVELDB:
            OCALL_LOG("Dispatching to leveldb_init");
            try {
                return leveldb_init(param_types, params);
            } catch (const std::exception& e) {
                OCALL_LOG("EXCEPTION in leveldb_init: %s", e.what());
                return TEE_ERROR_GENERIC;
            } catch (...) {
                OCALL_LOG("UNKNOWN EXCEPTION in leveldb_init");
                return TEE_ERROR_GENERIC;
            }
            
        case TA_EEVM_CMD_LEVELDB_PUT:
            OCALL_LOG("Dispatching to leveldb_put");
            return leveldb_put(param_types, params);
            
        case TA_EEVM_CMD_LEVELDB_GET:
            OCALL_LOG("Dispatching to leveldb_get");
            return leveldb_get(param_types, params);
        
        case TA_EEVM_CMD_LEVELDB_DELETE:
            OCALL_LOG("Dispatching to leveldb_delete");
            return leveldb_delete(param_types, params);

        case TA_EEVM_CMD_TEST_SHM_STRING:
            OCALL_LOG("Dispatching to test_shm_string");
            return test_shm_string(param_types, params);
        
        case TA_EEVM_CMD_TEST_EXCEPTION:
            OCALL_LOG("Dispatching to test_exception_handling");
            try {
                return test_exception_handling(param_types, params);
            } catch (const std::exception& e) {
                OCALL_LOG("EXCEPTION in test_exception_handling: %s", e.what());
                return TEE_ERROR_GENERIC;
            } catch (...) {
                OCALL_LOG("UNKNOWN EXCEPTION in test_exception_handling");
                return TEE_ERROR_GENERIC;
            }
        
        case TA_EEVM_CMD_TEST_VIRTUAL:
            OCALL_LOG("Dispatching to test_virtual_functions");
            try {
                return test_virtual_functions(param_types, params);
            } catch (const std::exception& e) {
                OCALL_LOG("EXCEPTION in test_virtual_functions: %s", e.what());
                return TEE_ERROR_GENERIC;
            } catch (...) {
                OCALL_LOG("UNKNOWN EXCEPTION in test_virtual_functions");
                return TEE_ERROR_GENERIC;
            }

        default:
            OCALL_LOG("ERROR: Unknown command: %u", cmd_id);
            return TEE_ERROR_BAD_PARAMETERS;
        }
    } catch (...) {
        OCALL_LOG("ERROR: Exception in TA_InvokeCommandEntryPoint for cmd_id=%u", cmd_id);
        return TEE_ERROR_GENERIC;
    }
}
