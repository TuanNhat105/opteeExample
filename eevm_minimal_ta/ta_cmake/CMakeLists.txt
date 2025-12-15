# TA CMakeLists.txt - Minimal EVM TA with OpenEnclave libcxx
cmake_minimum_required(VERSION 3.13)

# ===================================================
# TA Source Files
# ===================================================
set(TA_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/minimal_evm_ta.cpp
    ${TA_DEV_KIT_DIR}/src/user_ta_header.c
    ${CMAKE_CURRENT_SOURCE_DIR}/user_ta_header_defines.h
)

# ===================================================
# TA Compile Flags (from OP-TEE ta_dev_kit.mk)
# ===================================================
set(TA_COMPILE_FLAGS
    # Architecture flags
    -mcpu=cortex-a76
    -mstrict-align
    -fno-short-enums
    -fno-common
    -mgeneral-regs-only
    
    # Security flags
    -fstack-protector-strong
    -fno-omit-frame-pointer
    -fno-builtin
    
    # OP-TEE specific
    -DTRACE_LEVEL=4
    -D__KERNEL__
    -DCFG_TEE_TA_LOG_LEVEL=4
    
    # Position independent code
    -fpie
    
    # libcxx requirements (following OpenEnclave)
    -D_LIBCPP_HAS_NO_THREADS
    -D_LIBCPP_HAS_NO_EXCEPTIONS
    -D_LIBCPP_HAS_MUSL_LIBC
    -DLIBCXXRT
    -fno-exceptions
    -fno-rtti
    -U__STDCPP_THREADS__
    -Ufallthrough
    
    # No standard libraries
    -nostdlib
    -nostdinc
    -nostdinc++
)

# ===================================================
# TA Include Directories
# ===================================================
set(TA_INCLUDES
    # OpenEnclave includes (order matters!)
    ${LIBCXX_INCLUDE}
    ${LIBCXXRT_INCLUDE}
    ${MUSL_INCLUDE}
    
    # OP-TEE includes
    ${TA_DEV_KIT_DIR}/include
    ${TA_DEV_KIT_DIR}/host_include
    
    # Local includes
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ===================================================
# TA Link Flags
# ===================================================
set(TA_LINK_FLAGS
    -nostdlib
    -nodefaultlibs
    -nostartfiles
    -fpie
    -Wl,-pie
    -Wl,--sort-section=alignment
    -Wl,--as-needed
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TA_UUID}.map
    -Wl,--defsym,ARM64=1
    -Wl,--undefined=__ta_entry
    -Wl,--undefined=ta_head
    
    # Use custom TA linker script (preprocess it)
    -Wl,--script=${CMAKE_CURRENT_BINARY_DIR}/ta.lds
)

# ===================================================
# Build TA Binary
# ===================================================

# Preprocess linker script
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ta.lds
    COMMAND ${CMAKE_C_COMPILER} -E -P -DARM64 
        -I${TA_DEV_KIT_DIR}/include
        ${TA_DEV_KIT_DIR}/src/ta.ld.S 
        -o ${CMAKE_CURRENT_BINARY_DIR}/ta.lds
    DEPENDS ${TA_DEV_KIT_DIR}/src/ta.ld.S
    COMMENT "Preprocessing linker script"
)

add_custom_target(ta_lds DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ta.lds)

# Build C++ TA main code
add_library(ta_cpp_objects OBJECT 
    ${CMAKE_CURRENT_SOURCE_DIR}/minimal_evm_ta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libc_stubs.cpp
)

target_compile_options(ta_cpp_objects PRIVATE ${TA_COMPILE_FLAGS})
target_include_directories(ta_cpp_objects PRIVATE ${TA_INCLUDES})
target_compile_definitions(ta_cpp_objects PRIVATE -D__ILP32__)

# Build C user_ta_header separately (without libcxx includes)
add_library(ta_header_objects OBJECT ${TA_DEV_KIT_DIR}/src/user_ta_header.c)

target_compile_options(ta_header_objects PRIVATE
    -mcpu=cortex-a76
    -mstrict-align
    -fno-short-enums
    -fno-common
    -mgeneral-regs-only
    -fstack-protector-strong
    -fno-omit-frame-pointer
    -fno-builtin
    -DTRACE_LEVEL=4
    -D__KERNEL__
    -DCFG_TEE_TA_LOG_LEVEL=4
    -fpie
    -nostdlib
    -isystem /home/abc/arm-toolchain/lib/gcc/aarch64-none-linux-gnu/14.3.1/include
)

target_include_directories(ta_header_objects PRIVATE
    # Only OP-TEE includes for C code
    ${TA_DEV_KIT_DIR}/include
    ${TA_DEV_KIT_DIR}/host_include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Don't define __ILP32__ for user_ta_header - let compiler decide

# Create TA executable
add_executable(${TA_UUID}.elf 
    $<TARGET_OBJECTS:ta_cpp_objects>
    $<TARGET_OBJECTS:ta_header_objects>
)

add_dependencies(${TA_UUID}.elf ta_lds)

target_link_options(${TA_UUID}.elf PRIVATE ${TA_LINK_FLAGS})

# Link libraries (order critical!)
target_link_libraries(${TA_UUID}.elf
    # OpenEnclave libcxx (standalone with weak symbols)
    ${LIBCXX_STANDALONE}
    
    # OP-TEE libraries
    ${TA_DEV_KIT_DIR}/lib/libutils.a
    ${TA_DEV_KIT_DIR}/lib/libutee.a
    ${TA_DEV_KIT_DIR}/lib/libmbedtls.a
    ${TA_DEV_KIT_DIR}/lib/libutils.a
    
    # Compiler runtime
    gcc
)

# ===================================================
# Sign TA (OP-TEE requirement)
# ===================================================
add_custom_command(
    OUTPUT ${TA_UUID}.ta
    COMMAND ${TA_DEV_KIT_DIR}/scripts/sign_encrypt.py
        --key /home/abc/optee_os/keys/default_ta.pem
        --uuid ${TA_UUID}
        --ta-version 0
        --in ${CMAKE_CURRENT_BINARY_DIR}/${TA_UUID}.elf
        --out ${CMAKE_CURRENT_BINARY_DIR}/${TA_UUID}.ta
    DEPENDS ${TA_UUID}.elf
    COMMENT "Signing TA binary..."
)

add_custom_target(ta_signed ALL DEPENDS ${TA_UUID}.ta)

# ===================================================
# Install TA
# ===================================================
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TA_UUID}.ta
        DESTINATION lib/optee_armtz)

message(STATUS "TA configuration complete:")
message(STATUS "  UUID: ${TA_UUID}")
message(STATUS "  Sources: ${TA_SRCS}")
message(STATUS "  Libcxx: ${LIBCXX_STANDALONE}")
