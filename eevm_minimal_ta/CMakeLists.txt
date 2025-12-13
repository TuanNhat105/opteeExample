cmake_minimum_required(VERSION 3.13)
project(eevm_minimal_ta_with_runtime C CXX ASM)

# ===================================================
# COMPILER FLAGS - Critical for C++ Runtime
# ===================================================
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc -ffreestanding -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc -nostdinc++ -ffreestanding -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti -funwind-tables")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")

# ===================================================
# Include Paths
# ===================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxx/libcxx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxxrt/libcxxrt/src
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libunwind/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/musl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/musl/arch/aarch64
    ${CMAKE_CURRENT_SOURCE_DIR}/libc
    $ENV{TA_DEV_KIT_DIR}/include
)

# ===================================================
# LIBCXX (STL)
# ===================================================
file(GLOB LIBCXX_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxx/libcxx/src/*.cpp
)
list(FILTER LIBCXX_SOURCES EXCLUDE REGEX "locale|iostream|filesystem|thread|mutex")

add_library(cxx STATIC ${LIBCXX_SOURCES})
target_compile_options(cxx PRIVATE -fPIC -fvisibility=hidden)

# ===================================================
# LIBCXXRT (C++ ABI, Exceptions, RTTI)
# ===================================================
set(LIBCXXRT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxxrt/libcxxrt/src/auxhelper.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxxrt/libcxxrt/src/dynamic_cast.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxxrt/libcxxrt/src/exception.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxxrt/libcxxrt/src/guard.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxxrt/libcxxrt/src/stdexcept.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libcxxrt/libcxxrt/src/typeinfo.cc
)

add_library(cxxrt STATIC ${LIBCXXRT_SOURCES})
target_compile_options(cxxrt PRIVATE -fPIC -fvisibility=hidden)

# ===================================================
# LIBUNWIND (Stack Unwinding)
# ===================================================
file(GLOB LIBUNWIND_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libunwind/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libunwind/src/*.S
)

add_library(unwind STATIC ${LIBUNWIND_SOURCES})
target_compile_options(unwind PRIVATE -fPIC)

# ===================================================
# DLMALLOC (Allocator)
# ===================================================
add_library(dlmalloc STATIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/dlmalloc/dlmalloc.c
)
target_compile_options(dlmalloc PRIVATE -fPIC)

# ===================================================
# LIBC Runtime Support
# ===================================================
file(GLOB LIBC_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/libc/*.c
)

add_library(oecrt STATIC ${LIBC_SOURCES})
target_compile_options(oecrt PRIVATE -fPIC)

# ===================================================
# BUILD OUTPUT
# ===================================================
message(STATUS "================================================")
message(STATUS "C++ Runtime Libraries Configuration")
message(STATUS "================================================")
message(STATUS "libcxx:   STL support (vector, map, string)")
message(STATUS "libcxxrt: Exceptions, RTTI, guards")
message(STATUS "libunwind: Stack unwinding")
message(STATUS "dlmalloc: Memory allocator")
message(STATUS "oecrt:    C runtime support")
message(STATUS "================================================")
