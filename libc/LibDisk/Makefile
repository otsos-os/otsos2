TARGET ?= x86_64-elf
CC ?= clang
AR ?= llvm-ar

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
KERNEL_INC = $(ROOT)/src
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -Isrc -I$(NATIVE_DIR)/include -I$(KERNEL_INC)

LIB = lib/libdisk.a
SRC = src/dev.c src/enum.c src/gpt.c src/format.c src/file.c src/boot.c
OBJ = $(SRC:src/%.c=build/%.o)
HDR = include/disk.h src/disk_int.h $(KERNEL_INC)/kernel/api/disk_abi.h

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
