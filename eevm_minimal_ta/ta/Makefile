# SPDX-License-Identifier: BSD-2-Clause
CFG_TEE_TA_LOG_LEVEL ?= 4
BINARY=8aaaf200-2450-11e4-abe2-0002a5d5c51b

# Suppress warning about overriding recipe (we intentionally override .elf target)
MAKEFLAGS += --no-print-directory

# Prevent ta_dev_kit.mk from automatically adding libstdc++
# We use OpenEnclave standalone libcxx instead
override LDFLAGS += -nostdlib++

-include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk

# Override libstdc++ and libgcc_eh variables BEFORE link.mk uses them
# These are set in gcc.mk and used in link.mk: link-ldadd += $(libstdc++$(sm)) $(libgcc_eh$(sm))
# sm is set to ta_arm64 in conf.mk
# We must override them to empty strings so link.mk doesn't add them
override libstdc++ta_arm64 :=
override libgcc_ehta_arm64 :=

# Remove any libstdc++ that ta_dev_kit.mk might have added
override LDADD := $(filter-out -lstdc++ -lstdc++_nonshared,$(LDADD))
override LDFLAGS += -Wl,--exclude-libs=libstdc++,libgcc_eh

# libcxx object files to link (strong symbols, not weak)
LIBCXX_OBJS := ../build_oe_libs/libcxx/string.o \
               ../build_oe_libs/libcxx/charconv.o \
               ../build_oe_libs/libcxx/vector.o \
               ../build_oe_libs/libcxx/bind.o \
               ../build_oe_libs/libcxx/functional.o \
               ../build_oe_libs/libcxx/memory.o \
               ../build_oe_libs/libcxx/new.o \
               ../build_oe_libs/libcxx/optional.o \
               ../build_oe_libs/libcxx/stdexcept.o \
               ../build_oe_libs/libcxx/typeinfo.o \
               ../build_oe_libs/libcxx/utility.o \
               ../build_oe_libs/libcxx/variant.o \
               ../build_oe_libs/libcxx/__dso_handle.o \
               ../build_oe_libs/libcxxrt/stdexcept.o

# Complete OpenEnclave libc (musl + wrappers) - provides everything we need
LIBOELIBC := ../libc/liboelibc.a

# Compiler runtime for long double operations
LIBGCC := $(shell $(CROSS_COMPILE)gcc -print-file-name=libgcc.a)

# Override the .elf target to filter out libstdc++ and libgcc_eh from linker command
# Link with libcxx objects + complete oelibc (musl + wrappers) + libgcc for full C++ & C runtime
# Suppress warning about overriding recipe (we intentionally override to fix libstdc++ issue)
$(BINARY).elf: $(objs) $(LIBCXX_OBJS) $(LIBOELIBC) $(libdeps) $(libdeps-after-libgcc) $(link-script-ppta_arm64) $(dynlistdep) $(additional-link-deps)
	@$(cmd-echo-silent) '  LD      $@'
	$(q)$(LDta_arm64) $(filter-out %libstdc++.a %libgcc_eh.a,$(ldargs-$(BINARY).elf)) $(LIBCXX_OBJS) $(LIBOELIBC) $(LIBGCC) -o $@

ifeq ($(wildcard $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk), )
clean:
	@echo 'Note: $$(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk not found, cannot clean TA'
endif
