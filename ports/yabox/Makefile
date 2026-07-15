TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
NATIVE_LIB = $(NATIVE_DIR)/libnativec.a
NATIVE_CRT0 = $(NATIVE_DIR)/build/crt0.o
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fno-pic -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-I$(NATIVE_DIR)/include -I.
LDFLAGS ?= -m elf_x86_64 -T $(ROOT)/init/linker.ld \
	--oformat=elf64-x86-64 -nostdlib

TOOLS = ls cat touch pwd whoami rm mkdir rmdir cp mv
COMMON = build/common.o
OBJS = $(TOOLS:%=build/%.o)
BINS = $(TOOLS:%=%)
OLD_BINS = $(TOOLS:%=yabox/%)

.PHONY: clean

all: $(NATIVE_LIB) $(BINS)

$(NATIVE_LIB):
	$(MAKE) -C $(NATIVE_DIR)

$(BINS): %: build/%.o $(COMMON) $(NATIVE_LIB)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $< $(COMMON) $(NATIVE_LIB) -o $@

build/%.o: %.c yabox.h
	@mkdir -p build
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

build/common.o: common.c yabox.h
	@mkdir -p build
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -f $(BINS) $(OLD_BINS)
