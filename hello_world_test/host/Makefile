CROSS_COMPILE ?= aarch64-linux-gnu-
CC      ?= $(CROSS_COMPILE)gcc
LD      ?= $(CROSS_COMPILE)ld
AR      ?= $(CROSS_COMPILE)ar
NM      ?= $(CROSS_COMPILE)nm
OBJCOPY ?= $(CROSS_COMPILE)objcopy
OBJDUMP ?= $(CROSS_COMPILE)objdump
READELF ?= $(CROSS_COMPILE)readelf

OBJS = main.o

# TEEC_EXPORT should point to optee_client/out/export/usr
TEEC_EXPORT ?= /home/abc/optee_client/out/export/usr

CFLAGS += -Wall -I../ta/include -I$(TEEC_EXPORT)/include -I./include
#Add/link other required libraries here
# Use static linking if STATIC_BUILD is set
ifeq ($(STATIC_BUILD),1)
LDADD += -lteec -L$(TEEC_EXPORT)/lib -static
else
LDADD += -lteec -L$(TEEC_EXPORT)/lib
endif

BINARY = optee_example_hello_world

.PHONY: all
all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $< $(LDADD)

.PHONY: clean
clean:
	rm -f $(OBJS) $(BINARY)

%.o: %.c
	@echo "CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@
