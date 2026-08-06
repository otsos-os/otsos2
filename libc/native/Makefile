TARGET ?= x86_64-elf
CC = clang
AR = llvm-ar
LD = ld.lld
SIZE = llvm-size

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fno-pic -fno-stack-protector \
          -nostdlib -nostdinc -fno-builtin -O2 -Wall -Wextra -Iinclude
ASFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fno-pic -nostdlib -Iinclude
LDFLAGS ?= -m elf_x86_64 -T ../../init/linker.ld --oformat=elf64-x86-64 \
           -nostdlib

LIB = lib/libc.a
SHARED = lib/native.so
CRT0 = lib/crt0.o

SRC = src/ctype.c src/data.c src/drm.c src/entity.c src/errno.c src/event.c \
      src/fs.c src/futex.c src/input.c src/ipc.c src/kofo.c src/malloc.c \
      src/mem.c src/net.c src/proc.c \
      src/reg.c src/start.c src/stdio.c src/stdlib.c src/string.c src/sys.c \
      src/syscall.c src/term.c src/trace.c
OBJ = $(SRC:src/%.c=obj/%.o)
PICOBJ = $(SRC:src/%.c=obj-pic/%.o)

all: $(CRT0) $(LIB) $(SHARED)

$(LIB): $(OBJ)
	@mkdir -p lib
	@echo "  AR      $@"
	@rm -f $@
	@$(AR) rcs $@ $(OBJ)

$(SHARED): $(PICOBJ)
	@mkdir -p lib
	@echo "  LLD     $@"
	@$(LD) -m elf_x86_64 --oformat=elf64-x86-64 -nostdlib \
	    -shared -soname native.so $(PICOBJ) -o $@
	@$(SIZE) $@
$(CRT0): crt/crt0.S
	@mkdir -p lib
	@echo "  CC      $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

obj/%.o: src/%.c
	@mkdir -p obj
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

obj-pic/%.o: src/%.c
	@mkdir -p obj-pic
	@echo "  CC      $< (pic)"
	@$(CC) $(CFLAGS) -fPIC -c $< -o $@
examples/hello: examples/hello.c $(CRT0) $(LIB)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o obj/hello.o
	@echo "  LLD     $@"
	@$(LD) $(LDFLAGS) $(CRT0) obj/hello.o $(LIB) -o $@
	@$(SIZE) $@

clean:
	rm -f $(OBJ) $(PICOBJ) obj/hello.o $(CRT0) $(LIB) $(SHARED) \
	    examples/hello

.PHONY: all clean
