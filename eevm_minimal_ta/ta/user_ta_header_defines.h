/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * The name of this file must not be modified
 */

#ifndef USER_TA_HEADER_DEFINES_H
#define USER_TA_HEADER_DEFINES_H

// Define MALLOC_INITIAL_POOL_MIN_SIZE BEFORE including minimal_evm_ta.h
// This prevents warning in user_ta_header.c which checks this value
#ifndef MALLOC_INITIAL_POOL_MIN_SIZE
#define MALLOC_INITIAL_POOL_MIN_SIZE 1024
#endif

#include <minimal_evm_ta.h>

#define TA_UUID				TA_MINIMAL_EVM_UUID

#define TA_FLAGS			0

// C++ STL (libcxx) requires larger heap/stack for initialization
// TA_DATA_SIZE is the actual heap in OP-TEE
#define TA_STACK_SIZE			(1 * 1024 * 1024)   // 1MB stack
#define TA_DATA_SIZE			(8 * 1024 * 1024)   // 8MB heap

#define TA_CURRENT_TA_EXT_PROPERTIES \
    { "gp.ta.description", USER_TA_PROP_TYPE_STRING, \
        "Minimal EVM TA with custom vector/map" }, \
    { "gp.ta.version", USER_TA_PROP_TYPE_U32, &(const uint32_t){ 0x0010 } }

#endif /* USER_TA_HEADER_DEFINES_H */
