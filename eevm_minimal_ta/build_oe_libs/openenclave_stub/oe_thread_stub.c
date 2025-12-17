// OP-TEE stub for OpenEnclave thread data
// Provides global thread-local storage for single-threaded OP-TEE environment

#include <openenclave/internal/sgx/td.h>

// Global thread data for single-threaded OP-TEE
// This is initialized to zero, so __cxx_thread_info starts as NULL
// libcxxrt will allocate and set it on first use
oe_thread_data_t __optee_global_thread_data = {NULL, NULL};

// Return global thread data (OP-TEE is single-threaded)
oe_thread_data_t* oe_get_thread_data(void) {
    return &__optee_global_thread_data;
}
