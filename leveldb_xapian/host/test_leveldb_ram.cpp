// SPDX-License-Identifier: BSD-2-Clause
/*
 * Host application to test LevelDB with RAM storage in OP-TEE TA
 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* UUID of the trusted application */
#include "../ta/eevm_ta.h"

int main(void)
{
    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_EEVM_UUID;
    uint32_t err_origin;
    
    printf("=== LevelDB RAM Test (Host) ===\n");
    
    /* Initialize a context connecting us to the TEE */
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS) {
        errx(1, "TEEC_InitializeContext failed with code 0x%x", res);
    }
    
    /* Open a session to the TA */
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                          TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS) {
        errx(1, "TEEC_OpenSession failed with code 0x%x origin 0x%x",
            res, err_origin);
    }
    
    printf("[Host] Session opened successfully\n");
    
    /* Prepare buffers */
    char ocall_buffer[16 * 1024]; // 16KB for OCALL logs
    memset(ocall_buffer, 0, sizeof(ocall_buffer));
    
    /* Prepare operation parameters */
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_OUTPUT,  // OCALL logs
        TEEC_VALUE_OUTPUT,         // Result status
        TEEC_NONE,
        TEEC_NONE);
    
    op.params[0].tmpref.buffer = ocall_buffer;
    op.params[0].tmpref.size = sizeof(ocall_buffer);
    
    printf("[Host] Invoking TA_EEVM_CMD_TEST_LEVELDB_RAM...\n");
    
    /* Invoke the command */
    res = TEEC_InvokeCommand(&sess, TA_EEVM_CMD_TEST_LEVELDB_RAM, &op, &err_origin);
    
    if (res != TEEC_SUCCESS) {
        printf("[Host] TEEC_InvokeCommand failed with code 0x%x origin 0x%x\n",
               res, err_origin);
    } else {
        printf("[Host] Command executed successfully\n");
        printf("[Host] Result status: %u\n", op.params[1].value.a);
    }
    
    /* Print OCALL logs */
    printf("\n=== OCALL LOGS FROM TA ===\n");
    if (op.params[0].tmpref.size > 0) {
        printf("%s\n", (char*)ocall_buffer);
    } else {
        printf("(No logs)\n");
    }
    printf("=== END OF LOGS ===\n\n");
    
    /* Cleanup */
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
    
    printf("=== Test completed ===\n");
    
    return 0;
}

