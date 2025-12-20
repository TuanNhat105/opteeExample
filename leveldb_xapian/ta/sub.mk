# SPDX-License-Identifier: BSD-2-Clause

# TA source files
global-incdirs-y += include
global-incdirs-y += ../include
srcs-y += leveldb_ta_main.cpp
srcs-y += ocall_logger.cpp

# eEVM include directories (relative to eEVM root)
EEVM_MINIMAL_TA := ../../../eevm_minimal_ta
EEVM_ROOT := ../..
LEVELDB_ROOT := /home/abc/nhat/leveldb

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


# LevelDB sources (core files needed for TA)
srcs-y += $(LEVELDB_ROOT)/db/builder.cc
srcs-y += $(LEVELDB_ROOT)/db/db_impl.cc
srcs-y += $(LEVELDB_ROOT)/db/db_iter.cc
srcs-y += $(LEVELDB_ROOT)/db/dbformat.cc
srcs-y += $(LEVELDB_ROOT)/db/filename.cc
srcs-y += $(LEVELDB_ROOT)/db/log_reader.cc
srcs-y += $(LEVELDB_ROOT)/db/log_writer.cc
srcs-y += $(LEVELDB_ROOT)/db/memtable.cc
srcs-y += $(LEVELDB_ROOT)/db/repair.cc
srcs-y += $(LEVELDB_ROOT)/db/table_cache.cc
srcs-y += $(LEVELDB_ROOT)/db/version_edit.cc
srcs-y += $(LEVELDB_ROOT)/db/version_set.cc
srcs-y += $(LEVELDB_ROOT)/db/write_batch.cc
srcs-y += $(LEVELDB_ROOT)/table/block.cc
srcs-y += $(LEVELDB_ROOT)/table/block_builder.cc
srcs-y += $(LEVELDB_ROOT)/table/filter_block.cc
srcs-y += $(LEVELDB_ROOT)/table/format.cc
srcs-y += $(LEVELDB_ROOT)/table/iterator.cc
srcs-y += $(LEVELDB_ROOT)/table/merger.cc
srcs-y += $(LEVELDB_ROOT)/table/table.cc
srcs-y += $(LEVELDB_ROOT)/table/table_builder.cc
srcs-y += $(LEVELDB_ROOT)/table/two_level_iterator.cc
srcs-y += $(LEVELDB_ROOT)/util/arena.cc
srcs-y += $(LEVELDB_ROOT)/util/bloom.cc
srcs-y += $(LEVELDB_ROOT)/util/cache.cc
srcs-y += $(LEVELDB_ROOT)/util/coding.cc
srcs-y += $(LEVELDB_ROOT)/util/comparator.cc
srcs-y += $(LEVELDB_ROOT)/util/crc32c.cc
srcs-y += $(LEVELDB_ROOT)/util/env.cc
srcs-y += $(LEVELDB_ROOT)/util/env_posix.cc
srcs-y += $(LEVELDB_ROOT)/util/filter_policy.cc
srcs-y += $(LEVELDB_ROOT)/util/hash.cc
srcs-y += $(LEVELDB_ROOT)/util/histogram.cc
srcs-y += $(LEVELDB_ROOT)/util/logging.cc
srcs-y += $(LEVELDB_ROOT)/util/options.cc
srcs-y += $(LEVELDB_ROOT)/util/status.cc

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
