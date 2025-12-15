/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * The name of this file must not be modified
 */

#ifndef USER_TA_HEADER_DEFINES_H
#define USER_TA_HEADER_DEFINES_H

#include <minimal_evm_ta.h>

#define TA_UUID				TA_MINIMAL_EVM_UUID

#define TA_FLAGS			0

#define TA_STACK_SIZE          (64 * 1024)
#define TA_DATA_SIZE           (512 * 1024)
#define TA_MALLOC_POOL_SIZE    (1024 * 1024)

#define TA_CURRENT_TA_EXT_PROPERTIES \
    { "gp.ta.description", USER_TA_PROP_TYPE_STRING, \
        "Minimal EVM TA with custom vector/map" }, \
    { "gp.ta.version", USER_TA_PROP_TYPE_U32, &(const uint32_t){ 0x0010 } }

#endif /* USER_TA_HEADER_DEFINES_H */
