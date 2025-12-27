// Test exception handling
#include <tee_internal_api.h>
#include <map>
#include <string>

extern "C" {
TEE_Result test_exception_handling() {
    DMSG("Testing exception handling...");
    
    // Test 1: Simple try-catch
    try {
        DMSG("Test 1: Inside try block");
        throw std::runtime_error("Test exception");
    } catch (const std::exception& e) {
        DMSG("Test 1: Caught exception: %s", e.what());
        return TEE_SUCCESS;
    } catch (...) {
        DMSG("Test 1: Caught unknown exception");
        return TEE_ERROR_GENERIC;
    }
    
    // Test 2: std::map initialization
    DMSG("Test 2: Creating std::map...");
    try {
        std::map<std::string, int> test_map;
        DMSG("Test 2: std::map created successfully");
        test_map["test"] = 42;
        DMSG("Test 2: std::map insertion successful");
        return TEE_SUCCESS;
    } catch (const std::exception& e) {
        DMSG("Test 2: Exception in std::map: %s", e.what());
        return TEE_ERROR_GENERIC;
    } catch (...) {
        DMSG("Test 2: Unknown exception in std::map");
        return TEE_ERROR_GENERIC;
    }
}

