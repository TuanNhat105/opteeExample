# ===================================================================
# C++ SUPPORT FOR OP-TEE TA (Level 1 - Minimal C++)
# ===================================================================
# NOTE: Full STL (Level 3) is not feasible with GCC 14 libstdc++ in TEE
# See docs/stl_challenge_report.md for details and alternatives (ETL/EASTL)

# C++ standard and compiler flags
cppflags-y += -std=c++17
cppflags-y += -fno-exceptions              # No exception support
cppflags-y += -fno-rtti                    # No runtime type info
cppflags-y += -fno-threadsafe-statics      # No thread-safe static init

# Link with C++ standard library (minimal runtime only)
ldflags-y += -static -lstdc++

# Debugging
ifeq ($(CFG_TEE_TA_LOG_LEVEL),4)
cppflags-y += -g -O0
else
cppflags-y += -O2
endif
