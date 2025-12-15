/*
 * Minimal TA - Test basic load without C++ STL
 */

extern "C" {
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <minimal_evm_ta.h>
}

extern "C" {

TEE_Result TA_CreateEntryPoint(void)
{
	DMSG("========================================");
	DMSG("TA_CreateEntryPoint - Basic test");
	DMSG("No C++ STL used yet");
	DMSG("========================================");
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
	DMSG("TA_DestroyEntryPoint");
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void __maybe_unused **sess_ctx)
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	DMSG("TA_OpenSessionEntryPoint - SUCCESS");

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
	DMSG("TA_CloseSessionEntryPoint");
}

TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[4])
{
	(void)&sess_ctx;
	(void)&param_types;
	(void)&params;

	switch (cmd_id) {
	case TA_MINIMAL_EVM_CMD_TEST_CPP:
		DMSG("========================================");
		DMSG("Basic TA test - NO C++ STL");
		DMSG("TA loaded successfully!");
		DMSG("========================================");
		return TEE_SUCCESS;

	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

} // extern "C"
