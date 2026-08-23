TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld
SIZE ?= llvm-size

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native
LIBSRAPI_DIR = $(ROOT)/libc/LibSRAPI
LIBG_DIR = $(ROOT)/libc/LibG
LIBAS_DIR = $(ROOT)/progs/toolchain/as
LIBELF_DIR = $(ROOT)/libc/LibELF
LIBEMIT_DIR = $(ROOT)/libc/LibEmit
SPROT_DIR = $(ROOT)/progs/GUI/sprot

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra -MMD -MP \
	-Iinclude -Itui/include -Igui/include -I$(NATIVE_DIR)/include \
	-I$(LIBSRAPI_DIR)/include -I$(LIBG_DIR)/include \
	-I$(SPROT_DIR)/include
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

REGEDIT_BIN = regedit
CORE_SRC = src/main.c src/core/path.c src/core/type.c src/core/hive.c \
	src/core/key.c src/core/value.c
TUI_SRC = tui/src/main.c tui/src/screen.c tui/src/state.c \
	tui/src/view.c tui/src/keys.c tui/src/prompt.c
GUI_SRC = gui/src/main.c gui/src/layout.c gui/src/state.c \
	gui/src/window.c gui/src/draw.c gui/src/input.c \
	gui/src/action.c gui/src/dialog.c
CORE_OBJ = $(CORE_SRC:%.c=build/%.o)
TUI_OBJ = $(TUI_SRC:%.c=build/%.o)
GUI_OBJ = $(GUI_SRC:%.c=build/%.o)
OBJ = $(CORE_OBJ) $(TUI_OBJ) $(GUI_OBJ)
DEP = $(OBJ:.o=.d)

# LibG needs LibSRAPI, and LibSRAPI's DRM path pulls symbols out of LibAS,
# LibELF and LibEmit, so all five ship in the link line even though regedit
# only calls into LibG and Sprot directly.
LIBS = $(LIBG) $(LIBSPROT) $(LIBSRAPI) $(LIBAS) $(LIBELF) $(LIBEMIT)

.PHONY: all clean FORCE

all: $(REGEDIT_BIN)

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

$(REGEDIT_BIN): $(NATIVE_CRT0) $(OBJ) $(LIBS) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(OBJ) $(LIBS) $(NATIVE_SO) -o $@
	@echo "  SIZE     $@"
	@$(SIZE) $@

build/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	@echo "  CC       $<"
	@$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP)

clean:
	rm -rf build
	rm -f $(REGEDIT_BIN)

FORCE:
