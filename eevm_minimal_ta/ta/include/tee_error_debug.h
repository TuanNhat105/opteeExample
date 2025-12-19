/*
 * OP-TEE Error Code Debug Helpers
 * 
 * This file provides additional error code definitions and helpers
 * for easier debugging in our project.
 * 
 * Note: We don't modify the original OP-TEE error codes, but add
 * descriptive aliases and helper functions for debugging.
 */

#ifndef TEE_ERROR_DEBUG_H
#define TEE_ERROR_DEBUG_H

#include <tee_internal_api.h>

// Original OP-TEE error codes (for reference)
// TEE_ERROR_TARGET_DEAD = 0xFFFF3024 (used in TA)
// TEEC_ERROR_TARGET_DEAD = 0xFFFF3024 (used in host)

// Debug-friendly aliases with context
#define TEE_ERROR_TA_CRASHED          TEE_ERROR_TARGET_DEAD  // TA crashed/panicked
#define TEE_ERROR_EXCEPTION_FAILED    TEE_ERROR_TARGET_DEAD  // Exception handling failed
#define TEE_ERROR_UNWINDER_FAILED     TEE_ERROR_TARGET_DEAD  // Stack unwinder failed

// Helper function to get error code description
static inline const char* tee_error_to_string(TEE_Result res)
{
    switch (res) {
        case TEE_SUCCESS:
            return "TEE_SUCCESS";
        case TEE_ERROR_TARGET_DEAD:
            return "TEE_ERROR_TARGET_DEAD (TA crashed/exception failed)";
        case TEE_ERROR_GENERIC:
            return "TEE_ERROR_GENERIC";
        case TEE_ERROR_BAD_PARAMETERS:
            return "TEE_ERROR_BAD_PARAMETERS";
        case TEE_ERROR_OUT_OF_MEMORY:
            return "TEE_ERROR_OUT_OF_MEMORY";
        case TEE_ERROR_SHORT_BUFFER:
            return "TEE_ERROR_SHORT_BUFFER";
        case TEE_ERROR_NOT_SUPPORTED:
            return "TEE_ERROR_NOT_SUPPORTED";
        default:
            return "UNKNOWN_ERROR";
    }
}

// Macro for logging errors with description
#define LOG_TEE_ERROR(res, context) \
    do { \
        if ((res) != TEE_SUCCESS) { \
            EMSG("ERROR in %s: 0x%08X (%s)", \
                 (context), (res), tee_error_to_string(res)); \
        } \
    } while (0)

#endif /* TEE_ERROR_DEBUG_H */

