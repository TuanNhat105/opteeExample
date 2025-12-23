# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= aarch64-none-linux-gnu-
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
LD      = $(CROSS_COMPILE)ld

# BINARY = leveldb_host
TEST_BINARY = test_simple_shm

# Đường dẫn đến OP-TEE Client Export (chứa libteec.so và các header)
TEEC_EXPORT ?= /usr

# CFLAGS cho C, CXXFLAGS cho C++
CFLAGS += -Wall -I../ta -I../include -I$(TEEC_EXPORT)/include
CXXFLAGS += -Wall -I../ta -I../include -I$(TEEC_EXPORT)/include -std=c++17 -g -pthread
LDADD += -L$(TEEC_EXPORT)/lib -lteec -pthread

.PHONY: all clean

all: $(TEST_BINARY)

# Khi link các file object C++ thành file thực thi, nên dùng $(CXX)
# $(BINARY): main_ringbuffer.o
# 	$(CXX) -o $@ $< $(LDADD)

# main_ringbuffer.o: main_ringbuffer.cpp
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# Simple test binary (NO LevelDB)
$(TEST_BINARY): test_simple_shm.o
	$(CXX) -o $@ $< $(LDADD)

test_simple_shm.o: test_simple_shm.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(BINARY) $(TEST_BINARY) *.o