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
$define %type bootpack_t as tar-backed boot payload
$define %type bootpack_file_t as file entry inside boot payload
$define %type mb2_builder_t as mutable multiboot2 info builder
$define %type mb2_mmap_entry_t as multiboot2 memory map entry
$define %type module_ctx_t as module loading context
$define %type cfsr_volume_t as mounted read-only ChainFS volume
$define %type cfsr_entry_t as ChainFS file table entry
$define %type gptr_part_t as located GPT partition range

$define %func align_up as function with args u32, u32
$define %func add_memory_map as function with args mb2_builder_t *
$define %func load_module as function with args const bootpack_file_t *, const char *, module_ctx_t *
$define %func module_cb as function with args const bootpack_file_t *, void *
$define %func panic as procedure with args const char *
$define %func cfs_disk_read as function with args void *, u64, u32, void *
$define %func cfs_memory_limit as function with args void
$define %func cfs_alloc as function with args u32
$define %func cfs_mount_root as function with args u32
$define %func cfs_load_kernel as function with args u64 *, u64 *
$define %func cfs_load_cmseed as function with args mb2_builder_t *
$define %func live_load_kernel as function with args u64 *
$define %func try_installed as function with args u32, u64 *, u64 *
$define %func bios_main as start with args void

*/

/* !SPACE!

$space %internal add_memory_map, load_module, module_cb, panic
$space %internal cfs_disk_read, cfs_memory_limit, cfs_alloc
$space %internal cfs_mount_root, cfs_load_kernel, cfs_load_cmseed
$space %internal live_load_kernel, try_installed
$space %export bios_main

*/

#include <boot/bootloader/bios/bios.h>
#include <boot/bootloader/lib/bootpack.h>
#include <boot/bootloader/lib/cfsread.h>
#include <boot/bootloader/lib/elf64.h>
#include <boot/bootloader/lib/gptread.h>
#include <boot/bootloader/lib/multiboot2.h>
#include <boot/bootloader/lib/string.h>

#define BOOTPACK_LOAD_ADDR	0x02000000U
#define MB2_INFO_ADDR		0x00800000U
#define MB2_INFO_CAP		0x00010000U
#define CFS_LOAD_BASE		0x02000000U
#define CFS_LOAD_LIMIT		0x40000000U
#define CFS_MAX_RUN		64U
#define CFS_MODULE_ALIGN	4096U
#define CFS_KERNEL_PATH		"/system/init/kernel.bin"
#define CFS_CMSEED_PATH		"/system/init/cmseed"
#define CFS_CMSEED_NAME		"cmseed"
#define BOOTPACK_KERNEL_NAME	"kernel"

typedef struct {
	mb2_builder_t	*mb;
	int		failed;
} module_ctx_t;

static cfsr_volume_t	cfs_vol;
static bootpack_t	live_pack;
static u32		cfs_load_ptr;
static u32		cfs_load_end;
static u32		cfs_drive;

static void
panic(const char *msg)
{
	bios_console_puts("\n[BIOS] panic: ");
	bios_console_puts(msg);
	bios_console_puts("\n");
	bios_halt();
}


static int
cfs_disk_read(void *ctx, u64 lba, u32 count, void *dst)
{

	if ((lba >> 32) != 0 || count == 0) {
		return (-1);
	}
	if (ctx != NULL) {
		bios_disk_select(*(const u32 *)ctx);
	}
	return (bios_disk_read((u32)lba, count, dst));
}


static u32
cfs_memory_limit(void)
{
	const bios_mmap_entry_t	*e;
	u64			end;
	u32			i, limit;

	limit = 0;
	for (i = 0; i < bios_mmap_count && i < BIOS_E820_MAX; i++) {
		e = &bios_mmap_entries[i];
		if (e->type != 1 || e->length == 0) {
			continue;
		}
		if (e->base_addr > CFS_LOAD_BASE) {
			continue;
		}
		end = e->base_addr + e->length;
		if (end <= (u64)CFS_LOAD_BASE) {
			continue;
		}
		if (end > (u64)CFS_LOAD_LIMIT) {
			limit = CFS_LOAD_LIMIT;
		} else {
			limit = (u32)end;
		}
		break;
	}
	if (limit == 0) {
		if (bios_boot_info.mem_upper_kb > (CFS_LOAD_BASE / 1024U)) {
			limit = 0x00100000U +
			    bios_boot_info.mem_upper_kb * 1024U;
			if (limit > CFS_LOAD_LIMIT) {
				limit = CFS_LOAD_LIMIT;
			}
		}
	}
	return (limit);
}

static u32
cfs_alloc(u32 size)
{
	u32	addr, next;

	if (size == 0 || cfs_load_ptr < CFS_LOAD_BASE) {
		return (0);
	}
	addr = cfs_load_ptr;
	if (size > cfs_load_end - addr) {
		return (0);
	}
	next = addr + size;
	if (next > cfs_load_end - CFS_MODULE_ALIGN) {
		next = cfs_load_end;
	} else {
		next = (next + CFS_MODULE_ALIGN - 1U) & ~(CFS_MODULE_ALIGN - 1U);
	}
	cfs_load_ptr = next;
	return (addr);
}

static int
add_memory_map(mb2_builder_t *mb)
{
	mb2_mmap_entry_t	entries[BIOS_E820_MAX];
	const bios_mmap_entry_t	*src;
	u32			count, i, out, type;

	count = bios_mmap_count;
	if (count > BIOS_E820_MAX) {
		count = BIOS_E820_MAX;
	}
	out = 0;
	for (i = 0; i < count; i++) {
		src = &bios_mmap_entries[i];
		if (src->length == 0) {
			continue;
		}
		type = src->type;
		if (type == 0 || type > 5) {
			type = 2;
		}
		entries[out].base_addr = src->base_addr;
		entries[out].length = src->length;
		entries[out].type = type;
		entries[out].reserved = 0;
		out++;
	}
	if (out == 0) {
		return (mb2_add_simple_mmap(mb, bios_boot_info.mem_lower_kb,
		    bios_boot_info.mem_upper_kb));
	}
	return (mb2_add_mmap_entries(mb, entries, out));
}

static int
load_module(const bootpack_file_t *file, const char *name, module_ctx_t *ctx)
{
	u32	start, end;

	if (file->size == 0) {
		return (0);
	}
	start = (u32)file->data;
	end = start + file->size;
	if (mb2_add_module(ctx->mb, start, end, name) != 0) {
		ctx->failed = 1;
		return (-1);
	}
	bios_console_puts("[BIOS] module ");
	bios_console_puts(name);
	bios_console_puts(" ");
	bios_console_puthex(file->size);
	bios_console_puts(" bytes\n");
	return (0);
}


static int
cfs_mount_root(u32 drive)
{
	gptr_part_t	part;

	cfs_drive = drive;
	if (gptr_find(cfs_disk_read, &cfs_drive, gptr_type_otsos, &part) != 0) {
		return (-1);
	}
	if (cfsr_mount(&cfs_vol, cfs_disk_read, &cfs_drive, part.first_lba,
	    CFS_MAX_RUN) != 0) {
		return (-1);
	}
	bios_console_puts("[BIOS] chainfs root on drive ");
	bios_console_puthex(drive);
	bios_console_puts(" lba ");
	bios_console_puthex((u32)part.first_lba);
	bios_console_puts("\n");
	return (0);
}

static int
cfs_load_kernel(u64 *entry, u64 *kernel_end)
{
	cfsr_entry_t	file;
	u32		addr, got;

	if (cfsr_lookup(&cfs_vol, CFS_KERNEL_PATH, &file) != 0) {
		return (-1);
	}
	if (file.type != CFSR_TYPE_FILE || file.size == 0) {
		return (-1);
	}
	addr = cfs_alloc(file.size);
	if (addr == 0) {
		return (-1);
	}
	if (cfsr_read(&cfs_vol, &file, (void *)addr, file.size, &got) != 0 ||
	    got != file.size) {
		return (-1);
	}
	return (elf64_load_kernel((const void *)addr, file.size, entry,
	    kernel_end));
}


static int
cfs_load_cmseed(mb2_builder_t *mb)
{
	cfsr_entry_t	file;
	u32		addr, got;

	if (cfsr_lookup(&cfs_vol, CFS_CMSEED_PATH, &file) != 0) {
		return (-1);
	}
	if (file.type != CFSR_TYPE_FILE || file.size == 0) {
		return (-1);
	}
	addr = cfs_alloc(file.size);
	if (addr == 0) {
		return (-1);
	}
	if (cfsr_read(&cfs_vol, &file, (void *)addr, file.size, &got) != 0 ||
	    got != file.size) {
		return (-1);
	}
	if (mb2_add_module(mb, addr, addr + file.size,
	    CFS_CMSEED_NAME) != 0) {
		return (-1);
	}
	bios_console_puts("[BIOS] module cmseed ");
	bios_console_puthex(file.size);
	bios_console_puts(" bytes\n");
	return (0);
}


static int
live_load_kernel(u64 *entry)
{
	bios_layout_t	*layout;
	bootpack_file_t	kernel;
	u64		kernel_end;
	u32		pack_sectors;

	bios_disk_select(bios_boot_info.boot_drive);
	layout = (bios_layout_t *)0x00072000U;
	if (bios_disk_read(BIOS_LAYOUT_LBA, 1, layout) != 0) {
		return (-1);
	}
	if (layout->magic != BIOS_LAYOUT_MAGIC ||
	    layout->bootpack_sectors == 0) {
		return (-1);
	}

	pack_sectors = layout->bootpack_sectors;
	bios_console_puts("[BIOS] reading bootpack sectors=");
	bios_console_puthex(pack_sectors);
	bios_console_puts("\n");
	if (bios_disk_read(BIOS_BOOTPACK_LBA, pack_sectors,
	    (void *)BOOTPACK_LOAD_ADDR) != 0) {
		return (-1);
	}
	bootpack_init(&live_pack, (const void *)BOOTPACK_LOAD_ADDR,
	    pack_sectors * BIOS_SECTOR_SIZE);

	if (bootpack_find(&live_pack, BOOTPACK_KERNEL_NAME, &kernel) != 0) {
		return (-1);
	}
	return (elf64_load_kernel(kernel.data, kernel.size, entry,
	    &kernel_end));
}

static int
try_installed(u32 drive, u64 *entry, u64 *kernel_end)
{
	if (cfs_mount_root(drive) != 0) {
		return (-1);
	}
	cfs_load_ptr = CFS_LOAD_BASE;
	if (cfs_load_kernel(entry, kernel_end) != 0) {
		return (-1);
	}
	cfs_load_ptr = CFS_LOAD_BASE;
	if (*kernel_end > (u64)CFS_LOAD_BASE) {
		if (*kernel_end >= (u64)cfs_load_end) {
			panic("kernel image fills staging area");
		}
		cfs_load_ptr = ((u32)*kernel_end + CFS_MODULE_ALIGN - 1U) &
		    ~(CFS_MODULE_ALIGN - 1U);
	}
	return (0);
}


static int
module_cb(const bootpack_file_t *file, void *arg)
{
	module_ctx_t	*ctx;

	ctx = (module_ctx_t *)arg;
	return (load_module(file, file->name, ctx));
}

void
bios_main(void)
{
	mb2_framebuffer_t	fb;
	mb2_builder_t		mb;
	module_ctx_t		mod_ctx;
	u64			entry, kernel_end;
	u32			mb2_size;
	u32			boot_drive;
	u32			drive;
	int			installed;

	bios_console_init();
	bios_console_puts("[BIOS] OTSOS BIOS loader\n");

	entry = 0;
	kernel_end = 0;
	cfs_load_end = cfs_memory_limit();
	cfs_load_ptr = CFS_LOAD_BASE;
	installed = 0;
	boot_drive = bios_boot_info.boot_drive;

	if (cfs_load_end <= CFS_LOAD_BASE) {
		panic("no usable memory for staging");
	}


	if (try_installed(boot_drive, &entry, &kernel_end) == 0) {
		installed = 1;
	} else if (live_load_kernel(&entry) == 0) {
		bios_console_puts("[BIOS] live boot from drive ");
		bios_console_puthex(boot_drive);
		bios_console_puts("\n");
	} else {
		for (drive = BIOS_DRIVE_FIRST; drive <= BIOS_DRIVE_LAST;
		    drive++) {
			if (drive == boot_drive) {
				continue;
			}
			if (bios_disk_present(drive) != 0) {
				continue;
			}
			if (try_installed(drive, &entry, &kernel_end) == 0) {
				installed = 1;
				break;
			}
		}
		if (!installed) {
			panic("Kernel not found");
		}
	}
	bios_console_puts("[BIOS] kernel entry ");
	bios_console_puthex((u32)entry);
	bios_console_puts("\n");

	mb2_builder_init(&mb, (void *)MB2_INFO_ADDR, MB2_INFO_CAP);
	if (mb2_add_bootloader_name(&mb, "OTSOS BIOS bootloader") != 0) {
		panic("mb2 bootloader tag failed");
	}
	if (mb2_add_basic_meminfo(&mb, bios_boot_info.mem_lower_kb,
	    bios_boot_info.mem_upper_kb) != 0) {
		panic("mb2 meminfo tag failed");
	}
	if (add_memory_map(&mb) != 0) {
		panic("mb2 mmap tag failed");
	}
	fb.addr = bios_boot_info.fb_addr;
	fb.pitch = bios_boot_info.fb_pitch;
	fb.width = bios_boot_info.fb_width;
	fb.height = bios_boot_info.fb_height;
	fb.bpp = bios_boot_info.fb_bpp;
	fb.type = bios_boot_info.fb_type;
	if (mb2_add_framebuffer(&mb, &fb) != 0) {
		panic("mb2 framebuffer tag failed");
	}

	if (installed) {
		if (cfs_load_cmseed(&mb) != 0) {
			panic("cmseed missing on installed root");
		}
	} else {
		mod_ctx.mb = &mb;
		mod_ctx.failed = 0;
		if (bootpack_foreach(&live_pack, module_cb, &mod_ctx) != 0 ||
		    mod_ctx.failed) {
			panic("module load failed");
		}
	}

	mb2_size = mb2_builder_finish(&mb);
	if (mb2_size == 0) {
		panic("mb2 finish failed");
	}

	bios_console_puts("[BIOS] jump kernel mb2=");
	bios_console_puthex(MB2_INFO_ADDR);
	bios_console_puts("\n");
	bios_jump_kernel((u32)entry, MB2_BOOTLOADER_MAGIC, MB2_INFO_ADDR);
}
