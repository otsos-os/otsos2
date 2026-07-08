/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type disk_t as struct with name, type, sector_size, sectors, ops
$define %type disk_type_t as enum with disk types
$define %type multiboot_info_t as struct with multiboot1 info
$define %type multiboot2_info_t as struct with multiboot2 info
$define %type multiboot2_tag_t as struct with multiboot2 tag header
$define %type multiboot2_tag_module_t as struct with module tag
$define %type module_copy_ctx_t as struct with multiboot pointers, boot magic, init module

$define %func debug_multiboot_info as procedure with args multiboot_info_t *
$define %func debug_multiboot2_tags as procedure with args multiboot2_info_t *
$define %func mb2_find_module as function with args multiboot2_info_t *, const char *, void **, u32 *
$define %func status_line as procedure with args const char *, int
$define %func disk_has_type as function with args disk_type_t
$define %func disk_find_type as function with args disk_type_t
$define %func timer_sanity_check as function with args void
$define %func enable_sse as procedure with args void
$define %func kernel_ensure_parent_dirs as function with args const char *
$define %func kernel_install_module_cb as procedure with args const char *, const char *, void *
$define %func kmain as start with args u64, u64, u64, u64

*/

/* !SPACE!

$space %internal debug_multiboot_info, debug_multiboot2_tags
$space %internal mb2_find_module, status_line
$space %internal disk_has_type, disk_find_type
$space %internal timer_sanity_check, enable_sse
$space %internal kernel_ensure_parent_dirs, kernel_install_module_cb
$space %export kmain

*/

#include <kernel/interrupts/apic/lapic.h>
#include <kernel/interrupts/apic/ioapic.h>
#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/disk/disk.h>
#ifdef CONFIG_DISK_PATA
#include <kernel/drivers/disk/pata/pata.h>
#endif
#include <kernel/drivers/disk/ramdisk/ramdisk.h>
#include <kernel/drivers/eventtimer.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/fs/devfs/devfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/power/power.h>
#include <kernel/drivers/timer.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/uart/uart.h>
#include <kernel/time.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/video/drm/init.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio-gpu.h>
#include <kernel/drivers/watchdog/watchdog.h>
#include <kernel/event/event.h>
#include <kernel/interrupts/idt.h>
#include <kernel/console/console.h>
#include <mm/vm/pmap.h>
#include <mm/mm.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <kernel/panic.h>
#include <kernel/pci/pci.h>
#include <kernel/api/api.h>
#include <kernel/bootmem.h>
#include <kernel/crypto/crypto.h>
#include <kernel/kshell/kshell.h>
#include <kernel/other/kusr.h>
#include <kernel/other/config.h>
#include <kernel/syscall.h>
#include <kernel/smp/smp.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mlibc/stdlib.h>
#include <userland/userspace.h>

extern void	cpuid_get(u32 code, u32 *res);
extern void	cinfo(char *buf);
extern u64	rinfo(u64 mb_ptr);
extern void	pit_init(void);
extern void	apic_timer_init(void);
extern char	start;
extern char	kernel_end;

static u32	boot_magic;
static int	is_multiboot2;

#define BOOT_FLAG_DISABLE_APIC	0x00000001ULL

static void
debug_multiboot_info(multiboot_info_t *mb_info)
{
	printk("Flags: 0x%x\n", mb_info->flags);

	if (mb_info->flags & MULTIBOOT_FLAG_MEM) {
		printk("mem_lower: %u KB, mem_upper: %u KB\n",
		    mb_info->mem_lower, mb_info->mem_upper);
	}

	if (mb_info->flags & MULTIBOOT_FLAG_CMDLINE) {
		printk("cmdline: %s\n",
		    (const char *)(u64)mb_info->cmdline);
	}

	if (mb_info->flags & MULTIBOOT_FLAG_BOOTLOADER_NAME) {
		printk("bootloader: %s\n",
		    (const char *)(u64)mb_info->boot_loader_name);
	}

	if (mb_info->flags & MULTIBOOT_FLAG_FRAMEBUFFER) {
		printk("framebuffer: %ux%ux%u\n",
		    mb_info->framebuffer_width,
		    mb_info->framebuffer_height,
		    mb_info->framebuffer_bpp);
	}
}

static void
debug_multiboot2_tags(multiboot2_info_t *mb_info)
{
	multiboot2_tag_t	*tag;
	u64			next_addr;

	tag = (multiboot2_tag_t *)((u8 *)mb_info + 8);

	while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
		printk("Tag type: %u, size: %u", tag->type,
		    tag->size);

		switch (tag->type) {
		case MULTIBOOT2_TAG_TYPE_CMDLINE:
			printk(" (CMDLINE)");
			break;
		case MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME:
			printk(" (BOOT_LOADER_NAME)");
			break;
		case MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO:
			printk(" (BASIC_MEMINFO)");
			break;
		case MULTIBOOT2_TAG_TYPE_MMAP:
			printk(" (MMAP)");
			break;
		case MULTIBOOT2_TAG_TYPE_FRAMEBUFFER:
			printk(" (FRAMEBUFFER)");
			break;
		case MULTIBOOT2_TAG_TYPE_ELF_SECTIONS:
			printk(" (ELF_SECTIONS)");
			break;
		case MULTIBOOT2_TAG_TYPE_ACPI_OLD:
			printk(" (ACPI_OLD)");
			break;
		case MULTIBOOT2_TAG_TYPE_ACPI_NEW:
			printk(" (ACPI_NEW)");
			break;
		default:
			printk(" (other)");
			break;
		}
		printk("\n");

		next_addr = (u64)tag + tag->size;
		next_addr = (next_addr + 7) & ~7;
		tag = (multiboot2_tag_t *)next_addr;
	}
}

static int
mb2_find_module(multiboot2_info_t *mb_info, const char *name,
    void **out_start, u32 *out_size)
{
	multiboot2_tag_t		*tag;
	multiboot2_tag_module_t		*mod;
	u64				next_addr;

	tag = (multiboot2_tag_t *)((u8 *)mb_info + 8);

	while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
		if (tag->type == MULTIBOOT2_TAG_TYPE_MODULE) {
			mod = (multiboot2_tag_module_t *)tag;
			if (strcmp(mod->cmdline, name) == 0) {
				if (out_start) {
					*out_start = (void *)(u64)
					    mod->mod_start;
				}
				if (out_size) {
					*out_size = mod->mod_end -
					    mod->mod_start;
				}
				return (0);
			}
		}
		next_addr = (u64)tag + tag->size;
		next_addr = (next_addr + 7) & ~7;
		tag = (multiboot2_tag_t *)next_addr;
	}
	return (-1);
}

static void
status_line(const char *label, int ok)
{
	const int	pad_col = 32;
	int		len, i;

	len = strlen(label);
	printf("%s", label);
	printk("%s", label);
	for (i = len; i < pad_col; i++) {
		console_putchar(' ');
		printk(" ");
	}
	if (ok) {
		printf("\033[32m[OK]\033[0m\n");
	} else {
		printf("\033[31m[FAILED]\033[0m\n");
	}
}

static int
disk_has_type(disk_type_t type)
{
	int	count, i;
	disk_t	*disk;

	count = disk_count();
	for (i = 0; i < count; i++) {
		disk = disk_get(i);
		if (disk && disk->type == type) {
			return (1);
		}
	}
	return (0);
}

static disk_t *
disk_find_type(disk_type_t type)
{
	int	count, i;
	disk_t	*disk;

	count = disk_count();
	for (i = 0; i < count; i++) {
		disk = disk_get(i);
		if (disk && disk->type == type) {
			return (disk);
		}
	}
	return (NULL);
}

static int
timer_sanity_check(void)
{
	u64	start;
	u64	i;

	if (!timer_is_initialized()) {
		return (0);
	}
	start = timer_get_ticks();
	for (i = 0; i < 1000000 &&
	    timer_get_ticks() == start; i++) {
		__asm__ volatile("pause");
	}
	return (timer_get_ticks() != start);
}

static void
enable_sse(void)
{
	u64	cr0, cr4;

	__asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~(1ULL << 2);
	cr0 &= ~(1ULL << 3);
	cr0 |= (1ULL << 1);
	__asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

	__asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= (1ULL << 9);
	cr4 |= (1ULL << 10);
	__asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
}

typedef struct {
	multiboot2_info_t	*mboot2_ptr;
	multiboot_info_t	*mboot1_ptr;
	u32			boot_magic;
	void			*init_mod;
	u32			init_sz;
} module_copy_ctx_t;

static int
kernel_ensure_parent_dirs(const char *path)
{
	char	buf[256];
	int	len;
	int	i;

	if (!path || path[0] != '/') {
		return (-1);
	}

	len = strlen(path);
	if (len >= (int)sizeof(buf)) {
		return (-1);
	}
	memcpy(buf, path, len + 1);

	for (i = 1; i < len; i++) {
		if (buf[i] == '/') {
			buf[i] = '\0';
			vfs_mkdir(buf);
			buf[i] = '/';
		}
	}
	return (0);
}

static void
kernel_install_module_cb(const char *name, const char *dest, void *ctx)
{
	module_copy_ctx_t	*c;
	void			*mod;
	u32			sz;
	int			res;

	c = (module_copy_ctx_t *)ctx;
	if (!name || !dest || !c) {
		return;
	}

	mod = NULL;
	sz = 0;
	if (c->boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
		mb2_find_module(c->mboot2_ptr, name, &mod, &sz);
	} else {
		return;
	}

	if (!mod || sz == 0) {
		printk("[KERNEL] Module '%s' not found, skipping "
		    "copy to %s\n", name, dest);
		return;
	}

	if (kernel_ensure_parent_dirs(dest) != 0) {
		printk("[KERNEL] Failed to create parent directories "
		    "for %s\n", dest);
		return;
	}

	res = vfs_write_file(dest, (const u8 *)mod, sz);
	if (res == 0) {
		printk("[KERNEL] Installed %s from module '%s' "
		    "(%u bytes)\n", dest, name, sz);
	} else {
		printk("[KERNEL] Failed to install %s from module "
		    "'%s'\n", dest, name);
	}

	if (strcmp(name, "init") == 0) {
		c->init_mod = mod;
		c->init_sz = sz;
	}
}

void
kmain(u64 magic, u64 addr, u64 boot_option, u64 boot_flags)
{
	int		safe_mode, debug_mode, disable_apic;
	void		*ramdisk_mem;
	multiboot2_info_t	*mboot2_ptr;
	multiboot_info_t	*mboot1_ptr;
	char		cpu_buf[64];
	char		*p;
	u64		ram_kb;
	char		c;
	disk_t		*selected_disk, *ram_disk;
#ifdef CONFIG_DISK_PATA
	disk_t		*pata_disk;
#endif
	int		fs_ok, fmt_ok, heap_ok, idt_ok, timer_ok;
	int		pmap_ok, syscall_ok, disk_ok;
#ifdef CONFIG_DISK_PATA
	int		pata_ok;
#endif
	int		ramdisk_ok, fb_ok, drm_atomic_ok, acpi_ok;
	int		power_ok, pci_ok, watchdog_ok;
	u32		format_blocks;
	void		*config_mod;
	u32		config_sz;
	module_copy_ctx_t	mod_ctx;

	safe_mode = (boot_option == 1);
	debug_mode = (boot_option == 2);
	disable_apic = ((boot_flags & BOOT_FLAG_DISABLE_APIC) != 0);

	uart_init();
	bootmem_init(magic, addr, 0x100000,
    (u64)&kernel_end - KERNEL_VMA);
	kmem_init();
	ramdisk_mem = bootmem_alloc(4 * 1024 * 1024, PAGE_SIZE);

	if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
		mboot2_ptr = (multiboot2_info_t *)addr;
		mb2_find_module(mboot2_ptr, "config",
		    &config_mod, &config_sz);
		if (config_mod && config_sz > 0) {
			config_init_from_data(
			    (const char *)config_mod,
			    config_sz);
			stdio_init();
			printk("loaded config from "
			    "(%u bytes)\n", config_sz);
		}
	}

	init_idt();
	pit_init();
	pmap_init();
	vm_page_init_from_bootmem();
	if (disable_apic) {
		printk("[BOOT] APIC disabled by boot settings\n");
	} else {
		lapic_init();
	}
	timer_init(config_get_int("timer", "hz", 1000));
	time_init();
	et_clocksource_init();
	vm_object_init();
	uma_init();
	enable_sse();
	__asm__ volatile("sti");

	if (!disable_apic && strcmp(config_get_string("timer", "default_timer",
	    "apic"), "apic") == 0)
		apic_timer_init();

	syscall_init();
	event_init();
	crypto_rng_init();

	disk_manager_init();
#ifdef CONFIG_DISK_PATA
	pata_identify(NULL);
#endif

	if (ramdisk_mem) {
		ramdisk_init(ramdisk_mem, 4 * 1024 * 1024);
	} else {
		printk("[RAMDISK] bootmem allocation "
		    "failed\n");
	}

	boot_magic = (u32)magic;

	if (boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
		is_multiboot2 = 1;
		printk("boot from mb2 (magic: 0x%x)\n",
		    boot_magic);

		mboot2_ptr = (multiboot2_info_t *)addr;
		debug_multiboot2_tags(mboot2_ptr);

		if (config_is_initialized()) {
			printk("config geted\n");
		} else {
			printk("config not "
			    "found\n");
		}

		drm_boot_init_mb2(mboot2_ptr, 0);

		acpi_init_from_multiboot2(mboot2_ptr);
		if (!disable_apic) {
			ioapic_init();
			smp_init();
		} else {
			smp_init_single_cpu();
		}

		clear_scr();
		terminal_init();
		stdio_set_terminal_mirror(terminal_log_mirror);

		cinfo(cpu_buf);
		p = cpu_buf;
		while (*p == ' ') {
			p++;
		}

		ram_kb = multiboot2_get_ram_kb(mboot2_ptr);

		printk("CPU: %s\n", p);
		printk("RAM: %u MB (%u KB)\n",
		    ram_kb / 1024, ram_kb);

		if (drm_is_ready()) {
			printf("CPU: %s\n", p);
			printf("RAM: %u MB\n", ram_kb / 1024);
		}

	} else if (boot_magic == MULTIBOOT_BOOTLOADER_MAGIC) {
		is_multiboot2 = 0;
		printk("boot from mb1 (magic: 0x%x)\n",
		    boot_magic);

		mboot1_ptr = (multiboot_info_t *)addr;
		debug_multiboot_info(mboot1_ptr);
		drm_boot_init_mb1(mboot1_ptr, 0);
		clear_scr();
		terminal_init();
		stdio_set_terminal_mirror(terminal_log_mirror);

		cinfo(cpu_buf);
		p = cpu_buf;
		while (*p == ' ') {
			p++;
		}

		ram_kb = multiboot_get_ram_kb(mboot1_ptr);

		printk("CPU: %s\n", p);
		printk("RAM: %u MB (%u KB)\n",
		    ram_kb / 1024, ram_kb);

		if (drm_is_ready()) {
			printf("CPU: %s\n", p);
			printf("RAM: %u MB\n", ram_kb / 1024);
		}

	} else {
		panic("ERROR: Unknown bootloader magic: "
		    "0x%x\nExpected MB1: 0x%x or MB2: "
		    "0x%x\n",
		    boot_magic, MULTIBOOT_BOOTLOADER_MAGIC,
		    MULTIBOOT2_BOOTLOADER_MAGIC);
	}

	pci_set_verbose_scan(debug_mode);
	if (debug_mode) {
		printk("[BOOT] Debug mode: verbose PCI "
		    "scan enabled\n");
	}

	power_init();
	drm_virtio_gpu_pci_register();
	pci_init();
	watchdog_init();
	if (watchdog_device_count() > 0) {
		if (watchdog_start(WATCHDOG_DEFAULT_TIMEOUT_SEC)
		    != 0) {
			panic("[WDT] failed to start watchdog\n");
		}
	}
	if (acpi_is_initialized()) {
		power_acpi_enable();
	}

	heap_ok = kmem_is_initialized() &&
	    kmem_free_bytes() > 0;
	idt_ok = idt_is_loaded();
	timer_ok = timer_sanity_check();
	pmap_ok = pmap_is_initialized() &&
	    pmap_get_cr3() != 0;
	syscall_ok = syscall_is_initialized();
	disk_ok = disk_manager_is_initialized();
#ifdef CONFIG_DISK_PATA
	pata_ok = disk_has_type(DISK_TYPE_PATA);
#endif
	ramdisk_ok = disk_has_type(DISK_TYPE_RAM);
	fb_ok = drm_is_ready() != 0;
	drm_atomic_ok = drm_is_ready();
	acpi_ok = acpi_is_initialized();
	power_ok = power_is_initialized();
	pci_ok = pci_is_initialized();
	watchdog_ok = watchdog_is_initialized() &&
	    watchdog_device_count() > 0;

	status_line("kmem heap", heap_ok);
	status_line("idt", idt_ok);
	status_line("timer", timer_ok);
	status_line("pmap", pmap_ok);
	status_line("syscall", syscall_ok);
	status_line("disk manager", disk_ok);
#ifdef CONFIG_DISK_PATA
	status_line("pata identify", pata_ok);
#endif
	status_line("ramdisk", ramdisk_ok);
	status_line("framebuffer", fb_ok);
	status_line("drm", drm_atomic_ok);
	status_line("acpi", acpi_ok);
	status_line("power", power_ok);
	status_line("pci scan", pci_ok);
	status_line("watchdog", watchdog_ok);

	sleep(430);

	keyboard_manager_init();
	kshell_set_boot_info(is_multiboot2);

	terminal_set_active(1);

	selected_disk = NULL;
	ram_disk = disk_find_type(DISK_TYPE_RAM);

#ifdef CONFIG_DISK_PATA
	pata_disk = disk_find_type(DISK_TYPE_PATA);

	if (pata_disk && ram_disk) {
		printf("\nSelect boot disk:\n");
		printf("1. Hard Drive (PATA)\n");
		printf("2. Live USB (RAM Disk)\n");

		while (1) {
			c = keyboard_getchar();
			if (c == '1') {
				selected_disk = pata_disk;
				printf("Selected PATA (%s)\n",
				    selected_disk->name);
				break;
			} else if (c == '2') {
				selected_disk = ram_disk;
				printf("Selected RAM Disk "
				    "(%s)\n",
				    selected_disk->name);
				break;
			}
		}
	} else if (pata_disk) {
		selected_disk = pata_disk;
		printf("Selected PATA (%s)\n",
		    selected_disk->name);
	} else
#endif
	{
		if (ram_disk) {
			selected_disk = ram_disk;
			printf("Selected RAM Disk (%s)\n",
			    selected_disk->name);
		}
	}

	if (!selected_disk) {
		panic("no boot disk\n");
	}

	fs_ok = (chainfs_init(selected_disk) == 0);
	status_line("chainfs init", fs_ok);
	if (!fs_ok) {
		printk("[CHAINFS] init failed, formatting "
		    "disk...\n");
		format_blocks = 64;
		if (selected_disk &&
		    selected_disk->total_sectors > 0) {
			format_blocks =
			    selected_disk->total_sectors;
		}
		fmt_ok = (chainfs_format(format_blocks, 128) == 0);
		status_line("chainfs format", fmt_ok);
		fs_ok = fmt_ok;
	}
	status_line("chainfs ready", fs_ok);

	if (fs_ok) {
		vfs_init();
		status_line("vfs", vfs_is_initialized());
		if (vfs_is_initialized() &&
		    config_is_initialized()) {
			vfs_mkdir("/conf");
			vfs_mkdir("/conf/boot");
			if (config_save_to_file(
			    CONFIG_PATH_BOOT) == 0) {
				printk("saved config to "
				    "to %s\n",
				    CONFIG_PATH_BOOT);
			} else {
				printk("fail save config "
				    "to %s\n",
				    CONFIG_PATH_BOOT);
			}
		}
	}
	if (!fs_ok) {
		printk("[CHAINFS] filesystem unavailable, "
		    "skipping userspace startup\n");
	}

	if (!safe_mode && fs_ok) {
		while (keyboard_getchar() != 0) {
			;
		}

		userspace_init();

		memset(&mod_ctx, 0, sizeof(mod_ctx));
		mod_ctx.mboot2_ptr = mboot2_ptr;
		mod_ctx.mboot1_ptr = mboot1_ptr;
		mod_ctx.boot_magic = boot_magic;

		api_init();

		if (config_is_initialized()) {
			config_foreach_in_section("modules",
			    kernel_install_module_cb, &mod_ctx);
		} else {
			printk("[KERNEL] No config loaded, "
			    "cannot install multiboot modules\n");
		}

		kusr_init();

		/*
		 * Hand the system console over to userspace: suspend all
		 * TTYs now so that init can explicitly wake the one it
		 * wants to use.  The boot menu / disk selection remains
		 * visible up to this point.
		 */
		terminal_power_suspend_all();

		if (mod_ctx.init_mod && mod_ctx.init_sz > 0) {
			printk("[KERNEL] Found init module "
			    "at %p, size %d. Starting init...\n",
			    mod_ctx.init_mod, mod_ctx.init_sz);
			userspace_load_init(mod_ctx.init_mod,
			    (u64)mod_ctx.init_sz);
		} else {
			printk("[KERNEL] Init module not "
			    "found! Falling back to kernel "
			    "loop...\n");
			while (1) {
				c = keyboard_getchar();
				if (c) {
					printf("\033[31m %c "
					    "\033[0m", c);
				}
			}
		}
	} else {
		if (!fs_ok) {
			printf("\n\033[31mChainFS unavailable. "
			    "Staying in kernel console.\033[0m\n");
		}
		if (safe_mode) {
			printf("\n\033[33m--- SAFE MOD ---"
			    "\033[0m\n");
			printf("init and userlang are disabled.\n");
			kshell_run();
		} else if (debug_mode) {
			printf("\n\033[36m--- DEBUG MODE ---"
			    "\033[0m\n");
			printf("userspace disabled because "
			    "filesystem is unavailable.\n");
			kshell_run();
		}

		while (1) {
			c = keyboard_getchar();
			if (c) {
				printf("%c", c);
			}
		}
	}
}
