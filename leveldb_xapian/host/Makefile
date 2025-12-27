# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= 
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
LD      = $(CROSS_COMPILE)ld

# BINARY = leveldb_host
LEVELDB_TEST_BINARY = test_leveldb_ram

# Đường dẫn đến OP-TEE Client Export (chứa libteec.so và các header)
TEEC_EXPORT ?= /usr

# CFLAGS cho C, CXXFLAGS cho C++
CFLAGS += -Wall -I../ta -I../include -I$(TEEC_EXPORT)/include
CXXFLAGS += -Wall -I../ta -I../include -I$(TEEC_EXPORT)/include -std=c++17 -g -pthread
LDADD += -L$(TEEC_EXPORT)/lib -lteec -pthread

.PHONY: all clean

all: $(LEVELDB_TEST_BINARY)

# LevelDB test binary
$(LEVELDB_TEST_BINARY): test_leveldb_ram.o
	$(CXX) -o $@ $< $(LDADD)

test_leveldb_ram.o: test_leveldb_ram.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(LEVELDB_TEST_BINARY) *.o