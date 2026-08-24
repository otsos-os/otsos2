TARGET ?= x86_64-elf
CC ?= clang
AR ?= llvm-ar

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
LIBARCHIVE_DIR = $(ROOT)/libc/LibArchive
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include -I$(LIBARCHIVE_DIR)/include

LIB = lib/libimage.a
SRC = src/png.c
OBJ = $(SRC:src/%.c=build/%.o)
HDR = include/libimage.h $(LIBARCHIVE_DIR)/include/libarchive.h

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
