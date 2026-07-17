TARGET ?= x86_64-elf
CC = clang
LD = ld.lld
SIZE = llvm-size
TARGET_CFLAGS ?= --target=$(TARGET)
ROOT = ../../..
NATIVE_DIR = $(ROOT)/libc/native
NATIVE_LIBC = $(NATIVE_DIR)/lib/libc.a
NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fno-pic \
          -fno-stack-protector -nostdlib -nostdinc -fno-builtin \
          -O2 -Wall -Wextra -I. -I$(NATIVE_DIR)/include
LDFLAGS ?= -m elf_x86_64 -T linker.ld --oformat=elf64-x86-64 \
           -nostdlib

BIN = drld
OBJ = dld.o

all: $(BIN)

$(NATIVE_LIBC) $(NATIVE_CRT0): FORCE
	@echo "  MAKE    native libc"
	@$(MAKE) -C $(NATIVE_DIR)

$(BIN): $(NATIVE_CRT0) $(OBJ) $(NATIVE_LIBC) linker.ld
	@echo "  LLD     $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(OBJ) $(NATIVE_LIBC) -o $@
	@$(SIZE) $@

dld.o: dld.c elf.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean FORCE
