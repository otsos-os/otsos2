TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
LIBARCHIVE_DIR = $(ROOT)/libc/LibArchive
NATIVE_LIB = $(NATIVE_DIR)/lib/libc.a
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
LIBARCHIVE = $(LIBARCHIVE_DIR)/lib/libarchive.a
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-I$(NATIVE_DIR)/include -I$(LIBARCHIVE_DIR)/include -I.
LDFLAGS ?= -m elf_x86_64 -pie --oformat=elf64-x86-64 \
	-nostdlib -dynamic-linker /lib/drld --export-dynamic

TOOLS = ls cat touch pwd whoami rm mkdir rmdir cp mv mmod profile lse ecat
ARCHIVE_TOOLS = unzip
COMMON = build/common.o
OBJS = $(TOOLS:%=build/%.o) $(ARCHIVE_TOOLS:%=build/%.o)
BINS = $(TOOLS:%=%) $(ARCHIVE_TOOLS:%=%)
OLD_BINS = $(BINS:%=yabox/%)

.PHONY: all clean

all: $(NATIVE_SO) $(NATIVE_CRT0) $(BINS)

$(NATIVE_LIB) $(NATIVE_SO) $(NATIVE_CRT0):
	$(MAKE) -C $(NATIVE_DIR)

$(LIBARCHIVE):
	$(MAKE) -C $(LIBARCHIVE_DIR)

$(TOOLS): %: build/%.o $(COMMON) $(NATIVE_SO) $(NATIVE_CRT0)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $< $(COMMON) $(NATIVE_SO) -o $@

$(ARCHIVE_TOOLS): %: build/%.o $(COMMON) $(LIBARCHIVE) $(NATIVE_SO) $(NATIVE_CRT0)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $< $(COMMON) $(LIBARCHIVE) \
	    $(NATIVE_SO) -o $@

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
	$(MAKE) -C $(LIBARCHIVE_DIR) clean
