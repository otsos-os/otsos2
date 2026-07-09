TARGET ?= x86_64-elf
CC = clang
LD = ld.lld
SIZE = llvm-size

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc \
          -fno-builtin -O0 -Wall -Wextra

LDFLAGS ?= -m elf_x86_64 -T ../init/linker.ld --oformat=elf64-x86-64 -nostdlib

YES_BIN = yes
YES_OBJS = yes.o

FETCH_BIN = fetch
FETCH_OBJS = fetch.o

SHELL_BIN = sh
SHELL_OBJS = shell.o

OVEN_BIN = oven
OVEN_OBJS = oven.o

SHMTEST_BIN = shmtest
SHMTEST_OBJS = shmtest.o

POSIX_HELLO_BIN = posix_hello
POSIX_HELLO_OBJS = posix_hello.o

KBDTEST_BIN = kbdtest
KBDTEST_OBJS = kbdtest.o

MUSL_DIR = ../libc/musl
MUSL_LIBC = $(MUSL_DIR)/lib/libc.a
MUSL_CRT1 = $(MUSL_DIR)/lib/crt1.o
MUSL_TEST_BIN = musl_test
MUSL_TEST_OBJS = musl_test.o
LUA_BIN = lua/lua/lua
LUA_DIR = lua/lua
LIBC_ENABLED ?= true

ifeq ($(LIBC_ENABLED),true)
ALL_TARGETS = $(YES_BIN) $(FETCH_BIN) $(SHELL_BIN) $(OVEN_BIN) $(SHMTEST_BIN) $(POSIX_HELLO_BIN) $(KBDTEST_BIN) $(MUSL_TEST_BIN) $(LUA_BIN)
else
ALL_TARGETS = $(YES_BIN) $(FETCH_BIN) $(SHELL_BIN) $(OVEN_BIN) $(SHMTEST_BIN) $(POSIX_HELLO_BIN) $(KBDTEST_BIN)
endif

all: $(ALL_TARGETS)

$(YES_BIN): $(YES_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(YES_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(FETCH_BIN): $(FETCH_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(FETCH_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(SHELL_BIN): $(SHELL_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(SHELL_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(OVEN_BIN): $(OVEN_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(OVEN_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(SHMTEST_BIN): $(SHMTEST_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(SHMTEST_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(POSIX_HELLO_BIN): $(POSIX_HELLO_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(POSIX_HELLO_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(KBDTEST_BIN): $(KBDTEST_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(KBDTEST_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(MUSL_LIBC):
	@echo "  MAKE    musl libc"
	@$(MAKE) -C $(MUSL_DIR)

$(MUSL_CRT1): $(MUSL_LIBC)
	@true

$(MUSL_TEST_BIN): $(MUSL_CRT1) $(MUSL_TEST_OBJS) $(MUSL_LIBC)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(MUSL_CRT1) $(MUSL_TEST_OBJS) $(MUSL_LIBC) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

lua: $(LUA_BIN)

$(LUA_BIN): $(MUSL_CRT1) $(MUSL_LIBC)
	@echo "  MAKE    ports/lua"
	@$(MAKE) -C $(LUA_DIR)

%.o: %.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

musl_test.o: musl_test.c
	@echo "  CC      $< (musl)"
	@$(CC) $(TARGET_CFLAGS) -O2 -g -fno-pic -fno-stack-protector \
	    -I$(MUSL_DIR)/arch/x86_64 -I$(MUSL_DIR)/arch/generic \
	    -I$(MUSL_DIR)/obj/include -I$(MUSL_DIR)/include -c $< -o $@

clean:
	rm -f $(YES_OBJS) $(YES_BIN) $(FETCH_OBJS) $(FETCH_BIN) $(SHELL_OBJS) $(SHELL_BIN) $(OVEN_OBJS) $(OVEN_BIN) $(SHMTEST_OBJS) $(SHMTEST_BIN) $(POSIX_HELLO_OBJS) $(POSIX_HELLO_BIN) $(KBDTEST_OBJS) $(KBDTEST_BIN) $(MUSL_TEST_OBJS) $(MUSL_TEST_BIN)
	$(MAKE) -C $(LUA_DIR) clean

.PHONY: all clean lua
