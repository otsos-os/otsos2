TARGET ?= x86_64-elf
CC ?= clang
AR ?= llvm-ar

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
CRYPTO_DIR = $(ROOT)/libc/LibCrypto
ARCHIVE_DIR = $(ROOT)/libc/LibArchive
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -Isrc -I$(NATIVE_DIR)/include -I$(CRYPTO_DIR)/include \
	-I$(ARCHIVE_DIR)/include

LIB = lib/libmtproto.a
SRC = src/mtp_log.c src/mtp_write.c src/mtp_read.c src/mtp_transport.c src/mtp_session.c src/mtp_handshake.c src/mtp_srp.c src/mtp_rpc.c src/mtp_api.c src/mtp_store.c src/mtp_client.c src/mtp_schema.c
OBJ = $(SRC:src/%.c=build/%.o)
HDR = include/mtproto.h src/mtp_internal.h src/mtp_schema.h

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
