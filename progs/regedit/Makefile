TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld
SIZE ?= llvm-size

ROOT = ../..
NATIVE_DIR = $(ROOT)/libc/native

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fPIE -fno-stack-protector \
	-nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra -MMD -MP \
	-Iinclude -Itui/include -I$(NATIVE_DIR)/include
LDFLAGS ?= -m elf_x86_64 -pie --oformat=elf64-x86-64 -nostdlib \
	-dynamic-linker /lib/drld --export-dynamic

NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
NATIVE_SO = $(NATIVE_DIR)/lib/native.so

REGEDIT_BIN = regedit
CORE_SRC = src/core/path.c src/core/type.c src/core/hive.c \
	src/core/key.c src/core/value.c
TUI_SRC = tui/src/main.c tui/src/screen.c tui/src/state.c \
	tui/src/view.c tui/src/keys.c tui/src/prompt.c
CORE_OBJ = $(CORE_SRC:%.c=build/%.o)
TUI_OBJ = $(TUI_SRC:%.c=build/%.o)
OBJ = $(CORE_OBJ) $(TUI_OBJ)
DEP = $(OBJ:.o=.d)

.PHONY: all clean FORCE

all: $(REGEDIT_BIN)

$(NATIVE_CRT0) $(NATIVE_SO): FORCE
	@echo "  MAKE    native libc"
	@$(MAKE) -C $(NATIVE_DIR)

$(REGEDIT_BIN): $(NATIVE_CRT0) $(OBJ) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(OBJ) $(NATIVE_SO) -o $@
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
