/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef EEVM_TA_H
#define EEVM_TA_H

/* UUID: 8aaaf200-2450-11e4-abe2-0002a5d5c53d */
#define TA_EEVM_UUID \
    { 0x8aaaf200, 0x2450, 0x11e4, \
        { 0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x3d } }

/* Commands for the TA */
#define TA_EEVM_CMD_HELLO_WORLD     0
#define TA_EEVM_CMD_OCALL_PRINT     1  // OCALL for printing from TA
#define TA_EEVM_CMD_TEST_STACK      2  // Test eEVM Stack operations
#define TA_EEVM_CMD_TEST_SIMPLE_SHM 7  // Simple test with shared memory (no LevelDB)
#define TA_EEVM_CMD_INIT_LEVELDB    3  // Initialize LevelDB with Ring Buffer
#define TA_EEVM_CMD_LEVELDB_PUT     4  // Put key-value to LevelDB
#define TA_EEVM_CMD_LEVELDB_GET     5  // Get value from LevelDB
#define TA_EEVM_CMD_LEVELDB_DELETE  6  // Delete key from LevelDB
#define TA_EEVM_CMD_TEST_SHM_STRING 8  // Test string return via shared memory
#define TA_EEVM_CMD_INIT_LOG_RING_BUFFER 9  // Initialize log ring buffer in shared memory
#define TA_EEVM_CMD_TEST_SHM_RING_BUFFER 10  // Test with ring buffer logging
#define TA_EEVM_CMD_TEST_LEVELDB_RAM 11  // Test LevelDB with RAM storage only


#endif /* EEVM_TA_H */
