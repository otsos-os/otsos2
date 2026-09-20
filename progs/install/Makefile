TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld
SIZE ?= llvm-size

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
LIBTUI_DIR = $(ROOT)/libc/LibTui
LIBDISK_DIR = $(ROOT)/libc/LibDisk

KERNEL_INC = $(ROOT)/src

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra \
	-Iinclude -I$(NATIVE_DIR)/include -I$(LIBTUI_DIR)/include \
	-I$(LIBDISK_DIR)/include -I$(KERNEL_INC)
LDFLAGS ?= -m elf_x86_64 -pie --oformat=elf64-x86-64 -nostdlib \
	-dynamic-linker /lib/drld --export-dynamic

NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
LIBTUI = $(LIBTUI_DIR)/lib/libtui.a
LIBDISK = $(LIBDISK_DIR)/lib/libdisk.a

INSTALL_BIN = install

SRC = src/main.c src/error.c src/geom.c src/walk.c src/plan.c src/scan.c src/layout.c \
	src/format.c src/copy.c src/boot.c src/verify.c src/firmware.c \
	src/ui.c src/ui_disk.c src/ui_layout.c src/ui_boot.c \
	src/ui_confirm.c src/ui_run.c
OBJ = $(SRC:%.c=build/%.o)
HDR = include/inst.h include/plan.h include/stage.h include/walk.h \
	include/ui.h

LIBS = $(LIBTUI) $(LIBDISK)

.PHONY: all clean FORCE

all: $(INSTALL_BIN)

$(NATIVE_CRT0) $(NATIVE_SO): FORCE
	@echo "  MAKE    native libc"
	@$(MAKE) -C $(NATIVE_DIR)

$(LIBTUI): FORCE
	@echo "  MAKE    LibTui"
	@$(MAKE) -C $(LIBTUI_DIR)

$(LIBDISK): FORCE
	@echo "  MAKE    LibDisk"
	@$(MAKE) -C $(LIBDISK_DIR)

$(INSTALL_BIN): $(NATIVE_CRT0) $(OBJ) $(LIBS) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(OBJ) $(LIBS) $(NATIVE_SO) -o $@
	@echo "  SIZE     $@"
	@$(SIZE) $@

build/%.o: %.c $(HDR) Makefile
	@mkdir -p $(dir $@)
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -f $(INSTALL_BIN)

FORCE:
