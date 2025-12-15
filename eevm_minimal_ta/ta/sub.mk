global-incdirs-y += include

# Use OpenEnclave libcxx + musl (properly built following CMakeLists.txt)
global-incdirs-y += ../build_oe_libs/musl/include
global-incdirs-y += ../build_oe_libs/libcxx/include
global-incdirs-y += ../build_oe_libs/libcxxrt/include

# Source selection
srcs-y += minimal_evm_ta.cpp
srcs-y += cxx_stubs.cpp

# Enable C++17
cppflags-y += -std=c++17
cppflags-y += -fno-exceptions
cppflags-y += -fno-rtti
cppflags-y += -fno-threadsafe-statics
cppflags-y += -nostdinc++
cppflags-y += -nodefaultlibs

# libcxx configuration (match OpenEnclave)
cppflags-y += -U__STDCPP_THREADS__
cppflags-y += -D_LIBCPP_HAS_NO_THREADS
cppflags-y += -D_LIBCPP_HAS_NO_EXCEPTIONS
cppflags-y += -DLIBCXXRT

# Link flags: Don't use default C++ libs  
LDFLAGS += -nostdlib -nodefaultlibs
LDFLAGS += -Wl,--no-undefined
LDFLAGS += -Wl,--exclude-libs,ALL

# Link with OpenEnclave standalone library
LDADD += -L../build_oe_libs/standalone -lc++_standalone
LDADD += -lgcc
