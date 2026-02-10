CC = clang
ZIG = zig
LD = ld.lld
AS = nasm
QEMU = qemu-system-x86_64
QEMU_GUI_OPTS = -display gtk

CFLAGS = -m64 -ffreestanding -fno-pic -fstack-protector-strong -g -O0 -fno-builtin -mno-red-zone -mcmodel=kernel -mno-sse -mno-mmx -mno-sse2 -mno-avx -mno-80387 -I.
LDFLAGS = -m elf_x86_64 -T linker.ld -Map=../bin/kernel.map

KERNEL_BIN = ../bin/kernel.bin
ISO_IMAGE = ../otsos.iso
ISODIR = ../isodir
DISK_IMG = ../bin/disk.img
PORTS_DIR = ../ports
YES_BIN = $(PORTS_DIR)/yes
# object files
BOOT_O = ../bin/boot.o
KERNEL_O = ../bin/kernel.o
STRING_O = ../bin/string.o
ITOA_O = ../bin/itoa.o
HARDWARE_O = ../bin/hardware.o
COM1_O = ../bin/com1.o
IDT_ASM_O = ../bin/idt_asm.o
IDT_C_O = ../bin/idt_c.o
PIC_O = ../bin/pic_c.o
HANDLERS_O = ../bin/handlers_c.o
PANIC_O = ../bin/panic.o
MEMORY_O = ../bin/memory.o
PATA_O = ../bin/pata.o
DISK_O = ../bin/disk.o
RAMDISK_O = ../bin/ramdisk.o
CHAINFS_O = ../bin/chainfs.o
VGA_O = ../bin/vga.o
TTY_O = ../bin/tty.o
PS2_O = ../bin/ps2.o
KEYBOARD_O = ../bin/keyboard.o
FB_O = ../bin/fb.o
FONT_O = ../bin/font.o
TIMER_O = ../bin/timer.o
STDLIB_O = ../bin/stdlib.o
MMU_O = ../bin/mmu.o
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
GDT_O = ../bin/gdt.o
GDT_ASM_O = ../bin/gdt_asm.o
PROCESS_O = ../bin/process.o
FORK_O = ../bin/fork.o
EXEC_O = ../bin/exec.o
ELF_O = ../bin/elf.o
USERSPACE_O = ../bin/userspace.o
USERSPACE_ASM_O = ../bin/userspace_asm.o
SYSCALL_ASM_O = ../bin/syscall_asm.o
USERADDR_O = ../bin/useraddr.o
SCHEDULER_O = ../bin/scheduler.o

OBJ = $(BOOT_O) $(KERNEL_O) $(STRING_O) $(ITOA_O) $(HARDWARE_O) $(COM1_O) $(IDT_ASM_O) $(IDT_C_O) $(PIC_O) \
      $(HANDLERS_O) $(PANIC_O) $(MEMORY_O) $(PATA_O) $(DISK_O) $(RAMDISK_O) $(CHAINFS_O) $(VGA_O) $(TTY_O) $(PS2_O) $(KEYBOARD_O) $(FB_O) $(FONT_O) $(TIMER_O) $(STDLIB_O) $(MMU_O) $(WRITE_O) $(READ_O) $(OPEN_O) $(CLOSE_O) $(LSEEK_O) $(WAIT_O) $(MMAP_O) $(PIPE_O) $(CLONE_O) $(SYSCALL_O) \
      $(GDT_O) $(GDT_ASM_O) $(PROCESS_O) $(FORK_O) $(EXEC_O) $(ELF_O) $(USERSPACE_O) $(USERSPACE_ASM_O) $(SYSCALL_ASM_O) $(USERADDR_O) $(SCHEDULER_O)

all: $(ISO_IMAGE)

INIT_BIN = ../init/init

.PHONY: $(INIT_BIN)
$(INIT_BIN):
	@echo "  MAKE    init"
	@$(MAKE) -C ../init

.PHONY: $(YES_BIN)
$(YES_BIN):
	@echo "  MAKE    ports/yes"
	@$(MAKE) -C $(PORTS_DIR)

$(ISO_IMAGE): $(KERNEL_BIN) $(INIT_BIN) $(YES_BIN)
	@mkdir -p $(ISODIR)/boot/grub
	@cp $(KERNEL_BIN) $(ISODIR)/boot/kernel.bin
	@cp $(INIT_BIN) $(ISODIR)/boot/init
	@cp $(YES_BIN) $(ISODIR)/boot/yes
	@echo 'set timeout=4' > $(ISODIR)/boot/grub/grub.cfg
	@echo 'set default=0' >> $(ISODIR)/boot/grub/grub.cfg
	@echo 'insmod all_video' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '' >> $(ISODIR)/boot/grub/grub.cfg
	@echo 'menuentry "OTSOS" {' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    multiboot2 /boot/kernel.bin' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/init init' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    module2 /boot/yes yes' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '    boot' >> $(ISODIR)/boot/grub/grub.cfg
	@echo '}' >> $(ISODIR)/boot/grub/grub.cfg
	@echo "  ISO     $(ISO_IMAGE)"
	@grub-mkrescue -o $(ISO_IMAGE) $(ISODIR) >/dev/null 2>&1

$(KERNEL_BIN): $(OBJ)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(OBJ) -o $@

$(BOOT_O): boot/boot.s
	@echo "  AS      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(IDT_ASM_O): kernel/interrupts/idt.asm
	@echo "  AS      $<"
	@$(AS) -f elf64 -g -F dwarf $< -o $@

$(KERNEL_O): kernel/kernel.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(IDT_C_O): kernel/interrupts/idt.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PIC_O): kernel/interrupts/pic.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(HANDLERS_O): kernel/interrupts/handlers.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PANIC_O): kernel/panic.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(MEMORY_O): mlibc/memory.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PATA_O): kernel/drivers/disk/pata/pata.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CHAINFS_O): kernel/drivers/fs/chainFS/chainfs.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(VGA_O): kernel/drivers/vga.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TTY_O): kernel/drivers/tty.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PS2_O): kernel/drivers/keyboard/ps2.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KEYBOARD_O): kernel/drivers/keyboard/keyboard.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(FB_O): kernel/drivers/video/fb.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(FONT_O): kernel/drivers/video/font.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TIMER_O): kernel/drivers/timer.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(STDLIB_O): mlibc/stdlib.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(MMU_O): kernel/mmu.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(WRITE_O): kernel/posix/write.c kernel/drivers/tty.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OPEN_O): kernel/posix/open.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SYSCALL_O): kernel/syscall.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(READ_O): kernel/posix/read.c kernel/posix/posix.h kernel/drivers/tty.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CLOSE_O): kernel/posix/close.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(LSEEK_O): kernel/posix/lseek.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(WAIT_O): kernel/posix/wait.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(MMAP_O): kernel/posix/mmap.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(PIPE_O): kernel/posix/pipe.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(CLONE_O): kernel/posix/clone.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(GDT_O): kernel/gdt.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(GDT_ASM_O): kernel/gdt.asm
	@echo "  AS      $<"
	@$(AS) -f elf64 -g -F dwarf $< -o $@

$(PROCESS_O): kernel/process.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(FORK_O): kernel/posix/fork.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(EXEC_O): kernel/posix/exec.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(USERADDR_O): kernel/useraddr.c kernel/useraddr.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(SCHEDULER_O): kernel/scheduler.c kernel/scheduler.h kernel/process.h kernel/mmu.h kernel/gdt.h kernel/interrupts/idt.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(ELF_O): userland/elf.zig
	@echo "  ZIG     $<"
	@$(ZIG) build-obj -femit-bin=$@ -target x86_64-freestanding -mcmodel=kernel -O ReleaseSafe --dep panic -Melf=userland/elf.zig -Mpanic=kernel/panic.zig

$(USERSPACE_O): userland/userspace.c userland/userspace.h kernel/process.h kernel/mmu.h kernel/gdt.h
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(USERSPACE_ASM_O): userland/userspace.asm
	@echo "  AS      $<"
	@$(AS) -f elf64 -g -F dwarf $< -o $@

$(SYSCALL_ASM_O): kernel/syscall_asm.asm
	@echo "  AS      $<"
	@$(AS) -f elf64 -g -F dwarf $< -o $@


$(STRING_O): mlibc/string.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(ITOA_O): mlibc/itoa.s
	@echo "  AS      $<"
	@$(CC) -c $< -o $@

$(HARDWARE_O): mlibc/hardware.asm
	@echo "  AS      $<"
	@$(AS) -f elf64 -g -F dwarf $< -o $@ -D__x86_64__




$(DISK_O): kernel/drivers/disk/disk.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(RAMDISK_O): kernel/drivers/disk/ramdisk/ramdisk.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@


$(COM1_O): lib/com1.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(DISK_IMG):
	@echo "  IMG     $(DISK_IMG)"
	@mkdir -p $(dir $(DISK_IMG))
	@dd if=/dev/zero of=$(DISK_IMG) bs=1M count=10 2>/dev/null

OVMF = /usr/share/ovmf/OVMF.fd

run: $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio

run_uefi: $(ISO_IMAGE) $(DISK_IMG)
	@echo "  RUN     QEMU (UEFI)"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -bios $(OVMF) $(QEMU_GUI_OPTS)

debug: $(ISO_IMAGE) $(DISK_IMG)
	@echo "  DEBUG   QEMU"
	@$(QEMU) -cdrom $(ISO_IMAGE) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -d int,cpu_reset > debug.txt 2>&1 -no-reboot $(QEMU_GUI_OPTS)

clean:
	@echo "  CLEAN"
	@rm -f ../bin/*.o ../bin/kernel.bin ../bin/kernel.map $(ISO_IMAGE) $(DISK_IMG)
	@rm -rf $(ISODIR)/boot/kernel.bin
