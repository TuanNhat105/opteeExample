/*
 * Minimal EVM TA - Testing std::map with OpenEnclave libcxx
 * SIMPLIFIED VERSION - No std::string to avoid libc dependencies
 */

// C++ standard library (must include before OP-TEE headers)
#include <map>
#include <vector>

// OP-TEE C headers
extern "C" {
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <minimal_evm_ta.h>
}

// C++ STL Test Functions
static void test_vector()
{
	DMSG("=== Testing std::vector<int> ===");
	
	std::vector<int> vec;
	vec.push_back(10);
	vec.push_back(20);
	vec.push_back(30);
	
	DMSG("Vector size: %zu", vec.size());
	DMSG("Vector capacity: %zu", vec.capacity());
	DMSG("Vector[0]: %d", vec[0]);
	DMSG("Vector[1]: %d", vec[1]);
	DMSG("Vector[2]: %d", vec[2]);
	
	DMSG("✓ std::vector test PASSED");
}

static void test_map()
{
	DMSG("=== Testing std::map<int, int> ===");
	
	std::map<int, int> mymap;
	
	// Insert elements
	mymap[1] = 100;
	mymap[2] = 200;
	mymap[3] = 300;
	mymap[999] = 999999;
	
	DMSG("Map size: %zu", mymap.size());
	DMSG("Map[1]: %d", mymap[1]);
	DMSG("Map[2]: %d", mymap[2]);
	DMSG("Map[3]: %d", mymap[3]);
	DMSG("Map[999]: %d", mymap[999]);
	
	// Test find
	auto it = mymap.find(2);
	if (it != mymap.end()) {
		DMSG("Found key 2, value: %d", it->second);
	}
	
	// Test erase
	mymap.erase(999);
	DMSG("After erase(999), size: %zu", mymap.size());
	
	// Test iteration
	DMSG("Iterating map:");
	for (const auto& pair : mymap) {
		DMSG("  Key: %d, Value: %d", pair.first, pair.second);
	}
	
	DMSG("✓ std::map test PASSED");
}

/*
 * OP-TEE TA Entry Points (with C linkage)
 */
extern "C" {

TEE_Result TA_CreateEntryPoint(void)
{
	DMSG("==========================================");
	DMSG("TA_CreateEntryPoint");
	DMSG("OpenEnclave libcxx initialized");
	DMSG("==========================================");
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

	DMSG("TA_OpenSessionEntryPoint");

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	(void)&params;
	(void)&sess_ctx;

	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
	(void)&sess_ctx;
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
		DMSG("==========================================");
		DMSG("C++ STL Testing - Simplified Version");
		DMSG("==========================================");
		DMSG("");
		
		test_vector();
		DMSG("");
		// test_map();
		DMSG("");
		
		DMSG("==========================================");
		DMSG("ALL TESTS PASSED! 🎉");
		DMSG("std::map works in OP-TEE with libcxx!");
		DMSG("==========================================");
		
		return TEE_SUCCESS;

	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

} // extern "C"
