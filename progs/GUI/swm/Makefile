TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld
SIZE ?= llvm-size

ROOT = ../../..
NATIVE_DIR = $(ROOT)/libc/native
LIBSRAPI_DIR = $(ROOT)/libc/LibSRAPI
LIBG_DIR = $(ROOT)/libc/LibG
LIBAS_DIR = $(ROOT)/progs/toolchain/as
LIBELF_DIR = $(ROOT)/libc/LibELF
LIBEMIT_DIR = $(ROOT)/libc/LibEmit
SPROT_DIR = ../sprot

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra -MMD -MP \
	-Iinclude -I$(SPROT_DIR)/include -I$(NATIVE_DIR)/include \
	-I$(LIBSRAPI_DIR)/include -I$(LIBG_DIR)/include
LDFLAGS ?= -m elf_x86_64 -pie --oformat=elf64-x86-64 -nostdlib \
	-dynamic-linker /lib/drld --export-dynamic

NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
LIBSRAPI = $(LIBSRAPI_DIR)/lib/libsrapi.a
LIBG = $(LIBG_DIR)/lib/libg.a
LIBAS = $(LIBAS_DIR)/lib/libas.a
LIBELF = $(LIBELF_DIR)/lib/libelf.a
LIBEMIT = $(LIBEMIT_DIR)/lib/libemit.a
LIBSPROT = $(SPROT_DIR)/lib/libsprot.a

SWM_BIN = swm
SWM_SRC = src/main.c src/backend/srapi_drm.c src/buffer/buffer.c \
	src/interaction/interaction.c src/protocol/protocol.c \
	src/render/render.c src/surface/surface.c
SWM_OBJ = $(SWM_SRC:src/%.c=build/%.o)

DE_BIN = de
DE_SRC = src/de/de.c
DE_OBJ = $(DE_SRC:src/%.c=build/%.o)
DEP = $(SWM_OBJ:.o=.d) $(DE_OBJ:.o=.d)

.PHONY: all clean FORCE

all: $(SWM_BIN) $(DE_BIN)

$(NATIVE_CRT0) $(NATIVE_SO): FORCE
	@echo "  MAKE    native libc"
	@$(MAKE) -C $(NATIVE_DIR)

$(LIBSRAPI): FORCE
	@echo "  MAKE    LibSRAPI"
	@$(MAKE) -C $(LIBSRAPI_DIR)

$(LIBG): $(LIBSRAPI) FORCE
	@echo "  MAKE    LibG"
	@$(MAKE) -C $(LIBG_DIR)

$(LIBAS): FORCE
	@echo "  MAKE    progs/toolchain/as"
	@$(MAKE) -C $(LIBAS_DIR)

$(LIBELF): FORCE
	@echo "  MAKE    LibELF"
	@$(MAKE) -C $(LIBELF_DIR)

$(LIBEMIT): FORCE
	@echo "  MAKE    LibEmit"
	@$(MAKE) -C $(LIBEMIT_DIR)

$(LIBSPROT): FORCE
	@echo "  MAKE    Sprot"
	@$(MAKE) -C $(SPROT_DIR)

$(SWM_BIN): $(NATIVE_CRT0) $(SWM_OBJ) $(LIBSPROT) $(LIBSRAPI) \
	$(LIBAS) $(LIBELF) $(LIBEMIT) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(SWM_OBJ) $(LIBSPROT) \
	    $(LIBSRAPI) $(LIBAS) $(LIBELF) $(LIBEMIT) $(NATIVE_SO) -o $@
	@echo "  SIZE     $@"
	@$(SIZE) $@

$(DE_BIN): $(NATIVE_CRT0) $(DE_OBJ) $(LIBG) $(LIBSPROT) $(LIBSRAPI) \
	$(LIBAS) $(LIBELF) $(LIBEMIT) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(DE_OBJ) $(LIBG) $(LIBSPROT) \
	    $(LIBSRAPI) $(LIBAS) $(LIBELF) $(LIBEMIT) $(NATIVE_SO) -o $@
	@echo "  SIZE     $@"
	@$(SIZE) $@

build/%.o: src/%.c Makefile
	@mkdir -p $(dir $@)
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP)

clean:
	rm -rf build
	rm -f $(SWM_BIN) $(DE_BIN)
	$(MAKE) -C $(SPROT_DIR) clean

FORCE:
