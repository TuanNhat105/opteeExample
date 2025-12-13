cmake_minimum_required(VERSION 3.13)
project(hello_cpp C CXX)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wbad-function-cast -Wcast-align \
		  -Werror-implicit-function-declaration -Wextra \
		  -Wfloat-equal -Wformat-nonliteral -Wformat-security \
		  -Wformat=2 -Winit-self -Wmissing-declarations \
		  -Wmissing-format-attribute -Wmissing-include-dirs \
		  -Wmissing-noreturn -Wmissing-prototypes -Wnested-externs \
		  -Wpointer-arith -Wshadow -Wstrict-prototypes \
		  -Wswitch-default -Wunsafe-loop-optimizations \
		  -Wwrite-strings -Werror")

find_package(Threads REQUIRED)
if(NOT THREADS_FOUND)
	message(FATAL_ERROR "Threads not found")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(TEEC REQUIRED tee-client-api)
include_directories(${TEEC_INCLUDE_DIRS})

# Build host application
add_executable(hello_cpp_host host/main.c)
target_include_directories(hello_cpp_host PRIVATE ta/include)
target_link_libraries(hello_cpp_host ${TEEC_LIBRARIES} pthread)

install(TARGETS hello_cpp_host DESTINATION bin)
