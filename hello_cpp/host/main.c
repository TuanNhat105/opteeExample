/*
 * Copyright (c) 2024, Hello C++ Host Application
 */

#include <err.h>
#include <stdio.h>
#include <string.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* TA API: UUID and command IDs */
#include <hello_cpp_ta.h>

/* TEE resources */
struct test_ctx {
    TEEC_Context ctx;
    TEEC_Session sess;
};

void prepare_tee_session(struct test_ctx *ctx)
{
    TEEC_UUID uuid = TA_HELLO_CPP_UUID;
    uint32_t origin;
    TEEC_Result res;

    /* Initialize a context connecting us to the TEE */
    res = TEEC_InitializeContext(NULL, &ctx->ctx);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

    /* Open a session with the TA */
    res = TEEC_OpenSession(&ctx->ctx, &ctx->sess, &uuid,
                   TEEC_LOGIN_PUBLIC, NULL, NULL, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
            res, origin);
}

void terminate_tee_session(struct test_ctx *ctx)
{
    TEEC_CloseSession(&ctx->sess);
    TEEC_FinalizeContext(&ctx->ctx);
}

/*
 * Test basic C++ features
 */
void test_basic_cpp(struct test_ctx *ctx)
{
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;

    printf("\n=== Test 1: Basic C++ Features ===\n");

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
                     TEEC_NONE, TEEC_NONE);

    res = TEEC_InvokeCommand(&ctx->sess, TA_HELLO_CPP_CMD_TEST_BASIC,
                 &op, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand(TEST_BASIC) failed 0x%x origin 0x%x",
            res, origin);

    printf("✅ Test PASSED: Basic C++ (string, concatenation)\n");
}

/*
 * Test STL containers
 */
void test_stl_containers(struct test_ctx *ctx)
{
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;

    printf("\n=== Test 2: STL Containers ===\n");

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
                     TEEC_NONE, TEEC_NONE);

    res = TEEC_InvokeCommand(&ctx->sess, TA_HELLO_CPP_CMD_TEST_STL,
                 &op, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand(TEST_STL) failed 0x%x origin 0x%x",
            res, origin);

    printf("✅ Test PASSED: STL (vector, find, algorithms)\n");
}

/*
 * Test C++ class
 */
void test_class_usage(struct test_ctx *ctx)
{
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;

    printf("\n=== Test 3: C++ Class ===\n");

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
                     TEEC_NONE, TEEC_NONE);

    res = TEEC_InvokeCommand(&ctx->sess, TA_HELLO_CPP_CMD_TEST_CLASS,
                 &op, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand(TEST_CLASS) failed 0x%x origin 0x%x",
            res, origin);

    printf("✅ Test PASSED: C++ Class (OOP, constructor, methods)\n");
}

/*
 * Test data processing
 */
void test_process_data(struct test_ctx *ctx)
{
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;
    char input[] = "Hello from Normal World!";
    char output[256] = {0};

    printf("\n=== Test 4: Data Processing ===\n");
    printf("Input:  %s\n", input);

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
                     TEEC_MEMREF_TEMP_OUTPUT,
                     TEEC_NONE, TEEC_NONE);

    op.params[0].tmpref.buffer = input;
    op.params[0].tmpref.size = strlen(input);
    op.params[1].tmpref.buffer = output;
    op.params[1].tmpref.size = sizeof(output);

    res = TEEC_InvokeCommand(&ctx->sess, TA_HELLO_CPP_CMD_PROCESS_DATA,
                 &op, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand(PROCESS_DATA) failed 0x%x origin 0x%x",
            res, origin);

    printf("Output: %s\n", output);
    printf("✅ Test PASSED: Data processing (uppercase conversion)\n");
}

int main(void)
{
    struct test_ctx ctx;

    printf("====================================\n");
    printf("  Hello C++ Trusted Application\n");
    printf("  Testing C++ in OP-TEE\n");
    printf("====================================\n");

    prepare_tee_session(&ctx);

    /* Run all tests */
    test_basic_cpp(&ctx);
    test_stl_containers(&ctx);
    test_class_usage(&ctx);
    test_process_data(&ctx);

    printf("\n====================================\n");
    printf("  All tests completed successfully!\n");
    printf("====================================\n");

    terminate_tee_session(&ctx);

    return 0;
}
