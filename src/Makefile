TARGET ?= x86_64-elf
CC = clang
ZIG = zig
PYTHON ?= python3
LD = ld.lld
AS = nasm
QEMU = qemu-system-x86_64
XORRISO = xorriso
QEMU_GUI_OPTS ?= -display gtk
QEMU_NODISPLAY_OPTS ?= -display none

TARGET_CFLAGS ?= --target=$(TARGET)
CFLAGS ?= $(TARGET_CFLAGS) -ffreestanding -fno-pic -fstack-protector-strong -g -O2 -fno-builtin -mno-red-zone -mcmodel=kernel -mno-sse -mno-mmx -mno-sse2 -mno-avx -mno-80387 -I. -Ikernel -Ikernel/drivers/video
LDFLAGS ?= -m elf_x86_64 -T linker.ld -Map=../bin/kernel.map
NASM_FORMAT ?= elf64
ZIG_TARGET ?= x86_64-freestanding

.DEFAULT_GOAL := all

include kernel/kshell/kshell.mk
CFLAGS += $(KSHELL_DEFINES)

include kernel/drivers/disk/disk.mk
CFLAGS += $(DISK_DEFINES)

KERNEL_BIN = ../bin/kernel.bin
ISO_IMAGE = ../otsos.iso
MAKEISO = ../tools/makeiso.py
DISK_IMG = ../bin/disk.img
PORTS_DIR = ../ports
MUSL_DIR = ../libc/musl
NATIVE_DIR = ../libc/native
MUSL_LIBC = $(MUSL_DIR)/lib/libc.a
MUSL_CRT1 = $(MUSL_DIR)/lib/crt1.o
YES_BIN = $(PORTS_DIR)/yes
FETCH_BIN = $(PORTS_DIR)/fetch
SHELL_BIN = $(PORTS_DIR)/sh
OVEN_BIN = $(PORTS_DIR)/oven
CURSORD_BIN = $(PORTS_DIR)/demons/cursord
SHMTEST_BIN = $(PORTS_DIR)/shmtest
UDP_ECHO_BIN = $(PORTS_DIR)/udp_echo
POSIX_HELLO_BIN = $(PORTS_DIR)/posix_hello
KBDTEST_BIN = $(PORTS_DIR)/kbdtest
MUSL_TEST_BIN = $(PORTS_DIR)/musl_test
LUA_BIN = $(PORTS_DIR)/lua/lua/lua
CPYTHON_BIN = $(PORTS_DIR)/cpython/cpython/python
MUSL_LDSO = $(MUSL_DIR)/lib/libc.so
LDSO_BIN = $(PORTS_DIR)/ld-musl-x86_64.so.1
NATIVE_LIBC = $(NATIVE_DIR)/lib/libc.a
NATIVE_CRT0 = $(NATIVE_DIR)/lib/crt0.o
NATIVE_SO = $(NATIVE_DIR)/lib/native.so
DLD_BIN = ../libc/LibExec/dld/drld
LIBC_ENABLED := $(shell sh ../tools/toml_get.sh config.toml libc enabled true)
MULTIBOOT_MODULES := $(shell perl ../tools/modules.pl config.toml)
CPYTHON_ENABLED := $(if $(filter python,$(MULTIBOOT_MODULES)),true,false)
DRLD_ENABLED := $(if $(filter drld,$(MULTIBOOT_MODULES)),true,false)
NATIVE_SO_ENABLED := $(if $(filter native-so,$(MULTIBOOT_MODULES)),true,false)
ifeq ($(CPYTHON_ENABLED),false)
CPYTHON_BIN =
endif

BIOS_BUILD_DIR = ../bin/bootloader/bios
UEFI_BUILD_DIR = ../bin/bootloader/uefi
BIOS_STAGE2_AREA_SECTORS = 256
BIOS_STAGE1_MAX_SECTORS = 60
BIOS_LAYOUT_LBA = 257
BIOS_BOOTPACK_LBA = 258
BIOS_STAGE1_OBJ = $(BIOS_BUILD_DIR)/stage1.o
BIOS_STAGE1_ELF = $(BIOS_BUILD_DIR)/stage1.elf
BIOS_STAGE1_BIN = $(BIOS_BUILD_DIR)/stage1.bin
BIOS_STAGE2_ELF = $(BIOS_BUILD_DIR)/stage2.elf
BIOS_STAGE2_BIN = $(BIOS_BUILD_DIR)/stage2.bin
BIOS_BOOTPACK = $(BIOS_BUILD_DIR)/bootpack.tar
BIOS_LAYOUT_BIN = $(BIOS_BUILD_DIR)/layout.bin
BIOS_BOOT_IMAGE = $(BIOS_BUILD_DIR)/otsos-bios.img
BIOS_PAYLOAD_DIR = $(BIOS_BUILD_DIR)/payload
UEFI_BOOTX64 = $(UEFI_BUILD_DIR)/BOOTX64.EFI
UEFI_ESP_IMAGE = $(UEFI_BUILD_DIR)/otsos-uefi-esp.img
UEFI_PAYLOAD_DIR = $(UEFI_BUILD_DIR)/payload
UEFI_TRAMPOLINE_OBJ = $(UEFI_BUILD_DIR)/trampoline.obj
BIOS_OBJCOPY = llvm-objcopy
BIOS_CFLAGS = --target=i386-elf -m32 -ffreestanding -fno-pic \
	-fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables \
	-nostdinc -O2 -g -I.
UEFI_CFLAGS = --target=x86_64-windows -ffreestanding -fno-pic \
	-fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables \
	-mno-red-zone -nostdinc -O2 -g -I.
BIOS_STAGE2_OBJS = \
	$(BIOS_BUILD_DIR)/entry.o \
	$(BIOS_BUILD_DIR)/main.o \
	$(BIOS_BUILD_DIR)/disk.o \
	$(BIOS_BUILD_DIR)/console.o \
	$(BIOS_BUILD_DIR)/lib_string.o \
	$(BIOS_BUILD_DIR)/lib_bootpack.o \
	$(BIOS_BUILD_DIR)/lib_elf64.o \
	$(BIOS_BUILD_DIR)/lib_multiboot2.o
UEFI_LIB_OBJS = \
	$(UEFI_BUILD_DIR)/lib_string.obj \
	$(UEFI_BUILD_DIR)/lib_bootpack.obj \
	$(UEFI_BUILD_DIR)/lib_elf64.obj \
	$(UEFI_BUILD_DIR)/lib_multiboot2.obj

ifeq ($(LIBC_ENABLED),true)
ISO_MUSL_TEST_DEP = $(MUSL_TEST_BIN)
ISO_LUA_DEP = $(LUA_BIN)
ISO_LDSO_DEP = $(LDSO_BIN)
LIBC_DEPS = $(MUSL_LIBC) $(MUSL_CRT1)
else
ISO_MUSL_TEST_DEP =
ISO_LUA_DEP =
ISO_LDSO_DEP =
LIBC_DEPS =
endif
ifeq ($(CPYTHON_ENABLED),true)
ISO_CPYTHON_DEP = $(CPYTHON_BIN)
else
ISO_CPYTHON_DEP =
endif
ifeq ($(DRLD_ENABLED),true)
ISO_DRLD_DEP = $(DLD_BIN)
else
ISO_DRLD_DEP =
endif
ifeq ($(NATIVE_SO_ENABLED),true)
ISO_NATIVE_SO_DEP = $(NATIVE_SO)
else
ISO_NATIVE_SO_DEP =
endif

# object files
BOOT_O = ../bin/boot.o
KERNEL_O = ../bin/kernel.o
STRING_O = ../bin/string.o
ITOA_O = ../bin/itoa.o
HARDWARE_O = ../bin/hardware.o
UART_O = ../bin/uart.o
STDIO_O = ../bin/stdio.o
BOOTMEM_O = ../bin/bootmem.o
IDT_ASM_O = ../bin/idt_asm.o
IDT_C_O = ../bin/idt_c.o
PIC_O = ../bin/pic_c.o
HANDLERS_O = ../bin/handlers_c.o
PANIC_O = ../bin/panic.o
CONSOLE_O = ../bin/console_console.o
DISK_O = ../bin/disk.o
RAMDISK_O = ../bin/ramdisk.o
CHAINFS_O = ../bin/chainfs.o
TERMINAL_O = ../bin/console_terminal.o
PTY_O = ../bin/console_pty.o
PS2_O = ../bin/ps2.o
KEYBOARD_O = ../bin/keyboard.o
KEYMAP_O = ../bin/keymap.o
MOUSE_PS2_O = ../bin/mouse_ps2.o
FONT_O = ../bin/font.o
DRM_CORE_O = ../bin/drm_core.o
DRM_GEM_O = ../bin/drm_gem.o
DRM_OBJ_O = ../bin/drm_obj.o
DRM_AUTH_O = ../bin/drm_auth.o
DRM_INIT_O = ../bin/drm_init.o
DRM_FBDEV_O = ../bin/drm_fbdev.o
DRM_KMS_FB_O = ../bin/drm_kms_fb.o
DRM_KMS_PLANE_O = ../bin/drm_kms_plane.o
DRM_KMS_CRTC_O = ../bin/drm_kms_crtc.o
DRM_KMS_CONN_O = ../bin/drm_kms_conn.o
DRM_KMS_ATOMIC_O = ../bin/drm_kms_atomic.o
KMS_CONSOLE_O = ../bin/kms_console.o
DRM_RAPI_O = ../bin/drm_rapi.o
TIMER_O = ../bin/timer.o
EVENTTIMER_O = ../bin/eventtimer.o
PIT_O = ../bin/pit.o
APIC_TIMER_O = ../bin/apic_timer.o
LAPIC_O = ../bin/lapic.o
IOAPIC_O = ../bin/ioapic.o
STDLIB_O = ../bin/stdlib.o
KMEM_O = ../bin/kmem.o
UMA_O = ../bin/uma.o
PMAP_O = ../bin/pmap.o
VM_PAGE_O = ../bin/vm_page.o
VM_MAP_O = ../bin/vm_map.o
VM_OBJECT_O = ../bin/vm_object.o
VM_PAGER_O = ../bin/vm_pager.o
WRITE_O = ../bin/write.o
READ_O = ../bin/read.o
OPEN_O = ../bin/open.o
CLOSE_O = ../bin/close.o
LSEEK_O = ../bin/lseek.o
WAIT_O = ../bin/wait.o
MMAP_O = ../bin/mmap.o
SHM_O = ../bin/shm.o
API_NET_O = ../bin/api_net.o

PIPE_O = ../bin/pipe.o
CLONE_O = ../bin/clone.o
SYSCALL_O = ../bin/syscall.o
UNAME_O = ../bin/uname.o
MEMINFO_O = ../bin/meminfo.o
CPUINFO_O = ../bin/cpuinfo.o
GDT_O = ../bin/gdt.o
GDT_ASM_O = ../bin/gdt_asm.o
PROCESS_O = ../bin/process.o
COPY_O = ../bin/proc_copy.o
SPAWN_O = ../bin/spawn.o
SESSION_O = ../bin/session.o
THREAD_O = ../bin/thread.o
CHDIR_O = ../bin/chdir.o
GETCWD_O = ../bin/getcwd.o
LISTDIR_O = ../bin/listdir.o
FS_META_O = ../bin/fs_meta.o
PROCLIST_O = ../bin/proclist.o
ELF_O = ../bin/elf.o
USERSPACE_O = ../bin/userspace.o
USERSPACE_ASM_O = ../bin/userspace_asm.o
SYSCALL_ASM_O = ../bin/syscall_asm.o
USERADDR_O = ../bin/useraddr.o
SCHEDULER_O = ../bin/scheduler.o
ACPI_O = ../bin/acpi.o
ACPI_TABLES_O = ../bin/acpi_tables.o
SMP_O = ../bin/smp.o
SMP_ASM_O = ../bin/smp_asm.o
POWER_O = ../bin/power.o
POWER_CTRL_O = ../bin/power_controller.o
POWER_PBUTTON_O = ../bin/power_pbutton.o
PCI_CORE_O = ../bin/pci_core.o
PCI_SCAN_O = ../bin/pci_scan.o
PCI_CONFIG_O = ../bin/pci_config.o
PCI_DEVICE_O = ../bin/pci_device.o
PCI_BAR_O = ../bin/pci_bar.o
WATCHDOG_O = ../bin/watchdog.o
WATCHDOG_I6300ESB_O = ../bin/watchdog_i6300esb.o
WATCHDOG_ICH_TCO_O = ../bin/watchdog_ich_tco.o
TOML_O = ../bin/toml.o
KUSR_O = ../bin/kusr.o
KUSR_AUTH_O = ../bin/kusr_auth.o
CONFIG_O = ../bin/config.o
DRM_API_O = ../bin/drm_api.o
RESTRICT_O = ../bin/restrict.o
CRYPTO_UTIL_O = ../bin/crypto_util.o
CRYPTO_SHA256_O = ../bin/crypto_sha256.o
CRYPTO_HMAC_O = ../bin/crypto_hmac.o
CRYPTO_PBKDF2_O = ../bin/crypto_pbkdf2.o
CRYPTO_RNG_O = ../bin/crypto_rng.o
CRYPTO_CHACHA20_O = ../bin/crypto_chacha20.o

VIRTIO_HW_O = ../bin/virtio_hw.o
VIRTIO_QUEUE_O = ../bin/virtio_queue.o
VIRTIO_PCI_O = ../bin/virtio_pci.o
VIRTIO_GPU_CMDS_O = ../bin/virtio_gpu_cmds.o
VIRTIO_GPU_O = ../bin/virtio_gpu.o
VIRTIO_NET_O = ../bin/virtio_net.o

NETDEV_O = ../bin/netdev.o
NET_O = ../bin/net.o
UNIX_SOCK_O = ../bin/unix_sock.o
NET_ETHERNET_O = ../bin/net_ethernet.o
NET_ARP_O = ../bin/net_arp.o
NET_IPV4_O = ../bin/net_ipv4.o
NET_ICMP_O = ../bin/net_icmp.o
NET_UDP_O = ../bin/net_udp.o
NET_ENDPOINT_O = ../bin/net_endpoint.o
NET_LOOPBACK_O = ../bin/net_loopback.o

EVENT_O = ../bin/event.o
EVENT_READ_O = ../bin/event_read.o
EVENT_WRITE_O = ../bin/event_write.o
EVENT_TIMER_O = ../bin/event_timer.o
EVENT_PROC_O = ../bin/event_proc.o
EVENT_SIGNAL_O = ../bin/event_signal.o
EVENT_USER_O = ../bin/event_user.o
EVENT_KBD_O = ../bin/event_kbd.o
EVENT_MOUSE_O = ../bin/event_mouse.o

VFS_O = ../bin/vfs.o
DEVFS_O = ../bin/devfs.o
LINK_O = ../bin/link.o
POSIX_O = ../bin/posix.o
POSIX_IO_O = ../bin/posix_io.o
POSIX_PROC_O = ../bin/posix_proc.o
POSIX_FS_O = ../bin/posix_fs.o
POSIX_SOCKET_O = ../bin/posix_socket.o
POSIX_MEM_O = ../bin/posix_mem.o
POSIX_SOCKET_O = ../bin/posix_socket.o
POSIX_SIGNAL_O = ../bin/posix_signal.o
SIGNAL_O = ../bin/signal.o
PROC_PID_O = ../bin/proc_pid.o
THREAD_API_O = ../bin/thread_api.o
FUTEX_O = ../bin/futex.o
SYS_O = ../bin/sys.o
API_TIME_O = ../bin/api_time.o
TERM_POWER_O = ../bin/term_power.o
TERM_INFO_O = ../bin/term_info.o
INPUT_O = ../bin/input.o

TIME_O = ../bin/time.o
CLOCKSOURCE_O = ../bin/clocksource.o
ET_CLOCKSOURCE_O = ../bin/et_clocksource.o
CMOS_RTC_O = ../bin/cmos_rtc.o

OBJ = $(BOOT_O) $(KERNEL_O) $(STRING_O) $(ITOA_O) $(HARDWARE_O) $(UART_O) $(STDIO_O) $(BOOTMEM_O) $(IDT_ASM_O) $(IDT_C_O) $(PIC_O) \
      $(HANDLERS_O) $(PANIC_O) $(KMEM_O) $(UMA_O) $(PMAP_O) $(VM_PAGE_O) $(VM_MAP_O) $(VM_OBJECT_O)       $(VM_PAGER_O) $(DISK_OBJ) $(DISK_O) $(RAMDISK_O) $(CHAINFS_O) $(CONSOLE_O) $(TERMINAL_O) $(PTY_O) $(PS2_O) $(KEYBOARD_O) $(KEYMAP_O) $(MOUSE_PS2_O) $(FONT_O) $(DRM_CORE_O) $(DRM_GEM_O) $(DRM_OBJ_O) $(DRM_AUTH_O) $(DRM_INIT_O) $(DRM_FBDEV_O) $(DRM_KMS_FB_O) $(DRM_KMS_PLANE_O) $(DRM_KMS_CRTC_O) $(DRM_KMS_CONN_O) $(DRM_KMS_ATOMIC_O) $(KMS_CONSOLE_O)       $(DRM_RAPI_O) $(TIMER_O) $(EVENTTIMER_O) $(PIT_O) $(APIC_TIMER_O) $(LAPIC_O) $(IOAPIC_O) $(STDLIB_O) $(WRITE_O) $(READ_O) $(OPEN_O) $(CLOSE_O) $(LSEEK_O) $(WAIT_O) $(MMAP_O) $(SHM_O) $(API_NET_O) $(PIPE_O) $(CLONE_O) $(SYSCALL_O) \
      $(GDT_O) $(GDT_ASM_O) $(PROCESS_O) $(COPY_O) $(SPAWN_O) $(SESSION_O) $(THREAD_O) $(CHDIR_O) $(GETCWD_O) $(LISTDIR_O) $(FS_META_O) $(PROCLIST_O) $(ELF_O) $(USERSPACE_O) $(USERSPACE_ASM_O) $(SYSCALL_ASM_O) $(USERADDR_O) $(SCHEDULER_O) \
      $(UNAME_O) $(MEMINFO_O) $(CPUINFO_O) \
      $(ACPI_O) $(ACPI_TABLES_O) $(SMP_O) $(SMP_ASM_O) $(POWER_O) $(POWER_CTRL_O) $(POWER_PBUTTON_O) \
      $(PCI_CORE_O) $(PCI_SCAN_O) $(PCI_CONFIG_O) $(PCI_DEVICE_O) $(PCI_BAR_O) \
      $(WATCHDOG_O) $(WATCHDOG_I6300ESB_O) $(WATCHDOG_ICH_TCO_O) \
      $(KSHELL_OBJ) \
      $(TOML_O) $(KUSR_O) $(KUSR_AUTH_O) $(CONFIG_O) $(DRM_API_O) $(RESTRICT_O) \
      $(CRYPTO_UTIL_O) $(CRYPTO_SHA256_O) $(CRYPTO_HMAC_O) $(CRYPTO_PBKDF2_O) $(CRYPTO_RNG_O) $(CRYPTO_CHACHA20_O) \
       $(VIRTIO_HW_O) $(VIRTIO_QUEUE_O) $(VIRTIO_PCI_O) $(VIRTIO_GPU_CMDS_O) $(VIRTIO_GPU_O) $(VIRTIO_NET_O) \
       $(NETDEV_O) $(NET_O) $(NET_ETHERNET_O) $(NET_ARP_O) $(NET_IPV4_O) $(NET_ICMP_O) $(NET_UDP_O) $(NET_ENDPOINT_O) $(NET_LOOPBACK_O) $(UNIX_SOCK_O) \
      $(EVENT_O) $(EVENT_READ_O) $(EVENT_WRITE_O) $(EVENT_TIMER_O) \
      $(EVENT_PROC_O) $(EVENT_SIGNAL_O) $(EVENT_USER_O) \
      $(EVENT_KBD_O) $(EVENT_MOUSE_O) \
      $(VFS_O) $(DEVFS_O) $(LINK_O) \
      $(POSIX_O) $(POSIX_IO_O) $(POSIX_PROC_O) $(POSIX_FS_O) \
      $(POSIX_MEM_O) $(POSIX_SOCKET_O) $(POSIX_SIGNAL_O) $(SIGNAL_O) \
      $(PROC_PID_O) $(THREAD_API_O) $(FUTEX_O) $(SYS_O) \
      $(API_TIME_O) $(TERM_POWER_O) $(TERM_INFO_O) $(INPUT_O) \
      $(TIME_O) $(CLOCKSOURCE_O) $(ET_CLOCKSOURCE_O) $(CMOS_RTC_O)

.PHONY: all ask_version check_build_tools check_run_tools check_bios_run_tools

all: check_build_tools ask_version $(ISO_IMAGE)

check_build_tools:
	@set -e; \
	for tool in "$(CC)" "$(LD)" "$(AS)" "$(ZIG)" "$(PYTHON)" \
		"$(BIOS_OBJCOPY)" "$(XORRISO)" llvm-size perl tar stat \
		truncate dd mformat mmd mcopy; do \
		if ! command -v "$$tool" >/dev/null 2>&1; then \
			echo "  MISSING $$tool"; \
			echo "  install missing tools to build"; \
			exit 1; \
		fi; \
	done

check_run_tools: check_build_tools
	@set -e; \
	for tool in "$(QEMU)" "$(OVMF)"; do \
		case "$$tool" in \
			/*) \
				if [ ! -e "$$tool" ]; then \
					echo "  MISSING $$tool"; \
					echo "  install OVMF to run"; \
					exit 1; \
				fi ;; \
			*) \
				if ! command -v "$$tool" >/dev/null 2>&1; then \
					echo "  MISSING $$tool"; \
					echo "  install missing tools to run"; \
					exit 1; \
				fi ;; \
		esac; \
	done

check_bios_run_tools: check_build_tools
	@set -e; \
	if ! command -v "$(QEMU)" >/dev/null 2>&1; then \
		echo "  MISSING $(QEMU)"; \
		echo "  install qemu to run"; \
		exit 1; \
	fi

ask_version:
	@set -e; \
	if [ -t 0 ]; then \
		printf "do you want to update kernel version? [y/N] "; \
		read ans; \
		case "$$ans" in \
			y|Y|yes|YES) \
				cur_ver=$$(sh ../tools/toml_get.sh config.toml os release "0.0.0"); \
				if [ -z "$$cur_ver" ]; then \
					echo "  VERSION failed to parse kernel version from config.toml"; \
					exit 1; \
				fi; \
				IFS=.; set -- $$cur_ver; \
				major=$$1; minor=$$2; patch=$$3; \
				patch=$$((patch + 1)); \
				if [ $$patch -ge 10 ]; then \
					patch=0; \
					minor=$$((minor + 1)); \
				fi; \
				if [ $$minor -ge 10 ]; then \
					minor=0; \
					major=$$((major + 1)); \
				fi; \
				next_ver="$$major.$$minor.$$patch"; \
				sed -i 's/^release = "[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*"/release = "'"$$next_ver"'"/' config.toml; \
				echo "  VERSION $$cur_ver -> $$next_ver";; \
			*) \
				echo "  VERSION unchanged";; \
		esac; \
	else \
		echo "  VERSION prompt skipped (non-interactive shell)"; \
	fi

INIT_BIN = ../init/init

.PHONY: $(INIT_BIN)
$(INIT_BIN):
	@echo "  MAKE    init"
	@$(MAKE) -C ../init

.PHONY: $(YES_BIN)
$(YES_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/yes"
	@$(MAKE) -C $(PORTS_DIR) yes LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(FETCH_BIN)
$(FETCH_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/fetch"
	@$(MAKE) -C $(PORTS_DIR) fetch LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(SHELL_BIN)
$(SHELL_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/sh"
	@$(MAKE) -C $(PORTS_DIR) sh LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(OVEN_BIN)
$(OVEN_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/oven"
	@$(MAKE) -C $(PORTS_DIR) oven LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(CURSORD_BIN)
$(CURSORD_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/demons/cursord"
	@$(MAKE) -C $(PORTS_DIR) demons/cursord LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(SHMTEST_BIN)
$(SHMTEST_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/shmtest"
	@$(MAKE) -C $(PORTS_DIR) shmtest LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(UDP_ECHO_BIN)
$(UDP_ECHO_BIN): $(NATIVE_CRT0) $(NATIVE_LIBC)
	@echo "  MAKE    ports/udp_echo"
	@$(MAKE) -C $(PORTS_DIR) udp_echo LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(POSIX_HELLO_BIN)
$(POSIX_HELLO_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/posix_hello"
	@$(MAKE) -C $(PORTS_DIR) posix_hello LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(KBDTEST_BIN)
$(KBDTEST_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/kbdtest"
	@$(MAKE) -C $(PORTS_DIR) kbdtest LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(MUSL_TEST_BIN)
$(MUSL_TEST_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/musl_test"
	@$(MAKE) -C $(PORTS_DIR) musl_test LIBC_ENABLED=$(LIBC_ENABLED)

.PHONY: $(LUA_BIN)
$(LUA_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/lua"
	@$(MAKE) -C $(PORTS_DIR) lua LIBC_ENABLED=$(LIBC_ENABLED)

ifneq ($(CPYTHON_BIN),)
.PHONY: $(CPYTHON_BIN)
$(CPYTHON_BIN): $(LIBC_DEPS)
	@echo "  MAKE    ports/cpython"
	@$(MAKE) -C $(PORTS_DIR) cpython LIBC_ENABLED=$(LIBC_ENABLED) CPYTHON_ENABLED=$(CPYTHON_ENABLED)
endif

.PHONY: $(DLD_BIN)
$(DLD_BIN):
	@echo "  MAKE    libc/LibExec/dld"
	@$(MAKE) -C ../libc/LibExec/dld

.PHONY: $(NATIVE_SO)
$(NATIVE_SO):
	@echo "  MAKE    native libc shared"
	@$(MAKE) -C $(NATIVE_DIR) lib/native.so

$(MUSL_LIBC):
	@echo "  MAKE    musl libc"
	@$(MAKE) -C $(MUSL_DIR)

$(MUSL_CRT1): $(MUSL_LIBC)
	@true

$(LDSO_BIN): $(MUSL_LDSO)
	@echo "  CP      $@"
	@cp $(MUSL_LDSO) $@

$(ISO_IMAGE): check_build_tools $(BIOS_BOOT_IMAGE) $(UEFI_ESP_IMAGE) $(MAKEISO)
	@echo "  ISO     $(ISO_IMAGE)"
	@$(PYTHON) $(MAKEISO) --output $(ISO_IMAGE) \
		--bios-image $(BIOS_BOOT_IMAGE) \
		--efi-image $(UEFI_ESP_IMAGE) \
		--xorriso $(XORRISO)

$(BIOS_BUILD_DIR):
	@mkdir -p $(BIOS_BUILD_DIR)

$(BIOS_BUILD_DIR)/entry.o: boot/bootloader/bios/entry.S boot/bootloader/bios/bios.h | $(BIOS_BUILD_DIR)
	@echo "  AS      $<"
	@$(CC) $(BIOS_CFLAGS) -x assembler-with-cpp -c $< -o $@

$(BIOS_BUILD_DIR)/%.o: boot/bootloader/bios/%.c boot/bootloader/bios/bios.h | $(BIOS_BUILD_DIR)
	@echo "  CC      $<"
	@$(CC) $(BIOS_CFLAGS) -c $< -o $@

$(BIOS_BUILD_DIR)/lib_%.o: boot/bootloader/lib/%.c | $(BIOS_BUILD_DIR)
	@echo "  CC      $<"
	@$(CC) $(BIOS_CFLAGS) -c $< -o $@

$(BIOS_STAGE2_ELF): $(BIOS_STAGE2_OBJS) boot/bootloader/bios/linker.ld
	@echo "  LLD     $@"
	@$(LD) -m elf_i386 -T boot/bootloader/bios/linker.ld \
		$(BIOS_STAGE2_OBJS) -o $@

$(BIOS_STAGE2_BIN): $(BIOS_STAGE2_ELF)
	@echo "  OBJCOPY $@"
	@$(BIOS_OBJCOPY) -O binary $< $@

$(BIOS_STAGE1_BIN): boot/bootloader/bios/stage1.S $(BIOS_STAGE2_BIN) | $(BIOS_BUILD_DIR)
	@set -e; \
	size=$$(stat -c%s $(BIOS_STAGE2_BIN)); \
	sectors=$$(((size + 511) / 512)); \
	if [ $$sectors -gt $(BIOS_STAGE1_MAX_SECTORS) ]; then \
		echo "  BIOS    stage2 too large: $$sectors sectors"; \
		exit 1; \
	fi; \
	echo "  AS      boot/bootloader/bios/stage1.S"; \
	$(CC) $(BIOS_CFLAGS) -DSTAGE2_SECTORS=$$sectors \
		-x assembler-with-cpp -c boot/bootloader/bios/stage1.S \
		-o $(BIOS_STAGE1_OBJ); \
	$(LD) -m elf_i386 -Ttext 0x0 $(BIOS_STAGE1_OBJ) \
		-o $(BIOS_STAGE1_ELF); \
	$(BIOS_OBJCOPY) -O binary $(BIOS_STAGE1_ELF) $@; \
	stage1_size=$$(stat -c%s $@); \
	if [ $$stage1_size -ne 512 ]; then \
		echo "  BIOS    stage1 bad size: $$stage1_size"; \
		exit 1; \
	fi

$(BIOS_BOOTPACK): $(KERNEL_BIN) $(INIT_BIN) $(YES_BIN) $(FETCH_BIN) $(SHELL_BIN) $(OVEN_BIN) $(CURSORD_BIN) $(SHMTEST_BIN) $(UDP_ECHO_BIN) $(POSIX_HELLO_BIN) $(KBDTEST_BIN) $(ISO_MUSL_TEST_DEP) $(ISO_LUA_DEP) $(ISO_CPYTHON_DEP) $(ISO_LDSO_DEP) $(ISO_DRLD_DEP) $(ISO_NATIVE_SO_DEP) | $(BIOS_BUILD_DIR)
	@rm -rf $(BIOS_PAYLOAD_DIR)
	@mkdir -p $(BIOS_PAYLOAD_DIR)
	@cp $(KERNEL_BIN) $(BIOS_PAYLOAD_DIR)/kernel.bin
	@cp config.toml $(BIOS_PAYLOAD_DIR)/config.toml
	@for m in $(MULTIBOOT_MODULES); do \
		src=""; \
		case $$m in \
			init) src=$(INIT_BIN);; \
			yes) src=$(YES_BIN);; \
			fetch) src=$(FETCH_BIN);; \
			sh) src=$(SHELL_BIN);; \
			oven) src=$(OVEN_BIN);; \
			cursord) src=$(CURSORD_BIN);; \
			shmtest) src=$(SHMTEST_BIN);; \
			udp_echo) src=$(UDP_ECHO_BIN);; \
			posix_hello) src=$(POSIX_HELLO_BIN);; \
			kbdtest) src=$(KBDTEST_BIN);; \
			musl_test) src=$(MUSL_TEST_BIN);; \
			lua) src=$(LUA_BIN);; \
			python) src=$(CPYTHON_BIN);; \
			ld-musl) src=$(LDSO_BIN);; \
			drld) src=$(DLD_BIN);; \
			native-so) src=$(NATIVE_SO);; \
		esac; \
		if [ -n "$$src" ] && [ -f "$$src" ]; then \
			cp "$$src" "$(BIOS_PAYLOAD_DIR)/$$m"; \
		fi; \
	done
	@echo "  TAR     $(BIOS_BOOTPACK)"
	@tar -cf $(BIOS_BOOTPACK) -C $(BIOS_PAYLOAD_DIR) .

$(BIOS_LAYOUT_BIN): $(BIOS_BOOTPACK) | $(BIOS_BUILD_DIR)
	@set -e; \
	bytes=$$(stat -c%s $(BIOS_BOOTPACK)); \
	sectors=$$(((bytes + 511) / 512)); \
	echo "  LAYOUT  $(BIOS_LAYOUT_BIN)"; \
	perl -e 'print pack("V4", 0x3250424f, shift, shift, 0); print "\0" x (512 - 16)' \
		$$sectors $$bytes > $@

$(BIOS_BOOT_IMAGE): $(BIOS_STAGE1_BIN) $(BIOS_STAGE2_BIN) $(BIOS_LAYOUT_BIN) $(BIOS_BOOTPACK) | $(BIOS_BUILD_DIR)
	@set -e; \
	bytes=$$(stat -c%s $(BIOS_BOOTPACK)); \
	sectors=$$(((bytes + 511) / 512)); \
	final_size=$$((($(BIOS_BOOTPACK_LBA) + sectors) * 512)); \
	echo "  IMG     $(BIOS_BOOT_IMAGE)"; \
	rm -f $@; \
	dd if=$(BIOS_STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc 2>/dev/null; \
	dd if=$(BIOS_STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null; \
	dd if=$(BIOS_LAYOUT_BIN) of=$@ bs=512 seek=$(BIOS_LAYOUT_LBA) conv=notrunc 2>/dev/null; \
	dd if=$(BIOS_BOOTPACK) of=$@ bs=512 seek=$(BIOS_BOOTPACK_LBA) conv=notrunc 2>/dev/null; \
	truncate -s $$final_size $@; \
	total_sectors=$$((final_size / 512)); \
	part_sectors=$$((total_sectors - 1)); \
	perl -e 'open(my $$fh, "+<", shift) or die $$!; binmode $$fh; seek($$fh, 446, 0); print $$fh pack("C8VV", 0x80, 0x00, 0x01, 0x00, 0x83, 0xfe, 0xff, 0xff, 1, shift); close $$fh;' \
		$@ $$part_sectors

$(UEFI_BUILD_DIR):
	@mkdir -p $(UEFI_BUILD_DIR)

$(UEFI_BUILD_DIR)/trampoline.obj: boot/bootloader/uefi/trampoline.S | $(UEFI_BUILD_DIR)
	@echo "  AS      $<"
	@$(CC) $(UEFI_CFLAGS) -x assembler-with-cpp -c $< -o $@

$(UEFI_BUILD_DIR)/lib_%.obj: boot/bootloader/lib/%.c | $(UEFI_BUILD_DIR)
	@echo "  CC      $<"
	@$(CC) $(UEFI_CFLAGS) -c $< -o $@

$(UEFI_BOOTX64): boot/bootloader/uefi/main.zig $(UEFI_TRAMPOLINE_OBJ) $(UEFI_LIB_OBJS) | $(UEFI_BUILD_DIR)
	@echo "  ZIG     $@"
	@$(ZIG) build-exe boot/bootloader/uefi/main.zig \
		-target x86_64-uefi -O ReleaseSmall \
		-femit-bin=$@ $(UEFI_TRAMPOLINE_OBJ) $(UEFI_LIB_OBJS)

$(UEFI_ESP_IMAGE): $(UEFI_BOOTX64) $(BIOS_BOOTPACK) | $(UEFI_BUILD_DIR)
	@echo "  ESP     $(UEFI_ESP_IMAGE)"
	@rm -f $@
	@truncate -s 16M $@
	@mformat -i $@ -v OTSOSUEFI ::
	@mmd -i $@ ::/EFI ::/EFI/BOOT ::/boot
	@mcopy -i $@ $(UEFI_BOOTX64) ::/EFI/BOOT/BOOTX64.EFI
	@mcopy -i $@ $(BIOS_BOOTPACK) ::/boot/bootpack.tar

$(KERNEL_BIN): $(OBJ)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(OBJ) -o $@

$(BOOT_O): boot/boot.s linker.ld
	@echo "  AS      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(IDT_ASM_O): kernel/interrupts/idt.asm
	@echo "  AS      $<"
	@$(AS) -f $(NASM_FORMAT) -g -F dwarf $< -o $@

$(KERNEL_O): kernel/kernel.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(IDT_C_O): kernel/interrupts/idt.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PIC_O): kernel/interrupts/pic.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(HANDLERS_O): kernel/interrupts/handlers.c kernel/mm/vm/vm_map.h kernel/interrupts/apic/lapic.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PANIC_O): kernel/panic.c kernel/drivers/console/kms_console.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CONSOLE_O): kernel/console/console.c kernel/console/console.h kernel/console/palette.h kernel/console/terminal.h kernel/drivers/console/kms_console.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CHAINFS_O): kernel/drivers/fs/chainFS/chainfs.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TERMINAL_O): kernel/console/terminal.c kernel/console/terminal.h kernel/console/palette.h kernel/drivers/console/kms_console.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PTY_O): kernel/console/pty.c kernel/console/pty.h kernel/console/terminal.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PS2_O): kernel/drivers/keyboard/ps2.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KEYBOARD_O): kernel/drivers/keyboard/keyboard.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KEYMAP_O): kernel/drivers/keyboard/keymap.c kernel/drivers/keyboard/keymap.h kernel/drivers/keyboard/keycodes.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(MOUSE_PS2_O): kernel/drivers/mouse/ps2.c kernel/drivers/mouse/mouse.h kernel/event/event.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(FONT_O): kernel/drivers/video/font.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_CORE_O): kernel/drivers/video/drm/drm.c kernel/drivers/video/drm/drm.h kernel/drivers/video/card/virtio-gpu/virtio-gpu.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_GEM_O): kernel/drivers/video/drm/gem.c kernel/drivers/video/drm/gem.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_OBJ_O): kernel/drivers/video/drm/object.c kernel/drivers/video/drm/object.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_AUTH_O): kernel/drivers/video/drm/auth.c kernel/drivers/video/drm/auth.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_INIT_O): kernel/drivers/video/drm/init.c kernel/drivers/video/drm/init.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_FBDEV_O): kernel/drivers/video/drm/fbdev.c kernel/drivers/video/drm/fbdev.h kernel/drivers/video/drm/rapi/rapi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_KMS_FB_O): kernel/drivers/video/drm/kms/framebuffer.c kernel/drivers/video/drm/kms/framebuffer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_KMS_PLANE_O): kernel/drivers/video/drm/kms/plane.c kernel/drivers/video/drm/kms/plane.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_KMS_CRTC_O): kernel/drivers/video/drm/kms/crtc.c kernel/drivers/video/drm/kms/crtc.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_KMS_CONN_O): kernel/drivers/video/drm/kms/connector.c kernel/drivers/video/drm/kms/connector.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_KMS_ATOMIC_O): kernel/drivers/video/drm/kms/atomic.c kernel/drivers/video/drm/kms/atomic.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KMS_CONSOLE_O): kernel/drivers/console/kms_console.c kernel/drivers/console/kms_console.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_RAPI_O): kernel/drivers/video/drm/rapi/rapi.c kernel/drivers/video/drm/rapi/rapi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TIMER_O): kernel/drivers/timer.c kernel/drivers/eventtimer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENTTIMER_O): kernel/drivers/eventtimer.c kernel/drivers/eventtimer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PIT_O): kernel/drivers/timers/pit.c kernel/drivers/eventtimer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(APIC_TIMER_O): kernel/drivers/timers/apic.c kernel/drivers/eventtimer.h kernel/interrupts/apic/lapic.h kernel/drivers/timer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(LAPIC_O): kernel/interrupts/apic/lapic.c kernel/interrupts/apic/lapic.h kernel/drivers/timer.h kernel/other/config.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(IOAPIC_O): kernel/interrupts/apic/ioapic.c kernel/interrupts/apic/ioapic.h kernel/interrupts/apic/lapic.h kernel/drivers/acpi/acpi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(STDLIB_O): mlibc/stdlib.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KMEM_O): kernel/mm/kmem.c kernel/mm/kmem.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(UMA_O): kernel/mm/uma.c kernel/mm/uma.h kernel/mm/kmem.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PMAP_O): kernel/mm/vm/pmap.c kernel/mm/vm/pmap.h kernel/mm/vm/vm_page.h kernel/mm/kmem.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VM_PAGE_O): kernel/mm/vm/vm_page.c kernel/mm/vm/vm_page.h kernel/mm/kmem.h kernel/bootmem.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VM_MAP_O): kernel/mm/vm/vm_map.c kernel/mm/vm/vm_map.h kernel/mm/vm/vm_object.h kernel/mm/vm/vm_pager.h kernel/mm/vm/vm_page.h kernel/mm/vm/pmap.h kernel/process.h kernel/api/api.h kernel/api/shm.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VM_OBJECT_O): kernel/mm/vm/vm_object.c kernel/mm/vm/vm_object.h kernel/mm/vm/vm_pager.h kernel/mm/vm/vm_page.h kernel/mm/kmem.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VM_PAGER_O): kernel/mm/vm/vm_pager.c kernel/mm/vm/vm_pager.h kernel/mm/vm/vm_page.h kernel/mm/kmem.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(WRITE_O): kernel/api/write.c kernel/console/terminal.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OPEN_O): kernel/api/open.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SYSCALL_O): kernel/syscall.c kernel/api/shm.h kernel/other/config.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(READ_O): kernel/api/read.c kernel/api/api.h kernel/console/terminal.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CLOSE_O): kernel/api/close.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(LSEEK_O): kernel/api/lseek.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(WAIT_O): kernel/api/wait.c kernel/mm/vm/vm_map.h kernel/mm/vm/pmap.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(MMAP_O): kernel/api/mmap.c kernel/api/api.h kernel/mm/vm/vm_map.h kernel/mm/vm/vm_object.h kernel/mm/vm/vm_page.h kernel/drivers/video/drm/gem.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SHM_O): kernel/api/shm.c kernel/api/shm.h kernel/api/api.h kernel/mm/vm/vm_map.h kernel/mm/vm/vm_object.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(API_NET_O): kernel/api/net.c kernel/api/api.h kernel/net/endpoint.h kernel/useraddr.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PIPE_O): kernel/api/pipe.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(UNAME_O): kernel/api/info.c kernel/api/api.h kernel/other/config.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(MEMINFO_O): kernel/api/meminfo.c kernel/api/api.h kernel/mm/vm/vm_page.h kernel/mm/kmem.h kernel/bootmem.h kernel/process.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CPUINFO_O): kernel/api/cpuinfo.c kernel/api/api.h kernel/smp/smp.h kernel/process.h kernel/thread.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CLONE_O): kernel/api/clone.c kernel/mm/vm/vm_map.h kernel/mm/vm/pmap.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(GDT_O): kernel/gdt.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(GDT_ASM_O): kernel/gdt.asm
	@echo "  AS      $<"
	@$(AS) -f $(NASM_FORMAT) -g -F dwarf $< -o $@

$(PROCESS_O): kernel/process.c kernel/mm/vm/vm_map.h kernel/mm/vm/pmap.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(THREAD_O): kernel/thread.c kernel/thread.h kernel/process.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(COPY_O): kernel/api/copy.c kernel/mm/vm/vm_map.h kernel/mm/vm/pmap.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SPAWN_O): kernel/api/spawn.c kernel/mm/vm/vm_page.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SESSION_O): kernel/api/session.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CHDIR_O): kernel/api/chdir.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(GETCWD_O): kernel/api/getcwd.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(LISTDIR_O): kernel/api/listdir.c kernel/drivers/fs/vfs/vfs.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(FS_META_O): kernel/api/fs_meta.c kernel/api/api.h kernel/drivers/fs/vfs/vfs.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PROCLIST_O): kernel/api/proclist.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(USERADDR_O): kernel/useraddr.c kernel/useraddr.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SCHEDULER_O): kernel/scheduler.c kernel/scheduler.h kernel/process.h kernel/mm/vm/pmap.h kernel/gdt.h kernel/interrupts/idt.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(ELF_O): userland/elf.zig kernel/mm/vm/vm_page.h
	@echo "  ZIG     $<"
	@$(ZIG) build-obj -femit-bin=$@ -target $(ZIG_TARGET) -mcpu=x86_64-sse-mmx-sse2-avx-avx2 -mcmodel=kernel -O ReleaseSafe --dep panic -Melf=userland/elf.zig -Mpanic=kernel/panic.zig

$(USERSPACE_O): userland/userspace.c userland/userspace.h kernel/process.h kernel/mm/vm/pmap.h kernel/mm/vm/vm_page.h kernel/gdt.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(USERSPACE_ASM_O): userland/userspace.asm
	@echo "  AS      $<"
	@$(AS) -f $(NASM_FORMAT) -g -F dwarf $< -o $@

$(ACPI_O): kernel/drivers/acpi/acpi.c kernel/drivers/acpi/acpi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(ACPI_TABLES_O): kernel/drivers/acpi/tables.c kernel/drivers/acpi/acpi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SMP_O): kernel/smp/smp.c kernel/smp/smp.h kernel/gdt.h kernel/interrupts/apic/lapic.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SMP_ASM_O): kernel/smp/smp.asm
	@echo "  AS      $<"
	@$(AS) -f $(NASM_FORMAT) -g -F dwarf $< -o $@

$(POWER_O): kernel/drivers/power/power.c kernel/drivers/power/power.h kernel/drivers/acpi/acpi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POWER_CTRL_O): kernel/drivers/power/controller.c kernel/drivers/power/power.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POWER_PBUTTON_O): kernel/drivers/power/pbutton.c kernel/drivers/power/pbutton.h kernel/drivers/acpi/acpi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PCI_CORE_O): kernel/pci/core/pci.c kernel/pci/pci.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PCI_SCAN_O): kernel/pci/core/scan.c kernel/pci/pci.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PCI_CONFIG_O): kernel/pci/core/config.c kernel/pci/pci.h kernel/pci/utils/io.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PCI_DEVICE_O): kernel/pci/core/device.c kernel/pci/pci.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PCI_BAR_O): kernel/pci/utils/bar.c kernel/pci/utils/bar.h kernel/pci/pci.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(WATCHDOG_O): kernel/drivers/watchdog/watchdog.c kernel/drivers/watchdog/watchdog.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(WATCHDOG_I6300ESB_O): kernel/drivers/watchdog/devices/i6300esb.c kernel/drivers/watchdog/watchdog.h kernel/pci/pci.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(WATCHDOG_ICH_TCO_O): kernel/drivers/watchdog/devices/ich_tco.c kernel/drivers/watchdog/watchdog.h kernel/pci/pci.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TOML_O): mlibc/toml.c mlibc/toml.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KUSR_O): kernel/other/kusr.c kernel/other/kusr.h mlibc/toml.h kernel/crypto/crypto.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KUSR_AUTH_O): kernel/api/kusr_auth.c kernel/api/api.h kernel/process.h mlibc/toml.h kernel/crypto/crypto.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DRM_API_O): kernel/api/drm.c kernel/api/api.h kernel/drivers/video/drm/drm.h kernel/drivers/video/drm/gem.h kernel/drivers/video/drm/kms/atomic.h kernel/drivers/video/drm/rapi/rapi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(RESTRICT_O): kernel/other/restrict.c kernel/other/restrict.h kernel/process.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CONFIG_O): kernel/other/config.c kernel/other/config.h mlibc/toml.h kernel/drivers/fs/vfs/vfs.h kernel/drivers/timer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CRYPTO_UTIL_O): kernel/crypto/util/crypto_util.c kernel/crypto/util/crypto_util.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CRYPTO_SHA256_O): kernel/crypto/hash/sha256.c kernel/crypto/hash/sha256.h kernel/crypto/util/crypto_util.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CRYPTO_HMAC_O): kernel/crypto/hmac/hmac_sha256.c kernel/crypto/hmac/hmac_sha256.h kernel/crypto/hash/sha256.h kernel/crypto/util/crypto_util.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CRYPTO_PBKDF2_O): kernel/crypto/kdf/pbkdf2.c kernel/crypto/kdf/pbkdf2.h kernel/crypto/hmac/hmac_sha256.h kernel/crypto/util/crypto_util.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CRYPTO_RNG_O): kernel/crypto/rng/rng.c kernel/crypto/rng/rng.h kernel/crypto/hash/sha256.h kernel/crypto/util/crypto_util.h kernel/drivers/timer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CRYPTO_CHACHA20_O): kernel/crypto/cipher/chacha20.c kernel/crypto/cipher/chacha20.h kernel/crypto/util/crypto_util.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_HW_O): kernel/drivers/virtio/virtio_hw.c kernel/drivers/virtio/virtio_hw.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_QUEUE_O): kernel/drivers/virtio/virtio_queue.c kernel/drivers/virtio/virtio_queue.h kernel/drivers/virtio/virtio_hw.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_PCI_O): kernel/drivers/virtio/virtio_pci.c kernel/drivers/virtio/virtio_pci.h kernel/drivers/virtio/virtio_hw.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_GPU_CMDS_O): kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.c kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.h kernel/drivers/virtio/virtio_hw.h kernel/drivers/virtio/virtio_queue.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_GPU_O): kernel/drivers/video/card/virtio-gpu/virtio-gpu.c kernel/drivers/video/card/virtio-gpu/virtio-gpu.h kernel/drivers/virtio/virtio_pci.h kernel/drivers/virtio/virtio_queue.h kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.h kernel/drivers/video/drm/drm.h kernel/drivers/video/drm/kms/crtc.h kernel/drivers/video/drm/rapi/rapi.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NETDEV_O): kernel/net/netdev/netdev.c kernel/net/netdev/netdev.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_NET_O): kernel/drivers/net/virtio-net/virtio_net.c kernel/drivers/net/virtio-net/virtio_net.h kernel/drivers/virtio/virtio_pci.h kernel/drivers/virtio/virtio_queue.h kernel/net/net.h kernel/net/netdev/netdev.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NET_O): kernel/net/net.c kernel/net/net.h kernel/net/netdev/netdev.h kernel/net/ethernet.h kernel/net/endpoint.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NET_ETHERNET_O): kernel/net/ethernet.c kernel/net/ethernet.h kernel/net/net.h kernel/net/netdev/netdev.h kernel/net/arp.h kernel/net/ipv4.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NET_ARP_O): kernel/net/arp.c kernel/net/arp.h kernel/net/ethernet.h kernel/drivers/timer.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NET_IPV4_O): kernel/net/ipv4.c kernel/net/ipv4.h kernel/net/icmp.h kernel/net/udp.h kernel/net/arp.h kernel/net/ethernet.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NET_ICMP_O): kernel/net/icmp.c kernel/net/icmp.h kernel/net/ipv4.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NET_UDP_O): kernel/net/udp.c kernel/net/udp.h kernel/net/ipv4.h kernel/net/ethernet.h kernel/net/endpoint.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NET_ENDPOINT_O): kernel/net/endpoint.c kernel/net/endpoint.h kernel/net/udp.h kernel/net/ipv4.h kernel/event/event.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(NET_LOOPBACK_O): kernel/net/loopback.c kernel/net/loopback.h kernel/net/net.h kernel/net/netdev/netdev.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(UNIX_SOCK_O): kernel/drivers/net/unix_sock.c kernel/drivers/net/unix_sock.h kernel/drivers/fs/vfs/vfs.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@
$(SYSCALL_ASM_O): kernel/syscall_asm.asm
	@echo "  AS      $<"
	@$(AS) -f $(NASM_FORMAT) -g -F dwarf $< -o $@

$(EVENT_O): kernel/event/event.c kernel/event/event.h kernel/api/api.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_READ_O): kernel/event/filter_read.c kernel/event/event.h kernel/net/endpoint.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_WRITE_O): kernel/event/filter_write.c kernel/event/event.h kernel/net/endpoint.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_TIMER_O): kernel/event/filter_timer.c kernel/event/event.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_PROC_O): kernel/event/filter_proc.c kernel/event/event.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_SIGNAL_O): kernel/event/filter_signal.c kernel/event/event.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_USER_O): kernel/event/filter_user.c kernel/event/event.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_KBD_O): kernel/event/filter_kbd.c kernel/event/event.h kernel/drivers/keyboard/keyboard.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_MOUSE_O): kernel/event/filter_mouse.c kernel/event/event.h kernel/drivers/mouse/mouse.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TERM_POWER_O): kernel/api/term_power.c kernel/api/api.h kernel/console/terminal.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TERM_INFO_O): kernel/api/term_info.c kernel/api/api.h kernel/console/terminal.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(INPUT_O): kernel/api/input.c kernel/api/api.h kernel/drivers/keyboard/keyboard.h kernel/console/terminal.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VFS_O): kernel/drivers/fs/vfs/vfs.c kernel/drivers/fs/vfs/vfs.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DEVFS_O): kernel/drivers/fs/devfs/devfs.c kernel/drivers/fs/devfs/devfs.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(LINK_O): kernel/api/link.c kernel/api/api.h kernel/drivers/fs/vfs/vfs.h kernel/other/restrict.h kernel/useraddr.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POSIX_O): kernel/api/posix/posix.c kernel/api/posix/posix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POSIX_IO_O): kernel/api/posix/posix_io.c kernel/api/posix/posix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POSIX_PROC_O): kernel/api/posix/posix_proc.c kernel/api/posix/posix.h kernel/other/config.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POSIX_FS_O): kernel/api/posix/posix_fs.c kernel/api/posix/posix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POSIX_MEM_O): kernel/api/posix/posix_mem.c kernel/api/posix/posix.h kernel/api/shm.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POSIX_SOCKET_O): kernel/api/posix/posix_socket.c kernel/api/posix/posix_socket.h kernel/api/posix/posix.h kernel/drivers/fs/vfs/vfs.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@
$(POSIX_SIGNAL_O): kernel/api/posix/posix_signal.c kernel/api/posix/posix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SIGNAL_O): kernel/api/signal.c kernel/api/signal.h kernel/process.h kernel/thread.h kernel/signal.h kernel/api/posix/posix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PROC_PID_O): kernel/api/proc_pid.c kernel/api/api.h kernel/process.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(THREAD_API_O): kernel/api/thread_api.c kernel/api/api.h kernel/process.h kernel/thread.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(FUTEX_O): kernel/futex.c kernel/thread.h kernel/process.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SYS_O): kernel/api/sys.c kernel/api/api.h kernel/crypto/rng/rng.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@


$(STRING_O): mlibc/string.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(ITOA_O): mlibc/itoa.s
	@echo "  AS      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(HARDWARE_O): mlibc/hardware.asm
	@echo "  AS      $<"
	@$(AS) -f $(NASM_FORMAT) -g -F dwarf $< -o $@ -D__x86_64__




$(DISK_O): kernel/drivers/disk/disk.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(RAMDISK_O): kernel/drivers/disk/ramdisk/ramdisk.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@


$(UART_O): kernel/drivers/uart/uart.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(STDIO_O): mlibc/stdio.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TIME_O): kernel/time.c kernel/time.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CLOCKSOURCE_O): kernel/time/clocksource.c kernel/time/clocksource.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(ET_CLOCKSOURCE_O): kernel/time/et_clocksource.c kernel/time.h kernel/time/clocksource.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CMOS_RTC_O): kernel/drivers/rtc/cmos_rtc.c kernel/drivers/rtc/rtc.h kernel/time.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(API_TIME_O): kernel/api/time.c kernel/api/api.h kernel/time.h kernel/time/clocksource.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BOOTMEM_O): kernel/bootmem.c kernel/bootmem.h kernel/multiboot.h kernel/multiboot2.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DISK_IMG):
	@echo "  IMG     $(DISK_IMG)"
	@mkdir -p $(dir $(DISK_IMG))
	@dd if=/dev/zero of=$(DISK_IMG) bs=1M count=16 2>/dev/null

OVMF ?= /usr/share/ovmf/OVMF.fd

run: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (UEFI)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_GUI_OPTS) -device i6300esb -watchdog-action reset -smp 2

run_nodis: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (UEFI, terminal)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_NODISPLAY_OPTS) -device i6300esb -watchdog-action reset

run_bios: check_bios_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (BIOS loader)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -device i6300esb -watchdog-action reset

virtio: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (virtio-gpu, virtio-net)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_GUI_OPTS) -device virtio-gpu-pci -netdev user,id=net0 -device virtio-net-pci,netdev=net0,disable-legacy=on -device i6300esb -watchdog-action reset

virtio_nodis: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (virtio-gpu, virtio-net, terminal)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_NODISPLAY_OPTS) -device virtio-gpu-pci -netdev user,id=net0 -device virtio-net-pci,netdev=net0,disable-legacy=on -device i6300esb -watchdog-action reset

virtio_udp: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (virtio-net, UDP echo hostfwd :10007 -> :7)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_GUI_OPTS) -device virtio-gpu-pci -netdev user,id=net0,hostfwd=udp::10007-:7 -device virtio-net-pci,netdev=net0,disable-legacy=on -device i6300esb -watchdog-action reset

virtio_udp_nodis: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (virtio-net, terminal, UDP echo hostfwd :10007 -> :7)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_NODISPLAY_OPTS) -device virtio-gpu-pci -netdev user,id=net0,hostfwd=udp::10007-:7 -device virtio-net-pci,netdev=net0,disable-legacy=on -device i6300esb -watchdog-action reset

debug: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  DEBUG   QEMU"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) -d int,cpu_reset > debug.txt 2>&1 -no-reboot $(QEMU_GUI_OPTS) -device i6300esb -watchdog-action reset

clean:
	@echo "  CLEAN"
	@rm -f ../bin/*.o ../bin/kernel.bin ../bin/kernel.map $(ISO_IMAGE) $(DISK_IMG)
	@rm -rf $(BIOS_BUILD_DIR) $(UEFI_BUILD_DIR)
