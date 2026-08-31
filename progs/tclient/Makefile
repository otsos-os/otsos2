TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld
SIZE ?= llvm-size

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
LIBG_DIR = $(ROOT)/libc/LibG
LIBSRAPI_DIR = $(ROOT)/libc/LibSRAPI
LIBSVG_DIR = $(ROOT)/libc/LibSVG
LIBIMAGE_DIR = $(ROOT)/libc/LibImage
LIBMTPROTO_DIR = $(ROOT)/libc/LibMTProto
LIBCRYPTO_DIR = $(ROOT)/libc/LibCrypto
LIBARCHIVE_DIR = $(ROOT)/libc/LibArchive
SPROT_DIR = $(ROOT)/progs/GUI/sprot

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include -I$(LIBG_DIR)/include \
	-I$(LIBSRAPI_DIR)/include -I$(LIBSVG_DIR)/include -I$(LIBIMAGE_DIR)/include \
    -I$(LIBMTPROTO_DIR)/include \
	-I$(SPROT_DIR)/include

NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
LIBG = $(LIBG_DIR)/lib/libg.a
LIBSRAPI = $(LIBSRAPI_DIR)/lib/libsrapi.a
LIBSVG = $(LIBSVG_DIR)/lib/libsvg.a
LIBIMAGE = $(LIBIMAGE_DIR)/lib/libimage.a
LIBMTPROTO = $(LIBMTPROTO_DIR)/lib/libmtproto.a
LIBCRYPTO = $(LIBCRYPTO_DIR)/lib/libcrypto.a
LIBARCHIVE = $(LIBARCHIVE_DIR)/lib/libarchive.a
LIBSPROT = $(SPROT_DIR)/lib/libsprot.a
LDFLAGS = -m elf_x86_64 -pie --oformat=elf64-x86-64 -nostdlib \
	-dynamic-linker /lib/drld --export-dynamic

BIN = tclient
SRC = src/main.c src/trace.c
OBJ = $(SRC:src/%.c=build/%.o)

.PHONY: all clean

all: $(BIN)

$(BIN): $(NATIVE_CRT0) $(OBJ) $(LIBMTPROTO) $(LIBCRYPTO) $(LIBARCHIVE) \
    $(LIBG) $(LIBSVG) $(LIBIMAGE) $(LIBSPROT) $(LIBSRAPI) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(OBJ) $(LIBMTPROTO) $(LIBCRYPTO) \
	    $(LIBARCHIVE) $(LIBG) $(LIBSVG) $(LIBIMAGE) $(LIBSPROT) $(LIBSRAPI) $(NATIVE_SO) -o $@
	@echo "  SIZE     $@"
	@$(SIZE) $@

build/%.o: src/%.c include/tclient.h
	@mkdir -p $(dir $@)
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -f $(BIN)
