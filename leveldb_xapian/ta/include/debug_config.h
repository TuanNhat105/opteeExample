// SPDX-License-Identifier: BSD-2-Clause
/*
 * Debug Configuration for LevelDB TA
 * 
 * Control debug output and test code with compile-time flags
 */

#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

// ============================================================================
// Debug Level Configuration
// ============================================================================

// Enable debug logging (set to 0 to disable all debug logs)
#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1
#endif

// Enable verbose parameter logging
#ifndef DEBUG_PARAMS
#define DEBUG_PARAMS 1
#endif

// Enable step-by-step execution logging
#ifndef DEBUG_STEPS
#define DEBUG_STEPS 1
#endif

// Enable test code (memory access tests, etc.)
#ifndef DEBUG_TEST_CODE
#define DEBUG_TEST_CODE 1
#endif

// Enable performance timing
#ifndef DEBUG_PERF
#define DEBUG_PERF 0
#endif

// ============================================================================
// Debug Macros
// ============================================================================

#if DEBUG_ENABLED
    #define DBG_LOG(fmt, ...) OCALL_LOG("[DEBUG] " fmt, ##__VA_ARGS__)
    #define DBG_DMSG(fmt, ...) DMSG(fmt, ##__VA_ARGS__)
#else
    #define DBG_LOG(fmt, ...) ((void)0)
    #define DBG_DMSG(fmt, ...) ((void)0)
#endif

#if DEBUG_ENABLED && DEBUG_PARAMS
    #define DBG_PARAM(fmt, ...) OCALL_LOG("[PARAM] " fmt, ##__VA_ARGS__)
#else
    #define DBG_PARAM(fmt, ...) ((void)0)
#endif

#if DEBUG_ENABLED && DEBUG_STEPS
    #define DBG_STEP(step, fmt, ...) OCALL_LOG("[STEP %d] " fmt, step, ##__VA_ARGS__)
#else
    #define DBG_STEP(step, fmt, ...) ((void)0)
#endif

#if DEBUG_ENABLED && DEBUG_TEST_CODE
    #define DBG_TEST(fmt, ...) OCALL_LOG("[TEST] " fmt, ##__VA_ARGS__)
    #define DBG_TEST_CODE(code) code
#else
    #define DBG_TEST(fmt, ...) ((void)0)
    #define DBG_TEST_CODE(code) ((void)0)
#endif

#if DEBUG_ENABLED && DEBUG_PERF
    #include <tee_api.h>
    #include <tee_internal_api.h>
    // Performance timing macros - use TEE_GetSystemTime() correctly
    // Note: These macros require variables to be in the same scope
    // Usage:
    //   TEE_Time _perf_start, _perf_end;
    //   TEE_GetSystemTime(&_perf_start);
    //   // ... code to measure ...
    //   TEE_GetSystemTime(&_perf_end);
    //   DBG_PERF_LOG("Operation", &_perf_start, &_perf_end);
    #define DBG_PERF_LOG(label, start_time, end_time) do { \
        uint32_t _start_ms = ((start_time)->seconds * 1000) + (start_time)->millis; \
        uint32_t _end_ms = ((end_time)->seconds * 1000) + (end_time)->millis; \
        uint32_t _diff_ms = _end_ms - _start_ms; \
        OCALL_LOG("[PERF] %s: %u ms", label, _diff_ms); \
    } while(0)
    // Legacy macros (deprecated - use manual timing instead)
    #define DBG_PERF_START() ((void)0)  // Deprecated - use manual TEE_GetSystemTime()
    #define DBG_PERF_END(label) ((void)0)  // Deprecated - use DBG_PERF_LOG() instead
#else
    #define DBG_PERF_START() ((void)0)
    #define DBG_PERF_END(label) ((void)0)
    #define DBG_PERF_LOG(label, start_time, end_time) ((void)0)
#endif

// ============================================================================
// Info/Error Macros (Always enabled)
// ============================================================================

#define INFO_LOG(fmt, ...) OCALL_LOG("[INFO] " fmt, ##__VA_ARGS__)
#define WARN_LOG(fmt, ...) OCALL_LOG("[WARN] " fmt, ##__VA_ARGS__)
#define ERROR_LOG(fmt, ...) OCALL_LOG("[ERROR] " fmt, ##__VA_ARGS__)

// ============================================================================
// Helper: Flush logs if buffer available
// ============================================================================

#define FLUSH_LOGS_IF_AVAILABLE(params) do { \
    if (params[2].memref.buffer && params[2].memref.size > 0) { \
        ocall_log_flush_to_params(params[2].memref.buffer, &params[2].memref.size); \
    } \
} while(0)

#endif // DEBUG_CONFIG_H

