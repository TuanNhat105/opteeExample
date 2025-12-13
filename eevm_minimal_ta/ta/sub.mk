global-incdirs-y += include

# Full musl + libcxx runtime includes
global-incdirs-y += ../build_full_musl_libcxx/include
global-incdirs-y += ../external/openenclave/3rdparty/libcxx/libcxx/include

# Source selection
srcs-y += minimal_evm_ta.cpp

# Enable C++17
cppflags-y += -std=c++17
cppflags-y += -fno-exceptions
cppflags-y += -fno-rtti
cppflags-y += -fno-threadsafe-statics
cppflags-y += -nostdinc++

# libcxx configuration
cppflags-y += -U__STDCPP_THREADS__
cppflags-y += -D_LIBCPP_HAS_NO_THREADS
cppflags-y += -D_LIBCPP_HAS_NO_EXCEPTIONS
cppflags-y += -D_LIBCPP_HAS_NO_RTTI

# Link with full musl runtime
libnames += musl cxx
libdirs += ../build_full_musl_libcxx
libdeps += ../build_full_musl_libcxx/libmusl.a
libdeps += ../build_full_musl_libcxx/libcxx.a
