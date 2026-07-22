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

CURSORD_BIN = demons/cursord
CURSORD_OBJS = demons/cursord.o

SHMTEST_BIN = shmtest
SHMTEST_OBJS = shmtest.o

UDP_ECHO_BIN = udp_echo
UDP_ECHO_OBJS = udp_echo.o
TCP_ECHO_BIN = tcp_echo
TCP_ECHO_OBJS = tcp_echo.o
SEND_BIN = send
SEND_OBJS = send.o
DHCPC_BIN = dhcpc
DHCPC_OBJS = dhcpc.o
DHCPD_BIN = demons/dhcpd
DHCPD_OBJS = demons/dhcpd.o

POSIX_HELLO_BIN = posix_hello
POSIX_HELLO_OBJS = posix_hello.o

YABOX_DIR = yabox
TOOLCHAIN_AS_DIR = toolchain/as
TOOLCHAIN_AS_BIN = $(TOOLCHAIN_AS_DIR)/as
TOOLCHAIN_LD_DIR = toolchain/ld
TOOLCHAIN_LD_BIN = $(TOOLCHAIN_LD_DIR)/ld

MUSL_DIR = ../libc/musl
MUSL_LIBC = $(MUSL_DIR)/lib/libc.a
MUSL_CRT1 = $(MUSL_DIR)/lib/crt1.o
NATIVE_DIR = ../libc/native
NATIVE_LIBC = $(NATIVE_DIR)/lib/libc.a
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
NATIVE_HDR = $(NATIVE_DIR)/include/native.h
NATIVE_DYN_LDFLAGS = -m elf_x86_64 -pie --oformat=elf64-x86-64 \
                     -nostdlib -dynamic-linker /lib/drld \
                     --export-dynamic
MUSL_TEST_BIN = musl_test
MUSL_TEST_OBJS = musl_test.o
LIBC_ENABLED ?= true

ifeq ($(LIBC_ENABLED),true)
ALL_TARGETS = $(YES_BIN) $(FETCH_BIN) $(SHELL_BIN) $(OVEN_BIN) $(CURSORD_BIN) $(SHMTEST_BIN) $(UDP_ECHO_BIN) $(TCP_ECHO_BIN) $(SEND_BIN) $(DHCPC_BIN) $(DHCPD_BIN) $(POSIX_HELLO_BIN) yabox $(TOOLCHAIN_AS_BIN) $(TOOLCHAIN_LD_BIN) $(MUSL_TEST_BIN)
else
ALL_TARGETS = $(YES_BIN) $(FETCH_BIN) $(SHELL_BIN) $(OVEN_BIN) $(CURSORD_BIN) $(SHMTEST_BIN) $(UDP_ECHO_BIN) $(TCP_ECHO_BIN) $(SEND_BIN) $(DHCPC_BIN) $(DHCPD_BIN) $(POSIX_HELLO_BIN) yabox $(TOOLCHAIN_AS_BIN) $(TOOLCHAIN_LD_BIN)
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

$(NATIVE_LIBC) $(NATIVE_SO) $(NATIVE_CRT0): FORCE
	@echo "  MAKE    native libc"
	@$(MAKE) -C $(NATIVE_DIR)

$(SHELL_BIN): $(NATIVE_CRT0) $(SHELL_OBJS) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(NATIVE_DYN_LDFLAGS) $(NATIVE_CRT0) $(SHELL_OBJS) \
	    $(NATIVE_SO) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(OVEN_BIN): $(OVEN_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(OVEN_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(CURSORD_BIN): $(NATIVE_CRT0) $(CURSORD_OBJS) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(NATIVE_DYN_LDFLAGS) $(NATIVE_CRT0) $(CURSORD_OBJS) \
	    $(NATIVE_SO) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(SHMTEST_BIN): $(SHMTEST_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(SHMTEST_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(UDP_ECHO_BIN): $(NATIVE_CRT0) $(UDP_ECHO_OBJS) $(NATIVE_LIBC)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(UDP_ECHO_OBJS) \
	    $(NATIVE_LIBC) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(TCP_ECHO_BIN): $(NATIVE_CRT0) $(TCP_ECHO_OBJS) $(NATIVE_LIBC)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(TCP_ECHO_OBJS) \
	    $(NATIVE_LIBC) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(SEND_BIN): $(NATIVE_CRT0) $(SEND_OBJS) $(NATIVE_LIBC)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(NATIVE_CRT0) $(SEND_OBJS) \
	    $(NATIVE_LIBC) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(DHCPC_BIN): $(NATIVE_CRT0) $(DHCPC_OBJS) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(NATIVE_DYN_LDFLAGS) $(NATIVE_CRT0) $(DHCPC_OBJS) \
	    $(NATIVE_SO) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(DHCPD_BIN): $(NATIVE_CRT0) $(DHCPD_OBJS) $(NATIVE_SO)
	@echo "  LLD      $@"
	@$(LD) $(NATIVE_DYN_LDFLAGS) $(NATIVE_CRT0) $(DHCPD_OBJS) \
	    $(NATIVE_SO) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

$(POSIX_HELLO_BIN): $(POSIX_HELLO_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(POSIX_HELLO_OBJS) -o $@
	@echo "  SIZE    $@"
	@$(SIZE) $@

yabox: $(NATIVE_CRT0) $(NATIVE_SO)
	@echo "  MAKE    progs/yabox"
	@$(MAKE) -C $(YABOX_DIR)

.PHONY: $(TOOLCHAIN_AS_BIN)
$(TOOLCHAIN_AS_BIN): $(NATIVE_CRT0) $(NATIVE_SO)
	@echo "  MAKE    progs/toolchain/as"
	@$(MAKE) -C $(TOOLCHAIN_AS_DIR)

.PHONY: $(TOOLCHAIN_LD_BIN)
$(TOOLCHAIN_LD_BIN): $(NATIVE_CRT0) $(NATIVE_SO)
	@echo "  MAKE    progs/toolchain/ld"
	@$(MAKE) -C $(TOOLCHAIN_LD_DIR)

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

%.o: %.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

shell.o: shell.c $(NATIVE_HDR)
	@echo "  CC      $< (native libc)"
	@$(CC) $(CFLAGS) -fPIE -I$(NATIVE_DIR)/include -c $< -o $@

udp_echo.o: udp_echo.c $(NATIVE_HDR)
	@echo "  CC      $< (native libc)"
	@$(CC) $(CFLAGS) -I$(NATIVE_DIR)/include -c $< -o $@

tcp_echo.o: tcp_echo.c $(NATIVE_HDR)
	@echo "  CC      $< (native libc)"
	@$(CC) $(CFLAGS) -I$(NATIVE_DIR)/include -c $< -o $@

send.o: send.c $(NATIVE_HDR)
	@echo "  CC      $< (native libc)"
	@$(CC) $(CFLAGS) -I$(NATIVE_DIR)/include -c $< -o $@

dhcpc.o: dhcpc.c $(NATIVE_HDR)
	@echo "  CC      $< (native libc)"
	@$(CC) $(CFLAGS) -fPIE -I$(NATIVE_DIR)/include -c $< -o $@

demons/dhcpd.o: demons/dhcpd.c $(NATIVE_HDR)
	@echo "  CC      $< (native libc)"
	@$(CC) $(CFLAGS) -fPIE -I$(NATIVE_DIR)/include -c $< -o $@

demons/cursord.o: demons/cursord.c $(NATIVE_HDR)
	@echo "  CC      $< (native libc)"
	@$(CC) $(CFLAGS) -fPIE -I$(NATIVE_DIR)/include -c $< -o $@

musl_test.o: musl_test.c
	@echo "  CC      $< (musl)"
	@$(CC) $(TARGET_CFLAGS) -O2 -g -fno-pic -fno-stack-protector \
	    -I$(MUSL_DIR)/arch/x86_64 -I$(MUSL_DIR)/arch/generic \
	    -I$(MUSL_DIR)/obj/include -I$(MUSL_DIR)/include -c $< -o $@

clean:
	rm -f $(YES_OBJS) $(YES_BIN) $(FETCH_OBJS) $(FETCH_BIN) $(SHELL_OBJS) $(SHELL_BIN) $(OVEN_OBJS) $(OVEN_BIN) $(CURSORD_OBJS) $(CURSORD_BIN) $(SHMTEST_OBJS) $(SHMTEST_BIN) $(UDP_ECHO_OBJS) $(UDP_ECHO_BIN) $(TCP_ECHO_OBJS) $(TCP_ECHO_BIN) $(SEND_OBJS) $(SEND_BIN) $(DHCPC_OBJS) $(DHCPC_BIN) $(DHCPD_OBJS) $(DHCPD_BIN) $(POSIX_HELLO_OBJS) $(POSIX_HELLO_BIN) $(MUSL_TEST_OBJS) $(MUSL_TEST_BIN)
	$(MAKE) -C $(YABOX_DIR) clean
	$(MAKE) -C $(TOOLCHAIN_AS_DIR) clean
	$(MAKE) -C $(TOOLCHAIN_LD_DIR) clean

.PHONY: all clean yabox FORCE
