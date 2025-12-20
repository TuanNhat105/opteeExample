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

# libcxx archive (all C++ standard library objects)
LIBCXX := ../build_oe_libs/libcxx/libc++.a

# Complete OpenEnclave libc (musl + wrappers + pthread/dl_find_object stubs)
LIBOELIBC := ../libc/liboelibc.a

# libcxxrt exception runtime (guards, typeinfo, bad_cast, etc)
# Built by build_libcxxrt_complete.sh into build_oe_libs/libcxxrt/
LIBCXXRT := ../build_oe_libs/libcxxrt/libcxxrt.a

# Compiler runtime for long double operations and math intrinsics
# NOTE: We keep libgcc.a (NOT libgcc_eh.a) because it provides compiler intrinsics
# that libunwind doesn't provide (__divdi3, __moddi3, soft-float, etc)
# libunwind only replaces libgcc_eh.a (exception handling)
LIBGCC := $(shell $(CROSS_COMPILE)gcc -print-file-name=libgcc.a)

# libunwind for stack unwinding (replaces libgcc_eh.a ONLY, not libgcc.a)
# Built by build_libunwind_openenclave.sh into build_libunwind/
LIBUNWIND := ../build_libunwind/libunwind.a

$(BINARY).elf: $(objs) $(LIBCXX) $(LIBCXXRT) $(LIBOELIBC) $(LIBUNWIND) $(libdeps) $(libdeps-after-libgcc) $(link-script-ppta_arm64) $(dynlistdep) $(additional-link-deps)
	@$(cmd-echo-silent) '  LD      $@'
	$(q)$(LDta_arm64) --allow-multiple-definition $(filter-out %libstdc++.a %libgcc_eh.a,$(ldargs-$(BINARY).elf)) $(LIBCXX) $(LIBCXXRT) $(LIBOELIBC) $(LIBUNWIND) $(LIBOELIBC) $(LIBGCC) -o $@

ifeq ($(wildcard $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk), )
clean:
	@echo 'Note: $$(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk not found, cannot clean TA'
endif