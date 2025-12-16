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
#define TA_MINIMAL_EVM_CMD_TEST_STRING       4  // New: test std::string with object linking

/* C++17 libcxx feature tests - OpenEnclave compatibility */
#define TA_MINIMAL_EVM_CMD_TEST_OPTIONAL     10
#define TA_MINIMAL_EVM_CMD_TEST_VARIANT      11
#define TA_MINIMAL_EVM_CMD_TEST_ANY          12
#define TA_MINIMAL_EVM_CMD_TEST_TUPLE        13
#define TA_MINIMAL_EVM_CMD_TEST_FUNCTIONAL   14
#define TA_MINIMAL_EVM_CMD_TEST_ALGORITHM    15
#define TA_MINIMAL_EVM_CMD_TEST_NUMERIC      16
#define TA_MINIMAL_EVM_CMD_TEST_MEMORY       17
#define TA_MINIMAL_EVM_CMD_TEST_DEQUE        18
#define TA_MINIMAL_EVM_CMD_TEST_LIST         19
#define TA_MINIMAL_EVM_CMD_TEST_SET          20
#define TA_MINIMAL_EVM_CMD_TEST_UNORDERED_MAP 21
#define TA_MINIMAL_EVM_CMD_TEST_QUEUE        22
#define TA_MINIMAL_EVM_CMD_TEST_EXCEPTION    23
#define TA_MINIMAL_EVM_CMD_TEST_ARRAY        24
#define TA_MINIMAL_EVM_CMD_TEST_FORWARD_LIST 25

#endif /* __MINIMAL_EVM_TA_H */
