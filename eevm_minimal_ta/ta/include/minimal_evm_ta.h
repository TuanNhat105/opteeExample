// SPDX-License-Identifier: BSD-2-Clause
/*
 * Minimal EVM TA Header
 */

#ifndef __MINIMAL_EVM_TA_H
#define __MINIMAL_EVM_TA_H

// UUID: 8aaaf200-2450-11e4-abe2-0002a5d5c51b
#define TA_MINIMAL_EVM_UUID \
    { 0x8aaaf200, 0x2450, 0x11e4, \
        { 0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }

/*
 * Commands
 */

#define TA_MINIMAL_EVM_CMD_TEST_VECTOR       0
#define TA_MINIMAL_EVM_CMD_TEST_MAP          1
#define TA_MINIMAL_EVM_CMD_EXECUTE_BYTECODE  3
#define TA_MINIMAL_EVM_CMD_TEST_STRING       4  // Test std::string

/* OCALL support - RPC logging */
#define TA_MINIMAL_EVM_CMD_TEST_STRING_OCALL 6

/* Host callback UUID for OCALL - same as main TA for simplicity */
#define HOST_CALLBACK_UUID TA_MINIMAL_EVM_UUID

/* Host callback commands */
#define HOST_CMD_OCALL_PRINT    100  // Print string to host console

#endif /* __MINIMAL_EVM_TA_H */
