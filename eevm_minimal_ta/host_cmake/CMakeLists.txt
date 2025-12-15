# Host CMakeLists.txt - Native Linux application
cmake_minimum_required(VERSION 3.13)

# ===================================================
# Host Application (runs on normal world)
# ===================================================
set(HOST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/minimal_evm_host.c
)

add_executable(minimal_evm_host ${HOST_SRCS})

# Host compile flags (native ARM64)
target_compile_options(minimal_evm_host PRIVATE
    -Wall
    -Wextra
    -O2
)

# Include TEEC headers
target_include_directories(minimal_evm_host PRIVATE
    ${TEEC_EXPORT}/include
)

# Link against TEEC library
target_link_libraries(minimal_evm_host
    ${TEEC_EXPORT}/lib/libteec.so
    pthread
)

# Install host application
install(TARGETS minimal_evm_host
        RUNTIME DESTINATION bin)

message(STATUS "Host configuration complete:")
message(STATUS "  Sources: ${HOST_SRCS}")
message(STATUS "  TEEC lib: ${TEEC_EXPORT}/lib/libteec.so")
