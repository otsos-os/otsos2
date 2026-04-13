TARGET ?= x86_64-elf
CC ?= clang
LD ?= ld.lld
SIZE ?= llvm-size

# Flags for freestanding userspace code (no libc)
TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc \
          -fno-builtin -O2 -Wall -Wextra

LDFLAGS ?= -m elf_x86_64 -T linker.ld --oformat=elf64-x86-64 -nostdlib

TARGET = init
HELLO_BIN = hello
OBJS = init.o
HELLO_OBJS = hello.o

all: $(TARGET) $(HELLO_BIN)

$(TARGET): $(OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(HELLO_BIN): $(HELLO_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(HELLO_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

init.o: init.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

hello.o: hello.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(HELLO_OBJS) $(TARGET) $(HELLO_BIN)

.PHONY: all clean
