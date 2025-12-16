# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= 
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
LD      = $(CROSS_COMPILE)ld
AR      = $(CROSS_COMPILE)ar
NM      = $(CROSS_COMPILE)nm
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
READELF = $(CROSS_COMPILE)readelf

BINARY = minimal_evm_host

TEEC_EXPORT ?= /usr

CXXFLAGS += -Wall -I../ta/include -I$(TEEC_EXPORT)/include -g -std=c++17
LDADD += -lteec -L$(TEEC_EXPORT)/lib

.PHONY: all clean

all: $(BINARY)

$(BINARY): main.o
	$(CXX) -o $@ $< $(LDADD)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(BINARY) main.o
