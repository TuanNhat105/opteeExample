# SPDX-License-Identifier: BSD-2-Clause
CFG_TEE_TA_LOG_LEVEL ?= 4
BINARY=8aaaf200-2450-11e4-abe2-0002a5d5c53d

# Suppress warning about overriding recipe
MAKEFLAGS += --no-print-directory

# Paths to pre-built libraries (define BEFORE including ta_dev_kit.mk)
EEVM_MINIMAL_TA := ../../eevm_minimal_ta
EEVM_ROOT := ../..

# Prevent ta_dev_kit.mk from automatically adding libstdc++
override LDFLAGS += -nostdlib++

-include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk

# Override libstdc++ and libgcc_eh variables AFTER ta_dev_kit.mk
override libstdc++ta_arm64 :=
override libgcc_ehta_arm64 :=

# Remove any libstdc++ that ta_dev_kit.mk might have added
override LDADD := $(filter-out -lstdc++ -lstdc++_nonshared,$(LDADD))
override LDFLAGS += -Wl,--exclude-libs=libstdc++,libgcc_eh

# Library paths
LIBCXX := $(EEVM_MINIMAL_TA)/build_oe_libs/libcxx/libc++.a
LIBOELIBC := $(EEVM_MINIMAL_TA)/libc/liboelibc.a
LIBCXXRT := $(EEVM_MINIMAL_TA)/build_oe_libs/libcxxrt/libcxxrt.a
LIBGCC := $(shell $(CROSS_COMPILE)gcc -print-file-name=libgcc.a)
LIBUNWIND := $(EEVM_MINIMAL_TA)/build_libunwind/libunwind.a
# CRITICAL: libutee provides TEE_Malloc, TEE_Free, TEE_Panic, etc.
LIBUTEE := $(TA_DEV_KIT_DIR)/lib/libutee.a
# LIBDLMALLOC := $(EEVM_MINIMAL_TA)/build_dlmalloc/libdlmalloc.a

# Add custom libraries to link dependencies
# This modifies the default link rule to include our libraries

# $(BINARY).elf: $(objs) $(LIBCXX) $(LIBCXXRT) $(LIBOELIBC) $(LIBUNWIND) $(libdeps) $(libdeps-after-libgcc) $(link-script-ppta_arm64) $(dynlistdep) $(additional-link-deps)
# 	@$(cmd-echo-silent) '  LD      $@'
# 	$(q)$(LDta_arm64) --allow-multiple-definition $(filter-out %libstdc++.a %libgcc_eh.a,$(ldargs-$(BINARY).elf)) $(LIBCXX) $(LIBCXXRT) $(LIBOELIBC) $(LIBUNWIND) $(LIBOELIBC) $(LIBGCC) -o $@

$(BINARY).elf: $(objs) $(LIBCXX) $(LIBCXXRT) $(LIBOELIBC) $(LIBUNWIND) $(LIBUTEE)
	@$(cmd-echo-silent) '  LD      $@'
	$(q)$(LDta_arm64) --allow-multiple-definition \
		$(filter-out %libstdc++.a %libgcc_eh.a,$(ldargs-$(BINARY).elf)) \
		--start-group \
		 $(LIBCXX) $(LIBCXXRT) $(LIBOELIBC) $(LIBUNWIND) $(LIBUTEE) $(LIBGCC) \
		--end-group \
		-o $@

ifeq ($(wildcard $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk), )
clean:
	@echo 'Note: $$(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk not found, cannot clean TA'
endif
