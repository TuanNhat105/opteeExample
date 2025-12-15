/*
 * Minimal EVM Host - Test C++ STL in TA
 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <tee_client_api.h>
#include "minimal_evm_ta.h"

int main(void)
{
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_MINIMAL_EVM_UUID;
	uint32_t err_origin;

	printf("====================================\n");
	printf("Minimal EVM TA - C++ STL Test\n");
	printf("OpenEnclave libcxx in OP-TEE\n");
	printf("====================================\n\n");

	/* Initialize context */
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	/* Open session */
	printf("Opening session with TA...\n");
	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);

	printf("Session opened successfully!\n\n");

	/* Test C++ STL */
	printf("Invoking TA command: TEST_CPP...\n");
	printf("------------------------------------\n");
	
	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
					 TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(&sess, TA_MINIMAL_EVM_CMD_TEST_CPP, &op,
				 &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x",
			res, err_origin);

	printf("------------------------------------\n\n");
	printf("✓ All tests completed successfully!\n");
	printf("✓ std::map is working in TA!\n");
	printf("✓ OpenEnclave libcxx integration successful!\n\n");

	/* Close session */
	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return 0;
}
