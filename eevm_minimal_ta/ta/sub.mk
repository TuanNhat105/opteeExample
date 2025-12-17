global-incdirs-y += include

# Use OpenEnclave libcxx + musl (properly built following CMakeLists.txt)
# Include our fixed headers first, then musl
global-incdirs-y += include
global-incdirs-y += ../build_oe_libs/musl/include
global-incdirs-y += ../build_oe_libs/libcxx/include
global-incdirs-y += ../build_oe_libs/libcxxrt/include

# Source selection
srcs-y += minimal_evm_ta.cpp
srcs-y += cxx_stubs.cpp

# Enable C++17
cppflags-y += -std=c++17
cppflags-y += -fexceptions
cppflags-y += -frtti
cppflags-y += -funwind-tables
cppflags-y += -nostdinc
cppflags-y += -nostdinc++
# cppflags-y += -nodefaultlibs
cppflags-y += -ffreestanding


# libcxx configuration (match OpenEnclave)
cppflags-y += -U__STDCPP_THREADS__
cppflags-y += -D_LIBCPP_HAS_NO_THREADS
cppflags-y += -DLIBCXXRT
# Undefine fallthrough macro to avoid conflict with libcxx __has_attribute(fallthrough)
# Note: __section conflict is handled by including libcxx_compat.h in source files
cppflags-y += -Ufallthrough

# Suppress harmless warnings
# - NULL redefinition: harmless conflict between musl (0L) and gcc (__null)
#   This happens because musl and gcc both define NULL differently
#   We suppress this warning as both definitions are functionally equivalent
cppflags-y += -Wno-macro-redefined
cppflags-y += -Wno-builtin-declaration-mismatch
# Also suppress for C compiler
cflags-y += -Wno-macro-redefined

# Link flags: Don't use default C++ libs  
LDFLAGS += -nostdlib -nodefaultlibs
LDFLAGS += -Wl,--no-undefined
# LDFLAGS += -Wl,--exclude-libs,ALL
# Prevent linking with libstdc++ from toolchain
LDFLAGS += -Wl,--as-needed
LDFLAGS += -Wl,--gc-sections
# Explicitly prevent libstdc++ and libgcc_eh
LDFLAGS += -Wl,--exclude-libs=libstdc++,libgcc_eh
LDFLAGS += -u __exidx_start -u __exidx_end

# Only link libgcc (objects added via objs variable above)
LDADD += -lgcc

# Note: We override link-ldadd in Makefile, not here
# sub.mk is included before link.mk, so we can't override here
