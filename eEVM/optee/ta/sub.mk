# SPDX-License-Identifier: BSD-2-Clause

# TA source files
global-incdirs-y += include
srcs-y += eevm_ta_main.cpp
srcs-y += ocall_logger.cpp

# eEVM include directories (relative to eEVM root)
EEVM_MINIMAL_TA := ../../../eevm_minimal_ta
EEVM_ROOT := ../..

global-incdirs-y += $(EEVM_ROOT)/include
global-incdirs-y += $(EEVM_ROOT)/3rdparty
global-incdirs-y += $(EEVM_ROOT)/3rdparty/intx/include
global-incdirs-y += $(EEVM_ROOT)/src

# Include musl and libcxx headers
global-incdirs-y += $(EEVM_MINIMAL_TA)/build_oe_libs/musl/include
global-incdirs-y += $(EEVM_MINIMAL_TA)/build_oe_libs/libcxx/include
global-incdirs-y += $(EEVM_MINIMAL_TA)/build_oe_libs/libcxxrt/include
global-incdirs-y += $(EEVM_MINIMAL_TA)/build_libunwind

# List eEVM sources explicitly with full paths (relative to TA dir)
# Now fmt-free! Can include processor and stack
srcs-y += ../../src/processor.cpp
srcs-y += ../../src/stack.cpp
srcs-y += ../../src/transaction.cpp
srcs-y += ../../src/util.cpp
srcs-y += ../../src/disassembler.cpp
srcs-y += ../../src/simple/simpleaccount.cpp
srcs-y += ../../src/simple/simpleglobalstate.cpp
srcs-y += ../../src/simple/simplestorage.cpp

# # Keccak C sources
srcs-y += ../../3rdparty/keccak/KeccakHash.c
srcs-y += ../../3rdparty/keccak/KeccakP-1600-opt64.c
srcs-y += ../../3rdparty/keccak/KeccakSpongeWidth1600.c
srcs-y += ../../3rdparty/keccak/SimpleFIPS202.c

# intx library source
srcs-y += ../../3rdparty/intx/lib/intx/div.cpp

# Stub implementations for missing symbols
srcs-y += stubs.cpp

# C++ compilation flags (use cppflags-y for OP-TEE build system)
cppflags-y += -std=c++17
cppflags-y += -fno-rtti
cppflags-y += -fexceptions
cppflags-y += -nostdinc++
cppflags-y += -funwind-tables
cppflags-y += -fpermissive

# libcxx configuration (match OpenEnclave settings)
cppflags-y += -U__STDCPP_THREADS__
cppflags-y += -D_LIBCPP_HAS_NO_THREADS
cppflags-y += -DLIBCXXRT

# Suppress warnings
cppflags-y += -Wno-macro-redefined
cppflags-y += -Wno-builtin-declaration-mismatch

# Also for C files (Keccak)
cflags-y += -Wno-macro-redefined

# Link flags
LDFLAGS += -nostdlib -nodefaultlibs
LDFLAGS += -Wl,--no-undefined
LDFLAGS += -Wl,--as-needed
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--exclude-libs=libstdc++,libgcc_eh
LDFLAGS += -u __exidx_start -u __exidx_end
