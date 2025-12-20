# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= aarch64-none-linux-gnu-
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
LD      = $(CROSS_COMPILE)ld

BINARY = leveldb_host

# Đường dẫn đến OP-TEE Client Export (chứa libteec.so và các header)
TEEC_EXPORT ?= /usr

# CFLAGS cho C, CXXFLAGS cho C++
CFLAGS += -Wall -I../ta -I../include -I$(TEEC_EXPORT)/include
CXXFLAGS += -Wall -I../ta -I../include -I$(TEEC_EXPORT)/include -std=c++17 -g -pthread
LDADD += -L$(TEEC_EXPORT)/lib -lteec -pthread

.PHONY: all clean

all: $(BINARY)

# Khi link các file object C++ thành file thực thi, nên dùng $(CXX)
$(BINARY): leveldb_test_main.o
	$(CXX) -o $@ $< $(LDADD)

# Đổi từ main.c sang main.cpp
leveldb_test_main.o: leveldb_test_main.cpp ring_buffer_consumer.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(BINARY) leveldb_test_main.o