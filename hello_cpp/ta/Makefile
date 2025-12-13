# User TA (Trusted Application) UUID
BINARY=f4e750bb-1437-4fbf-8785-8d3580c34994

# C++ source files - using minimal C++ (Level 1)
# For eEVM integration, consider using ETL or EASTL embedded libraries
# See docs/stl_challenge_report.md for details
srcs-y += hello_cpp_ta_simple.cpp

# Include directories
global-incdirs-y += include

# Enable C++ support with maximum logging
CFG_TEE_TA_LOG_LEVEL ?= 4

# Use TA development kit
-include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk
