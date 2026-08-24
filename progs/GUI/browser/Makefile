TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld
SIZE ?= llvm-size

ROOT = ../../..
PROGS_ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
LIBHTML_DIR = $(ROOT)/libc/LibHtml
LIBG_DIR = $(ROOT)/libc/LibG
LIBSVG_DIR = $(ROOT)/libc/LibSVG
LIBCSS_DIR = $(ROOT)/libc/LibCSS
LIBIMAGE_DIR = $(ROOT)/libc/LibImage
LIBARCHIVE_DIR = $(ROOT)/libc/LibArchive
LIBSRAPI_DIR = $(ROOT)/libc/LibSRAPI
SPROT_DIR = $(PROGS_ROOT)/GUI/sprot
TOOLCHAIN_AS_DIR = $(PROGS_ROOT)/toolchain/as
LIBELF_DIR = $(ROOT)/libc/LibELF
LIBEMIT_DIR = $(ROOT)/libc/LibEmit

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include -I$(LIBHTML_DIR)/include \
	-I$(LIBG_DIR)/include -I$(LIBSVG_DIR)/include \
	-I$(LIBCSS_DIR)/include -I$(LIBSRAPI_DIR)/include \
	-I$(LIBIMAGE_DIR)/include \
	-I$(SPROT_DIR)/include

NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
LIBHTML = $(LIBHTML_DIR)/lib/libhtml.a
LIBG = $(LIBG_DIR)/lib/libg.a
LIBSVG = $(LIBSVG_DIR)/lib/libsvg.a
LIBCSS = $(LIBCSS_DIR)/lib/libcss.a
LIBIMAGE = $(LIBIMAGE_DIR)/lib/libimage.a
LIBARCHIVE = $(LIBARCHIVE_DIR)/lib/libarchive.a
LIBSPROT = $(SPROT_DIR)/lib/libsprot.a
LIBSRAPI = $(LIBSRAPI_DIR)/lib/libsrapi.a
LIBAS = $(TOOLCHAIN_AS_DIR)/lib/libas.a
LIBELF = $(LIBELF_DIR)/lib/libelf.a
LIBEMIT = $(LIBEMIT_DIR)/lib/libemit.a

NATIVE_DYN_LDFLAGS = -m elf_x86_64 -pie --oformat=elf64-x86-64 \
                     -nostdlib -dynamic-linker /lib/drld \
                     --export-dynamic

BIN = browser
SRC = src/main.c src/net.c src/ui.c src/loader.c src/svg.c src/image.c
OBJ = $(SRC:src/%.c=build/%.o)

.PHONY: all clean

all: $(BIN)

$(BIN): $(NATIVE_CRT0) $(OBJ) $(LIBHTML) $(LIBCSS) $(LIBG) $(LIBSVG) \
    $(LIBIMAGE) $(LIBARCHIVE) $(LIBSPROT) $(LIBSRAPI) $(LIBAS) $(LIBELF) \
    $(LIBEMIT) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(NATIVE_DYN_LDFLAGS) $(NATIVE_CRT0) $(OBJ) \
	    $(LIBHTML) $(LIBCSS) $(LIBG) $(LIBIMAGE) $(LIBARCHIVE) \
	    $(LIBSVG) $(LIBSPROT) \
	    $(LIBSRAPI) \
	    $(LIBAS) $(LIBELF) $(LIBEMIT) \
	    $(NATIVE_SO) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

build/%.o: src/%.c include/browser.h
	@mkdir -p $(dir $@)
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -f $(BIN)
