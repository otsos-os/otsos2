TARGET ?= x86_64-elf
CC ?= clang
AR ?= llvm-ar

ROOT = ../../..
NATIVE_DIR = $(ROOT)/libc/native
LIBSRAPI_DIR = $(ROOT)/libc/LibSRAPI
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include -I$(LIBSRAPI_DIR)/include

LIB = lib/libsprot.a
SRC = src/client.c src/msg.c src/srapi_bridge.c
OBJ = $(SRC:src/%.c=build/%.o)
HDR = include/sprot/client.h include/sprot/sprot.h \
	include/sprot/srapi_bridge.h

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJ)
	@mkdir -p lib
	@echo "  AR       $@"
	@rm -f $@
	@$(AR) rcs $@ $(OBJ)

build/%.o: src/%.c $(HDR)
	@mkdir -p $(dir $@)
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -f $(LIB)
