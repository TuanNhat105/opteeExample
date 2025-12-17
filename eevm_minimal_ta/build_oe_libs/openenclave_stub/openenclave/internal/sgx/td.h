// Stub header for OpenEnclave SGX thread data
// OP-TEE doesn't use OpenEnclave's thread-local storage
#ifndef _OPENENCLAVE_INTERNAL_SGX_TD_H
#define _OPENENCLAVE_INTERNAL_SGX_TD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stub for oe_thread_data_t - used by libcxxrt for exception handling
typedef struct oe_thread_data {
    void* __cxx_thread_info;  // libcxxrt thread-local exception info
    void* dummy;
} oe_thread_data_t;

// Global thread data for single-threaded OP-TEE (defined in oe_thread_stub.c)
extern oe_thread_data_t __optee_global_thread_data;

// Return global thread data (OP-TEE is single-threaded)
oe_thread_data_t* oe_get_thread_data(void);

#ifdef __cplusplus
}
#endif

#endif // _OPENENCLAVE_INTERNAL_SGX_TD_H
