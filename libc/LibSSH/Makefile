TARGET ?= x86_64-elf
CC ?= clang
AR ?= llvm-ar

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
LIBCRYPTO_DIR = $(ROOT)/libc/LibCrypto
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include -I$(LIBCRYPTO_DIR)/include

LIB = lib/libssh.a
SRC = src/auth.c src/buffer.c src/channel.c src/client.c src/ident.c \
	src/kexinit.c \
	src/knownhost.c src/log.c src/namelist.c src/kex.c src/packet.c \
	src/transport.c
OBJ = $(SRC:src/%.c=build/%.o)
HDR = include/libssh.h src/private.h

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
