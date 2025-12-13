/*
 * Copyright (c) 2024, Hello C++ TA - Simplified version
 * Using minimal C++ without full STL
 */

// Include TEE headers as C
extern "C" {
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
}

#include "hello_cpp_ta.h"

// Use C headers (available in TEE environment)
#include <string.h>
#include <stdint.h>

// C++ namespace for TA logic
namespace HelloCpp {

/*
 * Simple C++ class without STL dependencies
 */
class DataProcessor {
private:
    const char* name_;
    uint32_t counter_;
    uint32_t history_[10];
    size_t history_size_;

public:
    DataProcessor(const char* name) 
        : name_(name), counter_(0), history_size_(0) {
        DMSG("C++ Constructor called: %s", name_);
        // Initialize array
        for (int i = 0; i < 10; i++) {
            history_[i] = 0;
        }
    }

    ~DataProcessor() {
        DMSG("C++ Destructor called: %s", name_);
    }

    void increment() {
        counter_++;
        if (history_size_ < 10) {
            history_[history_size_++] = counter_;
        }
        DMSG("Counter incremented to: %u", counter_);
    }

    uint32_t getCounter() const {
        return counter_;
    }

    const char* getName() const {
        return name_;
    }

    size_t getHistorySize() const {
        return history_size_;
    }

    // Simple sort without STL
    void sortArray(uint32_t* data, size_t size) {
        DMSG("Sorting array of size: %zu", size);
        
        // Bubble sort
        for (size_t i = 0; i < size - 1; i++) {
            for (size_t j = 0; j < size - i - 1; j++) {
                if (data[j] > data[j + 1]) {
                    uint32_t temp = data[j];
                    data[j] = data[j + 1];
                    data[j + 1] = temp;
                }
            }
        }
    }
};

/*
 * Test basic C++ features
 */
TEE_Result test_basic_cpp() {
    DMSG("=== Testing Basic C++ Features ===");
    
    // Test const char* manipulation
    const char* message = "Hello from C++ in TEE!";
    DMSG("String message: %s", message);
    size_t len = 0;
    while (message[len]) len++;
    DMSG("String length: %zu", len);
    
    // Test simple string operations  
    char combined[100] = {0};
    const char* part1 = "Secure ";
    const char* part2 = "World";
    
    // Manual copy
    size_t i = 0;
    while (part1[i]) {
        combined[i] = part1[i];
        i++;
    }
    size_t j = 0;
    while (part2[j]) {
        combined[i++] = part2[j++];
    }
    combined[i] = '\0';
    
    DMSG("Combined: %s", combined);
    
    return TEE_SUCCESS;
}

/*
 * Test C++ arrays and loops
 */
TEE_Result test_cpp_arrays() {
    DMSG("=== Testing C++ Arrays ===");
    
    // Test array
    uint32_t numbers[5] = {10, 20, 30, 40, 50};
    
    DMSG("Array size: %zu", sizeof(numbers)/sizeof(numbers[0]));
    DMSG("Array contents:");
    for (size_t i = 0; i < 5; i++) {
        DMSG("  [%zu] = %u", i, numbers[i]);
    }
    
    // Calculate sum
    uint32_t sum = 0;
    for (size_t i = 0; i < 5; i++) {
        sum += numbers[i];
    }
    DMSG("Sum of elements: %u", sum);
    
    // Simple find
    uint32_t target = 30;
    int found_index = -1;
    for (size_t i = 0; i < 5; i++) {
        if (numbers[i] == target) {
            found_index = i;
            break;
        }
    }
    if (found_index >= 0) {
        DMSG("Found value %u at position: %d", target, found_index);
    }
    
    return TEE_SUCCESS;
}

/*
 * Test C++ class
 */
TEE_Result test_class_usage() {
    DMSG("=== Testing C++ Class ===");
    
    // Create object on stack
    DataProcessor processor("SecureProcessor");
    
    DMSG("Processor name: %s", processor.getName());
    
    // Test increment
    for (int i = 0; i < 3; i++) {
        processor.increment();
    }
    
    DMSG("Final counter: %u", processor.getCounter());
    DMSG("History size: %zu", processor.getHistorySize());
    
    // Test with array
    uint32_t data[] = {42, 17, 99, 5, 63};
    size_t data_size = sizeof(data) / sizeof(data[0]);
    
    DMSG("Before sort: 42, 17, 99, 5, 63");
    processor.sortArray(data, data_size);
    
    DMSG("After sort:");
    for (size_t i = 0; i < data_size; i++) {
        DMSG("  %u", data[i]);
    }
    
    return TEE_SUCCESS;
}

/*
 * Process buffer data using C++
 */
TEE_Result process_data(void* in_buf, size_t in_size, 
                        void* out_buf, size_t* out_size) {
    DMSG("=== Processing Data ===");
    DMSG("Input size: %zu bytes", in_size);
    
    if (!in_buf || !out_buf || !out_size) {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    
    // Get input as C string
    const char* input = (const char*)in_buf;
    char* output = (char*)out_buf;
    
    DMSG("Input string: %s", input);
    
    // Process: uppercase conversion
    for (size_t i = 0; i < in_size && i < *out_size; i++) {
        char c = input[i];
        if (c >= 'a' && c <= 'z') {
            output[i] = c - 32; // Convert to uppercase
        } else {
            output[i] = c;
        }
    }
    
    *out_size = in_size;
    DMSG("Output string: %s", output);
    DMSG("Output size: %zu bytes", *out_size);
    
    return TEE_SUCCESS;
}

} // namespace HelloCpp

/*
 * ===================================================================
 * TA Entry Points
 * Must use extern "C" to avoid name mangling even though headers declare them
 * ===================================================================
 */

extern "C" {
TEE_Result TA_CreateEntryPoint(void)
{
    DMSG("TA_CreateEntryPoint (C++)");
    // Initialize C++ runtime (minimal)
    // Global constructors are called automatically
    return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
    DMSG("TA_DestroyEntryPoint (C++)");
    
    // C++ runtime cleanup
    // Global destructors are called automatically
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
                                     TEE_Param __maybe_unused params[4],
                                     void __maybe_unused **sess_ctx)
{
    uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
                                                 TEE_PARAM_TYPE_NONE,
                                                 TEE_PARAM_TYPE_NONE,
                                                 TEE_PARAM_TYPE_NONE);

    DMSG("TA_OpenSessionEntryPoint (C++)");

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    /* Unused parameters */
    (void)&params;
    (void)&sess_ctx;

    IMSG("Hello from C++ Trusted Application!");

    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
    (void)&sess_ctx;
    IMSG("Goodbye from C++ TA!");
}

TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
                                       uint32_t cmd_id,
                                       uint32_t param_types,
                                       TEE_Param params[4])
{
    (void)&sess_ctx;
    DMSG("TA_InvokeCommandEntryPoint: cmd=%u (C++)", cmd_id);
    switch (cmd_id) {
    case TA_HELLO_CPP_CMD_TEST_BASIC:
        DMSG("Command: TEST_BASIC");
        return HelloCpp::test_basic_cpp();

    case TA_HELLO_CPP_CMD_TEST_STL:
        DMSG("Command: TEST_CPP_ARRAYS");
        return HelloCpp::test_cpp_arrays();

    case TA_HELLO_CPP_CMD_TEST_CLASS:
        DMSG("Command: TEST_CLASS");
        return HelloCpp::test_class_usage();

    case TA_HELLO_CPP_CMD_PROCESS_DATA: {
        DMSG("Command: PROCESS_DATA");
        
        uint32_t exp_param_types = TEE_PARAM_TYPES(
            TEE_PARAM_TYPE_MEMREF_INPUT,
            TEE_PARAM_TYPE_MEMREF_OUTPUT,
            TEE_PARAM_TYPE_NONE,
            TEE_PARAM_TYPE_NONE);

        if (param_types != exp_param_types) {
            return TEE_ERROR_BAD_PARAMETERS;
        }

        void* in_buf = params[0].memref.buffer;
        size_t in_size = params[0].memref.size;
        void* out_buf = params[1].memref.buffer;
        size_t out_size = params[1].memref.size;

        TEE_Result res = HelloCpp::process_data(in_buf, in_size, 
                                                 out_buf, &out_size);
        
        if (res == TEE_SUCCESS) {
            params[1].memref.size = out_size;
        }

        return res;
    }

    default:
        EMSG("Unknown command: %u", cmd_id);
        return TEE_ERROR_BAD_PARAMETERS;
    }
}

} // extern "C"
