# SPDX-License-Identifier: BSD-2-Clause

# ============================================================================
# Debug Configuration
# ============================================================================
# Set these flags to control debug output:
#   DEBUG_ENABLED=1      - Enable all debug logs (default: 1)
#   DEBUG_PARAMS=1       - Enable parameter logging (default: 1)
#   DEBUG_STEPS=1        - Enable step-by-step logging (default: 1)
#   DEBUG_TEST_CODE=1    - Enable test code (memory access tests) (default: 1)
#   DEBUG_PERF=0         - Enable performance timing (default: 0)
#
# Example: To disable all debug logs, add to build command:
#   make DEBUG_ENABLED=0
# ============================================================================

# TA source files
global-incdirs-y += include
global-incdirs-y += ../include

# Debug flags (can be overridden from command line)
cppflags-y += -DDEBUG_ENABLED=$(or $(DEBUG_ENABLED),1)
cppflags-y += -DDEBUG_PARAMS=$(or $(DEBUG_PARAMS),1)
cppflags-y += -DDEBUG_STEPS=$(or $(DEBUG_STEPS),1)
cppflags-y += -DDEBUG_TEST_CODE=$(or $(DEBUG_TEST_CODE),1)
cppflags-y += -DDEBUG_PERF=$(or $(DEBUG_PERF),0)
# Use simple buffer version (no LevelDB ring buffer for now)
srcs-y += leveldb_ta_main.cpp
srcs-y += ocall_logger.cpp

# eEVM include directories (relative to eEVM root)
EEVM_MINIMAL_TA := ../../eevm_minimal_ta
EEVM_ROOT := ../..
LEVELDB_ROOT := ../leveldb

global-incdirs-y += $(EEVM_ROOT)/include
global-incdirs-y += $(EEVM_ROOT)/3rdparty
global-incdirs-y += $(EEVM_ROOT)/3rdparty/intx/include
global-incdirs-y += $(EEVM_ROOT)/src

# LevelDB include directories
global-incdirs-y += $(LEVELDB_ROOT)/include
global-incdirs-y += $(LEVELDB_ROOT)

# Include musl and libcxx headers
global-incdirs-y += $(EEVM_MINIMAL_TA)/build_oe_libs/musl/include
global-incdirs-y += $(EEVM_MINIMAL_TA)/build_oe_libs/libcxx/include
global-incdirs-y += $(EEVM_MINIMAL_TA)/build_oe_libs/libcxxrt/include
global-incdirs-y += $(EEVM_MINIMAL_TA)/build_libunwind


# LevelDB sources (using symlinks .cpp -> .cc for OP-TEE compatibility)
# Run create_leveldb_symlinks.sh first to generate these
srcs-y += leveldb_cpp/db/builder.cpp
srcs-y += leveldb_cpp/db/db_impl.cpp
srcs-y += leveldb_cpp/db/db_iter.cpp
srcs-y += leveldb_cpp/db/dbformat.cpp
srcs-y += leveldb_cpp/db/filename.cpp
srcs-y += leveldb_cpp/db/log_reader.cpp
srcs-y += leveldb_cpp/db/log_writer.cpp
srcs-y += leveldb_cpp/db/memtable.cpp
srcs-y += leveldb_cpp/db/repair.cpp
srcs-y += leveldb_cpp/db/table_cache.cpp
srcs-y += leveldb_cpp/db/version_edit.cpp
srcs-y += leveldb_cpp/db/version_set.cpp
srcs-y += leveldb_cpp/db/write_batch.cpp
srcs-y += leveldb_cpp/table/block.cpp
srcs-y += leveldb_cpp/table/block_builder.cpp
srcs-y += leveldb_cpp/table/filter_block.cpp
srcs-y += leveldb_cpp/table/format.cpp
srcs-y += leveldb_cpp/table/iterator.cpp
srcs-y += leveldb_cpp/table/merger.cpp
srcs-y += leveldb_cpp/table/table.cpp
srcs-y += leveldb_cpp/table/table_builder.cpp
srcs-y += leveldb_cpp/table/two_level_iterator.cpp
srcs-y += leveldb_cpp/util/arena.cpp
srcs-y += leveldb_cpp/util/bloom.cpp
srcs-y += leveldb_cpp/util/cache.cpp
srcs-y += leveldb_cpp/util/coding.cpp
srcs-y += leveldb_cpp/util/comparator.cpp
srcs-y += leveldb_cpp/util/crc32c.cpp
srcs-y += leveldb_cpp/util/env.cpp
srcs-y += leveldb_cpp/util/filter_policy.cpp
srcs-y += leveldb_cpp/util/hash.cpp
srcs-y += leveldb_cpp/util/histogram.cpp
srcs-y += leveldb_cpp/util/logging.cpp
srcs-y += leveldb_cpp/util/options.cpp
srcs-y += leveldb_cpp/util/status.cpp

# # Keccak C sources
# srcs-y += ../../3rdparty/keccak/KeccakHash.c
# srcs-y += ../../3rdparty/keccak/KeccakP-1600-opt64.c
# srcs-y += ../../3rdparty/keccak/KeccakSpongeWidth1600.c
# srcs-y += ../../3rdparty/keccak/SimpleFIPS202.c

# intx library source
# srcs-y += ../../3rdparty/intx/lib/intx/div.cpp

# Stub implementations for missing symbols
srcs-y += stubs.cpp
srcs-y += env_default_stub.cpp

# C++ compilation flags (use cppflags-y for OP-TEE build system)
cppflags-y += -std=c++17
cppflags-y += -fno-rtti
cppflags-y += -fexceptions
cppflags-y += -nostdinc++
cppflags-y += -funwind-tables
cppflags-y += -fpermissive

# libcxx configuration (Enable threads support for std::atomic)
cppflags-y += -U__STDCPP_THREADS__
# cppflags-y += -D_LIBCPP_HAS_NO_THREADS  # REMOVED: We need atomic support
cppflags-y += -DLIBCXXRT

# LevelDB configuration for OP-TEE
cppflags-y += -DLEVELDB_PLATFORM_TEE
cppflags-y += -DLEVELDB_ATOMIC_PRESENT

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
