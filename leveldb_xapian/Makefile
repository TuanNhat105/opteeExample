# SPDX-License-Identifier: BSD-2-Clause
# Main Makefile for eEVM OP-TEE application

.PHONY: all ta host clean

all: ta host

ta:
	$(MAKE) -C ta

host:
	$(MAKE) -C host

clean:
	$(MAKE) -C ta clean
	$(MAKE) -C host clean

help:
	@echo "eEVM OP-TEE Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all   - Build both TA and host application (default)"
	@echo "  ta    - Build only the Trusted Application"
	@echo "  host  - Build only the host application"
	@echo "  clean - Clean all build artifacts"
	@echo "  help  - Show this help message"
	@echo ""
	@echo "Required environment variables:"
	@echo "  TA_DEV_KIT_DIR  - Path to OP-TEE TA dev kit"
	@echo "  TEEC_EXPORT     - Path to OP-TEE client export"
	@echo "  CROSS_COMPILE   - Cross compiler prefix (optional)"
	@echo ""
	@echo "Example:"
	@echo "  export TA_DEV_KIT_DIR=/path/to/optee_os/out/arm/export-ta_arm64"
	@echo "  export TEEC_EXPORT=/path/to/optee_client/out/export"
	@echo "  export CROSS_COMPILE=aarch64-linux-gnu-"
	@echo "  make all"
	@echo ""
	@echo "Or use the build script:"
	@echo "  ./build.sh"
