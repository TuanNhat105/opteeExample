#ifndef HELLO_CPP_TA_H
#define HELLO_CPP_TA_H

/*
 * TA UUID: f4e750bb-1437-4fbf-8785-8d3580c34994
 * This UUID is generated with uuidgen
 */
#define TA_HELLO_CPP_UUID \
    { 0xf4e750bb, 0x1437, 0x4fbf, \
        { 0x87, 0x85, 0x8d, 0x35, 0x80, 0xc3, 0x49, 0x94 } }

/* Command IDs for TA */
#define TA_HELLO_CPP_CMD_TEST_BASIC       0  /* Test basic C++ features */
#define TA_HELLO_CPP_CMD_TEST_STL         1  /* Test STL containers */
#define TA_HELLO_CPP_CMD_TEST_CLASS       2  /* Test C++ class */
#define TA_HELLO_CPP_CMD_PROCESS_DATA     3  /* Process buffer data */
#define TA_HELLO_CPP_CMD_TEST_SMART_PTR   4  /* Test smart pointers (for eEVM) */

#endif /* HELLO_CPP_TA_H */
