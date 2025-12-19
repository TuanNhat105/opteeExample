// Stub for OpenEnclave's td.h (thread data)
// For OP-TEE single-threaded environment

#ifndef _OE_INTERNAL_SGX_TD_H
#define _OE_INTERNAL_SGX_TD_H

#ifdef __cplusplus
extern "C" {
#endif

// Minimal thread data structure for exception handling
typedef struct _oe_thread_data
{
    void* exception_ptr;       // For __cxa_current_primary_exception
    void* __cxx_thread_info;   // For libcxxrt thread info (__cxa_thread_info*)
    void* tsd[16];             // Thread-specific data slots
} oe_thread_data_t;

// Get thread data - single-threaded stub
static inline oe_thread_data_t* oe_get_thread_data(void)
{
    static __thread oe_thread_data_t _thread_data = {0};
    return &_thread_data;
}

#ifdef __cplusplus
}
#endif

#endif /* _OE_INTERNAL_SGX_TD_H */
