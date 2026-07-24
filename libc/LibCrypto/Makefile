TARGET ?= x86_64-elf
CC ?= clang
AR ?= llvm-ar

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include

LIB = lib/libcrypto.a
SRC = src/chacha20.c src/chacha20_poly1305.c src/curve25519.c \
	src/hmac_sha256.c src/poly1305.c src/random.c src/sha256.c \
	src/sha512.c src/ed25519.c src/util.c
OBJ = $(SRC:src/%.c=build/%.o)
HDR = include/libcrypto.h src/private.h

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
