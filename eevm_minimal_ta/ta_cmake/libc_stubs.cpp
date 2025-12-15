/*
 * Minimal C stubs for libcxx in OP-TEE TA
 */

// Only include what we need - avoid full tee_api.h which pulls in libcxx headers
extern "C" {

// Forward declare TEE_Malloc
void *TEE_Malloc(unsigned long size, unsigned long hint);
#define TEE_MALLOC_FILL_ZERO 0

// posix_memalign stub - use TEE_Malloc
int posix_memalign(void **memptr, unsigned long alignment, unsigned long size)
{
    (void)alignment; // Ignore alignment for now
    *memptr = TEE_Malloc(size, TEE_MALLOC_FILL_ZERO);
    return (*memptr == ((void*)0)) ? -1 : 0;
}

} // extern "C"
