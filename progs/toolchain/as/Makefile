TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld
SIZE ?= llvm-size
AR ?= llvm-ar

ROOT = ../../..
NATIVE_DIR = $(ROOT)/libc/native
LIBELF_DIR = $(ROOT)/libc/LibELF
LIBEMIT_DIR = $(ROOT)/libc/LibEmit
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
LIBELF = $(LIBELF_DIR)/lib/libelf.a
LIBEMIT = $(LIBEMIT_DIR)/lib/libemit.a
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include -I$(LIBELF_DIR)/include \
	-I$(LIBEMIT_DIR)/include
LDFLAGS ?= -m elf_x86_64 -pie --oformat=elf64-x86-64 \
	-nostdlib -dynamic-linker /lib/drld --export-dynamic

BIN = as
LIB = lib/libas.a
OBJ = as.o
LIBOBJ = build/as_lib.o

.PHONY: all clean FORCE

all: $(BIN) $(LIB)

$(NATIVE_SO) $(NATIVE_CRT0): FORCE
	@echo "  MAKE    native libc"
	@$(MAKE) -C $(NATIVE_DIR)

$(LIBELF): FORCE
	@echo "  MAKE    LibELF"
	@$(MAKE) -C $(LIBELF_DIR)

$(LIBEMIT): FORCE
	@echo "  MAKE    LibEmit"
	@$(MAKE) -C $(LIBEMIT_DIR)

$(BIN): $(NATIVE_CRT0) $(OBJ) $(LIBELF) $(LIBEMIT) $(NATIVE_SO)
	@echo "  LLD     $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(OBJ) $(LIBELF) $(LIBEMIT) \
	    $(NATIVE_SO) -o $@
	@$(SIZE) $@

$(LIB): $(LIBOBJ)
	@mkdir -p lib
	@echo "  AR      $@"
	@rm -f $@
	@$(AR) rcs $@ $(LIBOBJ)

as.o: as.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

build/as_lib.o: as.c include/libas.h
	@mkdir -p build
	@echo "  CC      $< (libas)"
	@$(CC) $(CFLAGS) -DLIBAS_NO_MAIN -c $< -o $@

clean:
	rm -rf build lib
	rm -f $(OBJ) $(BIN)
