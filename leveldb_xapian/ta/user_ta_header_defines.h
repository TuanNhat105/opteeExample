/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef USER_TA_HEADER_DEFINES_H
#define USER_TA_HEADER_DEFINES_H

/* Include config.h for IS_ENABLED macro before any other includes */
#ifndef __ASSEMBLER__
#include <config.h>
#endif

/* Fallback definition for IS_ENABLED if not already defined */
#ifndef IS_ENABLED
#define _CFG_TA_STACK_PROTECTOR 0
#define IS_ENABLED(x) (x)
#endif

#include <eevm_ta.h>

#define TA_UUID             TA_EEVM_UUID

#define TA_FLAGS            0

/* Stack and heap sizes for LevelDB + eEVM execution */
#define TA_STACK_SIZE       (32 * 1024 * 1024)  // 2MB stack
#define TA_DATA_SIZE        (32 * 1024 * 1024) // 16MB heap (increased for LevelDB)

/* Minimum malloc pool size */
#ifndef MALLOC_INITIAL_POOL_MIN_SIZE
#define MALLOC_INITIAL_POOL_MIN_SIZE (100 * 1024)  // 100KB
#endif

#define TA_CURRENT_TA_EXT_PROPERTIES \
    { "gp.ta.description", USER_TA_PROP_TYPE_STRING, \
        "eEVM Hello World TA" }, \
    { "gp.ta.version", USER_TA_PROP_TYPE_U32, &(const uint32_t){ 0x0001 } }

#endif /* USER_TA_HEADER_DEFINES_H */
