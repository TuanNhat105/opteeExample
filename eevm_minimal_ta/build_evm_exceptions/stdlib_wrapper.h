#ifndef _STDLIB_WRAPPER_H
#define _STDLIB_WRAPPER_H

/* Use musl's stdlib.h which doesn't have fallthrough conflicts */
#include "/home/abc/nhat/optee_examples/eevm_minimal_ta/build_full_musl_libcxx/include/stdlib.h"

/* Declare TEE functions that might be needed */
#include <stddef.h>
#include <string.h>

#endif /* _STDLIB_WRAPPER_H */
