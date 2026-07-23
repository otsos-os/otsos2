TARGET ?= x86_64-elf
CC ?= clang
AR ?= llvm-ar

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include

LIB = lib/libsrapi.a
SRC = src/backend.c src/command.c src/core.c src/raster.c src/vm.c
OBJ = $(SRC:src/%.c=build/%.o)
HDR = include/srapi.h src/srapi_private.h

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJ)
	@mkdir -p lib
	@echo "  AR       $@"
	@rm -f $@
	@$(AR) rcs $@ $(OBJ)

build/%.o: src/%.c $(HDR)
	@mkdir -p build
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -f $(LIB)
