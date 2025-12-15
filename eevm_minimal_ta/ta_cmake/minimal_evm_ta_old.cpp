/*
 * Minimal EVM TA - Testing std::map with OpenEnclave libcxx
 * Copyright (c) 2024
 */

// C++ standard library test
#include <map>
#include <string>
#include <vector>

// OP-TEE C headers (must be after C++ headers to avoid macro conflicts)
extern "C" {
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <minimal_evm_ta.h>
}

/*
 * Called when the instance of the TA is created.
 */
TEE_Result TA_CreateEntryPoint(void)
{
	DMSG("TA_CreateEntryPoint: OpenEnclave libcxx ready");
	return TEE_SUCCESS;
}

/*
 * Called when the instance of the TA is destroyed.
 */
void TA_DestroyEntryPoint(void)
{
	DMSG("TA_DestroyEntryPoint");
}

/*
 * Called when a new session is opened to the TA.
 */
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

/*
 * Called when a session is closed.
 */
void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
	(void)&sess_ctx;
	DMSG("TA_CloseSessionEntryPoint");
}

// C++ STL Test Functions
static void test_vector()
{
	DMSG("=== Testing std::vector ===");
	
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
	DMSG("=== Testing std::map ===");
	
	std::map<int, int> mymap;
	
	// Insert elements
	mymap[1] = 100;
	mymap[2] = 200;
	mymap[3] = 300;
	
	DMSG("Map size: %zu", mymap.size());
	DMSG("Map[1]: %d", mymap[1]);
	DMSG("Map[2]: %d", mymap[2]);
	DMSG("Map[3]: %d", mymap[3]);
	
	// Test find
	auto it = mymap.find(2);
	if (it != mymap.end()) {
		DMSG("Found key 2, value: %d", it->second);
	}
	
	// Test iteration
	DMSG("Iterating map:");
	for (const auto& pair : mymap) {
		DMSG("  Key: %d, Value: %d", pair.first, pair.second);
	}
	
	DMSG("✓ std::map test PASSED");
}

static void test_string()
{
	DMSG("=== Testing std::string ===");
	
	std::string str1 = "Hello";
	std::string str2 = " World";
	std::string str3 = str1 + str2;
	
	DMSG("String1: %s", str1.c_str());
	DMSG("String2: %s", str2.c_str());
	DMSG("String3: %s", str3.c_str());
	DMSG("String3 length: %zu", str3.length());
	
	DMSG("✓ std::string test PASSED");
}

static void test_map_with_string_keys()
{
	DMSG("=== Testing std::map<string, int> ===");
	
	std::map<std::string, int> accounts;
	
	accounts["Alice"] = 1000;
	accounts["Bob"] = 2000;
	accounts["Charlie"] = 3000;
	
	DMSG("Accounts map size: %zu", accounts.size());
	DMSG("Alice balance: %d", accounts["Alice"]);
	DMSG("Bob balance: %d", accounts["Bob"]);
	DMSG("Charlie balance: %d", accounts["Charlie"]);
	
	// Update balance
	accounts["Alice"] += 500;
	DMSG("Alice new balance: %d", accounts["Alice"]);
	
	DMSG("✓ std::map<string, int> test PASSED");
}
#endif

/*
 * Called when a TA is invoked.
 */
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[4])
{
	(void)&sess_ctx;

	switch (cmd_id) {
	case TA_MINIMAL_EVM_CMD_TEST_CPP:
		DMSG("==========================================");
		DMSG("C++ STL Testing with OpenEnclave libcxx");
		DMSG("==========================================");
		
#ifdef __cplusplus
		test_vector();
		DMSG("");
		
		test_map();
		DMSG("");
		
		test_string();
		DMSG("");
		
		test_map_with_string_keys();
		DMSG("");
		
		DMSG("==========================================");
		DMSG("ALL TESTS PASSED! 🎉");
		DMSG("std::map works with OpenEnclave libcxx!");
		DMSG("==========================================");
#else
		DMSG("ERROR: C++ not enabled");
		return TEE_ERROR_NOT_SUPPORTED;
#endif
		return TEE_SUCCESS;

	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

#ifdef __cplusplus
} // extern "C"
#endif
