TARGET ?= x86_64-elf
CC = clang
ZIG = zig
LD = ld.lld
AS = nasm
QEMU = qemu-system-x86_64
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
ISODIR = ../isodir
DISK_IMG = ../bin/disk.img
PORTS_DIR = ../ports
MUSL_DIR = ../libc/musl
MUSL_LIBC = $(MUSL_DIR)/lib/libc.a
MUSL_CRT1 = $(MUSL_DIR)/lib/crt1.o
YES_BIN = $(PORTS_DIR)/yes
FETCH_BIN = $(PORTS_DIR)/fetch
SHELL_BIN = $(PORTS_DIR)/sh
POSIX_HELLO_BIN = $(PORTS_DIR)/posix_hello
KBDTEST_BIN = $(PORTS_DIR)/kbdtest
MUSL_TEST_BIN = $(PORTS_DIR)/musl_test
LIBC_ENABLED := $(shell sh ../tools/toml_get.sh config.toml libc enabled true)

ifeq ($(LIBC_ENABLED),true)
ISO_MUSL_TEST_DEP = $(MUSL_TEST_BIN)
ISO_MUSL_TEST_COPY = @cp $(MUSL_TEST_BIN) $(ISODIR)/boot/musl_test
GRUB_MUSL_TEST = @echo '    module2 /boot/musl_test musl_test' >> $(ISODIR)/boot/grub/grub.cfg
LIBC_DEPS = $(MUSL_LIBC) $(MUSL_CRT1)
else
ISO_MUSL_TEST_DEP =
ISO_MUSL_TEST_COPY =
GRUB_MUSL_TEST =
LIBC_DEPS =
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
PS2_O = ../bin/ps2.o
KEYBOARD_O = ../bin/keyboard.o
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

PIPE_O = ../bin/pipe.o
CLONE_O = ../bin/clone.o
SYSCALL_O = ../bin/syscall.o
UNAME_O = ../bin/uname.o
MEMINFO_O = ../bin/meminfo.o
GDT_O = ../bin/gdt.o
GDT_ASM_O = ../bin/gdt_asm.o
PROCESS_O = ../bin/process.o
COPY_O = ../bin/proc_copy.o
SPAWN_O = ../bin/spawn.o
THREAD_O = ../bin/thread.o
CHDIR_O = ../bin/chdir.o
GETCWD_O = ../bin/getcwd.o
LISTDIR_O = ../bin/listdir.o
PROCLIST_O = ../bin/proclist.o
ELF_O = ../bin/elf.o
USERSPACE_O = ../bin/userspace.o
USERSPACE_ASM_O = ../bin/userspace_asm.o
SYSCALL_ASM_O = ../bin/syscall_asm.o
USERADDR_O = ../bin/useraddr.o
SCHEDULER_O = ../bin/scheduler.o
ACPI_O = ../bin/acpi.o
ACPI_TABLES_O = ../bin/acpi_tables.o
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
VIRTIO_GPU_CMDS_O = ../bin/virtio_gpu_cmds.o
VIRTIO_GPU_O = ../bin/virtio_gpu.o

EVENT_O = ../bin/event.o
EVENT_READ_O = ../bin/event_read.o
EVENT_WRITE_O = ../bin/event_write.o
EVENT_TIMER_O = ../bin/event_timer.o
EVENT_PROC_O = ../bin/event_proc.o
EVENT_SIGNAL_O = ../bin/event_signal.o
EVENT_USER_O = ../bin/event_user.o
EVENT_KBD_O = ../bin/event_kbd.o

VFS_O = ../bin/vfs.o
DEVFS_O = ../bin/devfs.o
POSIX_O = ../bin/posix.o
POSIX_IO_O = ../bin/posix_io.o
POSIX_PROC_O = ../bin/posix_proc.o
POSIX_FS_O = ../bin/posix_fs.o
POSIX_MEM_O = ../bin/posix_mem.o
POSIX_SIGNAL_O = ../bin/posix_signal.o
PROC_PID_O = ../bin/proc_pid.o
THREAD_API_O = ../bin/thread_api.o
FUTEX_O = ../bin/futex.o
SYS_O = ../bin/sys.o
API_TIME_O = ../bin/api_time.o
TERM_POWER_O = ../bin/term_power.o

TIME_O = ../bin/time.o
CLOCKSOURCE_O = ../bin/clocksource.o
ET_CLOCKSOURCE_O = ../bin/et_clocksource.o
CMOS_RTC_O = ../bin/cmos_rtc.o

OBJ = $(BOOT_O) $(KERNEL_O) $(STRING_O) $(ITOA_O) $(HARDWARE_O) $(UART_O) $(STDIO_O) $(BOOTMEM_O) $(IDT_ASM_O) $(IDT_C_O) $(PIC_O) \
      $(HANDLERS_O) $(PANIC_O) $(KMEM_O) $(UMA_O) $(PMAP_O) $(VM_PAGE_O) $(VM_MAP_O) $(VM_OBJECT_O)       $(VM_PAGER_O) $(DISK_OBJ) $(DISK_O) $(RAMDISK_O) $(CHAINFS_O) $(CONSOLE_O) $(TERMINAL_O) $(PS2_O) $(KEYBOARD_O) $(FONT_O) $(DRM_CORE_O) $(DRM_GEM_O) $(DRM_OBJ_O) $(DRM_AUTH_O) $(DRM_INIT_O) $(DRM_FBDEV_O) $(DRM_KMS_FB_O) $(DRM_KMS_PLANE_O) $(DRM_KMS_CRTC_O) $(DRM_KMS_CONN_O) $(DRM_KMS_ATOMIC_O) $(KMS_CONSOLE_O) $(DRM_RAPI_O) $(TIMER_O) $(EVENTTIMER_O) $(PIT_O) $(STDLIB_O) $(WRITE_O) $(READ_O) $(OPEN_O) $(CLOSE_O) $(LSEEK_O) $(WAIT_O) $(MMAP_O) $(PIPE_O) $(CLONE_O) $(SYSCALL_O) \
      $(GDT_O) $(GDT_ASM_O) $(PROCESS_O) $(COPY_O) $(SPAWN_O) $(THREAD_O) $(CHDIR_O) $(GETCWD_O) $(LISTDIR_O) $(PROCLIST_O) $(ELF_O) $(USERSPACE_O) $(USERSPACE_ASM_O) $(SYSCALL_ASM_O) $(USERADDR_O) $(SCHEDULER_O) \
      $(UNAME_O) $(MEMINFO_O) \
      $(ACPI_O) $(ACPI_TABLES_O) $(POWER_O) $(POWER_CTRL_O) $(POWER_PBUTTON_O) \
      $(PCI_CORE_O) $(PCI_SCAN_O) $(PCI_CONFIG_O) $(PCI_DEVICE_O) $(PCI_BAR_O) \
      $(WATCHDOG_O) $(WATCHDOG_I6300ESB_O) $(WATCHDOG_ICH_TCO_O) \
      $(KSHELL_OBJ) \
      $(TOML_O) $(KUSR_O) $(KUSR_AUTH_O) $(CONFIG_O) $(DRM_API_O) $(RESTRICT_O) \
      $(CRYPTO_UTIL_O) $(CRYPTO_SHA256_O) $(CRYPTO_HMAC_O) $(CRYPTO_PBKDF2_O) $(CRYPTO_RNG_O) $(CRYPTO_CHACHA20_O) \
      $(VIRTIO_HW_O) $(VIRTIO_QUEUE_O) $(VIRTIO_GPU_CMDS_O) $(VIRTIO_GPU_O) \
      $(EVENT_O) $(EVENT_READ_O) $(EVENT_WRITE_O) $(EVENT_TIMER_O) \
      $(EVENT_PROC_O) $(EVENT_SIGNAL_O) $(EVENT_USER_O) \
      $(EVENT_KBD_O) \
      $(VFS_O) $(DEVFS_O) \
      $(POSIX_O) $(POSIX_IO_O) $(POSIX_PROC_O) $(POSIX_FS_O) \
      $(POSIX_MEM_O) $(POSIX_SIGNAL_O) \
      $(PROC_PID_O) $(THREAD_API_O) $(FUTEX_O) $(SYS_O) \
      $(API_TIME_O) $(TERM_POWER_O) \
      $(TIME_O) $(CLOCKSOURCE_O) $(ET_CLOCKSOURCE_O) $(CMOS_RTC_O)

.PHONY: all ask_version check_build_tools check_run_tools

all: check_build_tools ask_version $(ISO_IMAGE)

check_build_tools:
	@set -e; \
	for tool in "$(CC)" "$(LD)" "$(AS)" "$(ZIG)" grub-mkrescue llvm-size; do \
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

$(MUSL_LIBC):
	@echo "  MAKE    musl libc"
	@$(MAKE) -C $(MUSL_DIR)

$(MUSL_CRT1): $(MUSL_LIBC)
	@true

$(ISO_IMAGE): check_build_tools $(KERNEL_BIN) $(INIT_BIN) $(YES_BIN) $(FETCH_BIN) $(SHELL_BIN) $(POSIX_HELLO_BIN) $(KBDTEST_BIN) $(ISO_MUSL_TEST_DEP)
	@mkdir -p $(ISODIR)/boot/grub
	@cp $(KERNEL_BIN) $(ISODIR)/boot/kernel.bin
	@cp $(INIT_BIN) $(ISODIR)/boot/init
	@cp $(YES_BIN) $(ISODIR)/boot/yes
	@cp $(FETCH_BIN) $(ISODIR)/boot/fetch
	@cp $(SHELL_BIN) $(ISODIR)/boot/sh
	@cp $(POSIX_HELLO_BIN) $(ISODIR)/boot/posix_hello
	@cp $(KBDTEST_BIN) $(ISODIR)/boot/kbdtest
	$(ISO_MUSL_TEST_COPY)
	@cp config.toml $(ISODIR)/boot/config.toml
	@echo 'set timeout=4' > $(ISODIR)/boot/grub/grub.cfg
	@echo 'set default=0' >> $(ISODIR)/boot/grub/grub.cfg
	@echo 'insmod all_video' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '' >> $(ISODIR)/boot/grub/grub.cfg
	@echo 'menuentry "OTSOS" {' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    multiboot2 /boot/kernel.bin' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/config.toml config' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/init init' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/yes yes' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/fetch fetch' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/sh sh' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/posix_hello posix_hello' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/kbdtest kbdtest' >> $(ISODIR)/boot/grub/grub.cfg
	$(GRUB_MUSL_TEST)
	@echo '    boot' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '}' >> $(ISODIR)/boot/grub/grub.cfg
	@echo "  ISO     $(ISO_IMAGE)"
	@grub-mkrescue -o $(ISO_IMAGE) $(ISODIR) >/dev/null 2>&1

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

$(HANDLERS_O): kernel/interrupts/handlers.c kernel/mm/vm/vm_map.h
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

$(PS2_O): kernel/drivers/keyboard/ps2.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KEYBOARD_O): kernel/drivers/keyboard/keyboard.c
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

$(DRM_FBDEV_O): kernel/drivers/video/drm/fbdev.c kernel/drivers/video/drm/fbdev.h
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

$(PIT_O): kernel/drivers/pit.c kernel/drivers/eventtimer.h
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

$(VM_MAP_O): kernel/mm/vm/vm_map.c kernel/mm/vm/vm_map.h kernel/mm/vm/vm_object.h kernel/mm/vm/vm_pager.h kernel/mm/vm/vm_page.h kernel/mm/vm/pmap.h kernel/process.h kernel/api/api.h
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

$(SYSCALL_O): kernel/syscall.c
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

$(PIPE_O): kernel/api/pipe.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(UNAME_O): kernel/api/info.c kernel/api/api.h kernel/other/config.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(MEMINFO_O): kernel/api/meminfo.c kernel/api/api.h kernel/mm/vm/vm_page.h kernel/mm/kmem.h kernel/bootmem.h kernel/process.h
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

$(CHDIR_O): kernel/api/chdir.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(GETCWD_O): kernel/api/getcwd.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(LISTDIR_O): kernel/api/listdir.c kernel/drivers/fs/vfs/vfs.h
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

$(CONFIG_O): kernel/other/config.c kernel/other/config.h mlibc/toml.h
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

$(VIRTIO_HW_O): kernel/drivers/video/card/virtio-gpu/virtio_hw.c kernel/drivers/video/card/virtio-gpu/virtio_hw.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_QUEUE_O): kernel/drivers/video/card/virtio-gpu/virtio_queue.c kernel/drivers/video/card/virtio-gpu/virtio_queue.h kernel/drivers/video/card/virtio-gpu/virtio_hw.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_GPU_CMDS_O): kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.c kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.h kernel/drivers/video/card/virtio-gpu/virtio_hw.h kernel/drivers/video/card/virtio-gpu/virtio_queue.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VIRTIO_GPU_O): kernel/drivers/video/card/virtio-gpu/virtio-gpu.c kernel/drivers/video/card/virtio-gpu/virtio-gpu.h kernel/drivers/video/card/virtio-gpu/virtio_hw.h kernel/drivers/video/card/virtio-gpu/virtio_queue.h kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.h kernel/drivers/video/drm/drm.h kernel/drivers/video/drm/kms/crtc.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SYSCALL_ASM_O): kernel/syscall_asm.asm
	@echo "  AS      $<"
	@$(AS) -f $(NASM_FORMAT) -g -F dwarf $< -o $@

$(EVENT_O): kernel/event/event.c kernel/event/event.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_READ_O): kernel/event/filter_read.c kernel/event/event.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EVENT_WRITE_O): kernel/event/filter_write.c kernel/event/event.h
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

$(TERM_POWER_O): kernel/api/term_power.c kernel/api/api.h kernel/console/terminal.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VFS_O): kernel/drivers/fs/vfs/vfs.c kernel/drivers/fs/vfs/vfs.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DEVFS_O): kernel/drivers/fs/devfs/devfs.c kernel/drivers/fs/devfs/devfs.h
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

$(POSIX_MEM_O): kernel/api/posix/posix_mem.c kernel/api/posix/posix.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(POSIX_SIGNAL_O): kernel/api/posix/posix_signal.c kernel/api/posix/posix.h
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
	@dd if=/dev/zero of=$(DISK_IMG) bs=1M count=10 2>/dev/null

OVMF ?= /usr/share/ovmf/OVMF.fd

run: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (UEFI)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_GUI_OPTS) -device i6300esb -watchdog-action reset

run_nodis: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (UEFI, terminal)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_NODISPLAY_OPTS) -device i6300esb -watchdog-action reset

run_bios: check_build_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -device i6300esb -watchdog-action reset

virtio: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (virtio-gpu)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_GUI_OPTS) -device virtio-gpu-pci -device i6300esb -watchdog-action reset

virtio_nodis: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (virtio-gpu, terminal)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_NODISPLAY_OPTS) -device virtio-gpu-pci -device i6300esb -watchdog-action reset

debug: check_run_tools $(ISO_IMAGE) $(DISK_IMG)
	@echo "  DEBUG   QEMU"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) -d int,cpu_reset > debug.txt 2>&1 -no-reboot $(QEMU_GUI_OPTS) -device i6300esb -watchdog-action reset

clean:
	@echo "  CLEAN"
	@rm -f ../bin/*.o ../bin/kernel.bin ../bin/kernel.map $(ISO_IMAGE) $(DISK_IMG)
	@rm -rf $(ISODIR)/boot/kernel.bin $(ISODIR)/boot/config.toml
