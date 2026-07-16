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

$define %func align_up as function with args u32, u32
$define %func add_memory_map as function with args mb2_builder_t *
$define %func load_module as function with args const bootpack_file_t *, const char *, module_ctx_t *
$define %func module_cb as function with args const bootpack_file_t *, void *
$define %func panic as procedure with args const char *
$define %func bios_main as start with args void

*/

/* !SPACE!

$space %internal align_up, add_memory_map, load_module, module_cb, panic
$space %export bios_main

*/

#include <boot/bootloader/bios/bios.h>
#include <boot/bootloader/lib/bootpack.h>
#include <boot/bootloader/lib/elf64.h>
#include <boot/bootloader/lib/multiboot2.h>
#include <boot/bootloader/lib/string.h>

#define BOOTPACK_LOAD_ADDR	0x02000000U
#define MODULE_LOAD_ADDR	0x01000000U
#define MB2_INFO_ADDR		0x00800000U
#define MB2_INFO_CAP		0x00010000U

typedef struct {
	mb2_builder_t	*mb;
	u32		next;
	int		failed;
} module_ctx_t;

static u32
align_up(u32 value, u32 align)
{
	return ((value + align - 1U) & ~(align - 1U));
}

static void
panic(const char *msg)
{
	bios_console_puts("\n[BIOS] panic: ");
	bios_console_puts(msg);
	bios_console_puts("\n");
	bios_halt();
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
	start = align_up(ctx->next, 4096);
	end = start + file->size;
	bl_memcpy((void *)start, file->data, file->size);
	if (mb2_add_module(ctx->mb, start, end, name) != 0) {
		ctx->failed = 1;
		return (-1);
	}
	ctx->next = end;
	bios_console_puts("[BIOS] module ");
	bios_console_puts(name);
	bios_console_puts(" ");
	bios_console_puthex(file->size);
	bios_console_puts(" bytes\n");
	return (0);
}

static int
module_cb(const bootpack_file_t *file, void *arg)
{
	module_ctx_t	*ctx;

	ctx = (module_ctx_t *)arg;
	if (bl_strcmp(file->name, "kernel.bin") == 0 ||
	    bl_strcmp(file->name, "config.toml") == 0) {
		return (0);
	}
	return (load_module(file, file->name, ctx));
}

void
bios_main(void)
{
	bios_layout_t		*layout;
	bootpack_t		pack;
	bootpack_file_t		kernel;
	bootpack_file_t		config;
	mb2_framebuffer_t	fb;
	mb2_builder_t		mb;
	module_ctx_t		mod_ctx;
	u64			entry, kernel_end;
	u32			pack_sectors;
	u32			mb2_size;

	bios_console_init();
	bios_console_puts("[BIOS] OTSOS BIOS loader\n");

	layout = (bios_layout_t *)0x00072000U;
	if (bios_disk_read(BIOS_LAYOUT_LBA, 1, layout) != 0) {
		panic("cannot read layout sector");
	}
	if (layout->magic != BIOS_LAYOUT_MAGIC ||
	    layout->bootpack_sectors == 0) {
		panic("bad layout sector");
	}

	pack_sectors = layout->bootpack_sectors;
	bios_console_puts("[BIOS] reading bootpack sectors=");
	bios_console_puthex(pack_sectors);
	bios_console_puts("\n");
	if (bios_disk_read(BIOS_BOOTPACK_LBA, pack_sectors,
	    (void *)BOOTPACK_LOAD_ADDR) != 0) {
		panic("cannot read bootpack");
	}
	bootpack_init(&pack, (const void *)BOOTPACK_LOAD_ADDR,
	    pack_sectors * BIOS_SECTOR_SIZE);

	if (bootpack_find(&pack, "kernel.bin", &kernel) != 0) {
		panic("kernel.bin not found");
	}
	if (elf64_load_kernel(kernel.data, kernel.size, &entry,
	    &kernel_end) != 0) {
		panic("kernel ELF load failed");
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

	mod_ctx.mb = &mb;
	mod_ctx.next = MODULE_LOAD_ADDR;
	mod_ctx.failed = 0;
	if (bootpack_find(&pack, "config.toml", &config) == 0) {
		if (load_module(&config, "config", &mod_ctx) != 0) {
			panic("config module failed");
		}
	} else {
		panic("config.toml not found");
	}
	if (bootpack_foreach(&pack, module_cb, &mod_ctx) != 0 ||
	    mod_ctx.failed) {
		panic("module load failed");
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
