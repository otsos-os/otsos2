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
 * and/or other materials provided with the distribution.
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

$define %type evr_boot_info_t as validated boot framebuffer description
$define %type evr_state_t as early video renderer state
$define %type device_t as pointer to newbus device
$define %type driver_t as newbus driver descriptor
$define %type devclass_t as newbus device class descriptor
$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed

$define %func evr_boot_from_newbus as function with args evr_boot_info_t *
$define %func evr_validate_boot as function with args const evr_boot_info_t *
$define %func evr_writer_enter as function with args void
$define %func evr_writer_leave as procedure with args void
$define %func evr_store_pixel as procedure with args u32, u32, u32
$define %func evr_fill_rect_locked as procedure with args u32, u32, u32,
u32, u32
$define %func evr_clear_locked as procedure with args void
$define %func evr_draw_glyph_locked as procedure with args char, u32, u32
$define %func evr_scroll_locked as procedure with args void
$define %func evr_linefeed_locked as procedure with args void
$define %func evr_put_printable_locked as procedure with args char
$define %func evr_putc_locked as procedure with args char
$define %func evr_start as function with args const evr_boot_info_t *
$define %func evr_identify as procedure with args driver_t *, device_t
$define %func evr_probe as function with args device_t
$define %func evr_attach as function with args device_t
$define %func evr_shutdown as procedure with args device_t
$define %func evr_is_ready as function with args void
$define %func evr_is_active as function with args void
$define %func evr_get_width as function with args void
$define %func evr_get_height as function with args void
$define %func evr_set_colors as procedure with args u32, u32
$define %func evr_put_entry_at as procedure with args char, u32, u32
$define %func evr_putc as procedure with args char
$define %func evr_write as procedure with args const char *
$define %func evr_clear as procedure with args void
$define %func evr_handoff as procedure with args void

*/

/* !SPACE!

$space %internal evr_boot_from_newbus, evr_validate_boot
$space %internal evr_writer_enter, evr_writer_leave, evr_store_pixel
$space %internal evr_fill_rect_locked, evr_clear_locked
$space %internal evr_draw_glyph_locked, evr_scroll_locked
$space %internal evr_linefeed_locked, evr_put_printable_locked
$space %internal evr_putc_locked, evr_start
$space %internal evr_identify, evr_probe, evr_attach, evr_shutdown
$space %export evr_is_ready, evr_is_active
$space %export evr_get_width, evr_get_height
$space %export evr_set_colors, evr_put_entry_at
$space %export evr_putc, evr_write, evr_clear, evr_handoff

*/

#include <evr/evr.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <mm/vm/pmap.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	EVR_FONT_WIDTH		8
#define	EVR_FONT_HEIGHT		16
#define	EVR_TAB_WIDTH		8
#define	EVR_FRAMEBUFFER_RGB	1

typedef struct evr_boot_info {
	u64	address;
	u32	pitch;
	u32	width;
	u32	height;
	u8	bpp;
	u8	type;
} evr_boot_info_t;

typedef struct evr_state {
	volatile u8	*base;
	u64		 size;
	u32		 pitch;
	u32		 width;
	u32		 height;
	u32		 cols;
	u32		 rows;
	u32		 cursor_x;
	u32		 cursor_y;
	u32		 fg;
	u32		 bg;
	volatile u32	 writers;
	volatile u32	 busy;
	volatile int	 ready;
	volatile int	 active;
	u8		 bpp;
} evr_state_t;

extern const u8	*get_font_data(char c);

static evr_state_t	evr_state;

static int	evr_validate_boot(const evr_boot_info_t *info);

static int
evr_boot_from_newbus(evr_boot_info_t *info)
{
	const newbus_bootinfo_t		*boot;
	multiboot2_tag_framebuffer_t	*fb2;
	multiboot_info_t		*mb1;

	if (info == NULL) {
		return (-1);
	}
	memset(info, 0, sizeof(*info));
	boot = newbus_get_bootinfo();
	if (boot == NULL) {
		return (-1);
	}
	if (boot->magic == MULTIBOOT2_BOOTLOADER_MAGIC &&
	    boot->mb2 != NULL) {
		fb2 = (multiboot2_tag_framebuffer_t *)multiboot2_find_tag(
		    (multiboot2_info_t *)boot->mb2,
		    MULTIBOOT2_TAG_TYPE_FRAMEBUFFER);
		if (fb2 == NULL) {
			return (-1);
		}
		info->address = fb2->framebuffer_addr;
		info->pitch = fb2->framebuffer_pitch;
		info->width = fb2->framebuffer_width;
		info->height = fb2->framebuffer_height;
		info->bpp = fb2->framebuffer_bpp;
		info->type = fb2->framebuffer_type;
		return (evr_validate_boot(info));
	}
	if (boot->magic != MULTIBOOT_BOOTLOADER_MAGIC ||
	    boot->mb1 == NULL) {
		return (-1);
	}
	mb1 = (multiboot_info_t *)boot->mb1;
	if ((mb1->flags & MULTIBOOT_FLAG_FRAMEBUFFER) == 0) {
		return (-1);
	}
	info->address = mb1->framebuffer_addr;
	info->pitch = mb1->framebuffer_pitch;
	info->width = mb1->framebuffer_width;
	info->height = mb1->framebuffer_height;
	info->bpp = mb1->framebuffer_bpp;
	info->type = mb1->framebuffer_type;
	return (evr_validate_boot(info));
}

static int
evr_validate_boot(const evr_boot_info_t *info)
{
	u64	minimum_pitch;
	u64	size;
	u32	bytes_per_pixel;

	if (info == NULL || info->address == 0 || info->pitch == 0 ||
	    info->width < EVR_FONT_WIDTH ||
	    info->height < EVR_FONT_HEIGHT ||
	    info->type != EVR_FRAMEBUFFER_RGB) {
		return (-1);
	}
	if (info->bpp != 16 && info->bpp != 24 && info->bpp != 32) {
		return (-1);
	}
	bytes_per_pixel = info->bpp / 8;
	minimum_pitch = (u64)info->width * bytes_per_pixel;
	if ((u64)info->pitch < minimum_pitch ||
	    (u64)info->height > ~0ULL / info->pitch) {
		return (-1);
	}
	size = (u64)info->pitch * info->height;
	if (info->address > ~0ULL - size) {
		return (-1);
	}
	return (0);
}

static int
evr_writer_enter(void)
{
	if (__atomic_load_n(&evr_state.active, __ATOMIC_ACQUIRE) == 0) {
		return (0);
	}
	__atomic_add_fetch(&evr_state.writers, 1, __ATOMIC_ACQUIRE);
	if (__atomic_load_n(&evr_state.active, __ATOMIC_ACQUIRE) == 0) {
		__atomic_sub_fetch(&evr_state.writers, 1, __ATOMIC_RELEASE);
		return (0);
	}
	if (__atomic_exchange_n(&evr_state.busy, 1,
	    __ATOMIC_ACQUIRE) != 0) {
		__atomic_sub_fetch(&evr_state.writers, 1, __ATOMIC_RELEASE);
		return (0);
	}
	if (__atomic_load_n(&evr_state.active, __ATOMIC_ACQUIRE) == 0) {
		__atomic_store_n(&evr_state.busy, 0, __ATOMIC_RELEASE);
		__atomic_sub_fetch(&evr_state.writers, 1, __ATOMIC_RELEASE);
		return (0);
	}
	return (1);
}

static void
evr_writer_leave(void)
{
	__atomic_store_n(&evr_state.busy, 0, __ATOMIC_RELEASE);
	__atomic_sub_fetch(&evr_state.writers, 1, __ATOMIC_RELEASE);
}

static void
evr_store_pixel(u32 x, u32 y, u32 color)
{
	volatile u8	*pixel;
	u64		 offset;
	u16		 rgb565;

	offset = (u64)y * evr_state.pitch +
	    (u64)x * (evr_state.bpp / 8);
	pixel = evr_state.base + offset;
	if (evr_state.bpp == 32) {
		*(volatile u32 *)pixel = color & 0x00FFFFFF;
	} else if (evr_state.bpp == 24) {
		pixel[0] = (u8)(color & 0xFF);
		pixel[1] = (u8)((color >> 8) & 0xFF);
		pixel[2] = (u8)((color >> 16) & 0xFF);
	} else {
		rgb565 = (u16)(((color >> 19) & 0x1F) << 11);
		rgb565 |= (u16)(((color >> 10) & 0x3F) << 5);
		rgb565 |= (u16)((color >> 3) & 0x1F);
		*(volatile u16 *)pixel = rgb565;
	}
}

static void
evr_fill_rect_locked(u32 x, u32 y, u32 width, u32 height, u32 color)
{
	u32	x_end, y_end;
	u32	px, py;

	if (x >= evr_state.width || y >= evr_state.height ||
	    width == 0 || height == 0) {
		return;
	}
	x_end = width > evr_state.width - x ? evr_state.width : x + width;
	y_end = height > evr_state.height - y ? evr_state.height : y + height;
	for (py = y; py < y_end; py++) {
		for (px = x; px < x_end; px++) {
			evr_store_pixel(px, py, color);
		}
	}
}

static void
evr_clear_locked(void)
{
	evr_fill_rect_locked(0, 0, evr_state.width, evr_state.height,
	    evr_state.bg);
	evr_state.cursor_x = 0;
	evr_state.cursor_y = 0;
}

static void
evr_draw_glyph_locked(char c, u32 cell_x, u32 cell_y)
{
	const u8	*glyph;
	u32		 pixel_x, pixel_y;
	u32		 color;
	u8		 bits;
	int		 row, col;

	if (cell_x >= evr_state.cols || cell_y >= evr_state.rows) {
		return;
	}
	glyph = get_font_data(c);
	if (glyph == NULL) {
		glyph = get_font_data('?');
	}
	if (glyph == NULL) {
		return;
	}
	pixel_x = cell_x * EVR_FONT_WIDTH;
	pixel_y = cell_y * EVR_FONT_HEIGHT;
	for (row = 0; row < EVR_FONT_HEIGHT; row++) {
		bits = glyph[row];
		for (col = 0; col < EVR_FONT_WIDTH; col++) {
			color = (bits & (1U << (7 - col))) != 0 ?
			    evr_state.fg : evr_state.bg;
			evr_store_pixel(pixel_x + (u32)col,
			    pixel_y + (u32)row, color);
		}
	}
}

static void
evr_scroll_locked(void)
{
	u32		 move_height;
	u64		 move_bytes;

	if (evr_state.height <= EVR_FONT_HEIGHT) {
		evr_clear_locked();
		return;
	}
	move_height = evr_state.height - EVR_FONT_HEIGHT;
	move_bytes = (u64)move_height * evr_state.pitch;
	memcpy((void *)evr_state.base,
	    (const void *)(evr_state.base +
	    (u64)EVR_FONT_HEIGHT * evr_state.pitch),
	    (unsigned long)move_bytes);
	evr_fill_rect_locked(0, move_height, evr_state.width,
	    EVR_FONT_HEIGHT, evr_state.bg);
}

static void
evr_linefeed_locked(void)
{
	evr_state.cursor_x = 0;
	evr_state.cursor_y++;
	if (evr_state.cursor_y >= evr_state.rows) {
		evr_scroll_locked();
		evr_state.cursor_y = evr_state.rows - 1;
	}
}

static void
evr_put_printable_locked(char c)
{
	evr_draw_glyph_locked(c, evr_state.cursor_x, evr_state.cursor_y);
	evr_state.cursor_x++;
	if (evr_state.cursor_x >= evr_state.cols) {
		evr_linefeed_locked();
	}
}

static void
evr_putc_locked(char c)
{
	u32	next_tab;

	if (c == '\n') {
		evr_linefeed_locked();
		return;
	}
	if (c == '\r') {
		evr_state.cursor_x = 0;
		return;
	}
	if (c == '\b') {
		if (evr_state.cursor_x > 0) {
			evr_state.cursor_x--;
		} else if (evr_state.cursor_y > 0) {
			evr_state.cursor_y--;
			evr_state.cursor_x = evr_state.cols - 1;
		}
		evr_draw_glyph_locked(' ', evr_state.cursor_x,
		    evr_state.cursor_y);
		return;
	}
	if (c == '\t') {
		next_tab = (evr_state.cursor_x + EVR_TAB_WIDTH) &
		    ~(EVR_TAB_WIDTH - 1);
		do {
			evr_put_printable_locked(' ');
		} while (evr_state.cursor_x != 0 &&
		    evr_state.cursor_x < next_tab);
		return;
	}
	if ((u8)c < 32 || (u8)c == 127) {
		return;
	}
	evr_put_printable_locked(c);
}

static int
evr_start(const evr_boot_info_t *info)
{
	void	*mapped;
	u64	 size;

	if (evr_validate_boot(info) != 0) {
		return (-1);
	}
	size = (u64)info->pitch * info->height;
	mapped = pmap_map_mmio(info->address, size);
	if (mapped == NULL) {
		return (-1);
	}
	memset(&evr_state, 0, sizeof(evr_state));
	evr_state.base = (volatile u8 *)mapped;
	evr_state.size = size;
	evr_state.pitch = info->pitch;
	evr_state.width = info->width;
	evr_state.height = info->height;
	evr_state.cols = info->width / EVR_FONT_WIDTH;
	evr_state.rows = info->height / EVR_FONT_HEIGHT;
	evr_state.fg = 0xAAAAAA;
	evr_state.bg = 0x000000;
	evr_state.bpp = info->bpp;
	evr_clear_locked();
	__atomic_store_n(&evr_state.ready, 1, __ATOMIC_RELEASE);
	__atomic_store_n(&evr_state.active, 1, __ATOMIC_RELEASE);
	return (0);
}

static void
evr_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "evr", 0) == NULL) {
		device_add_child(parent, "evr", 0);
	}
}

static int
evr_probe(device_t dev)
{
	evr_boot_info_t	info;

	(void)dev;
	return (evr_boot_from_newbus(&info));
}

static int
evr_attach(device_t dev)
{
	evr_boot_info_t	info;

	(void)dev;
	if (evr_boot_from_newbus(&info) != 0 || evr_start(&info) != 0) {
		return (-1);
	}
	drivers_log("[EVR] active: %ux%ux%u, pitch=%u\n",
	    info.width, info.height, (u32)info.bpp, info.pitch);
	return (0);
}

static void
evr_shutdown(device_t dev)
{
	(void)dev;
	evr_handoff();
}

int
evr_is_ready(void)
{
	return (__atomic_load_n(&evr_state.ready, __ATOMIC_ACQUIRE));
}

int
evr_is_active(void)
{
	return (__atomic_load_n(&evr_state.active, __ATOMIC_ACQUIRE));
}

int
evr_get_width(void)
{
	if (!evr_is_ready()) {
		return (0);
	}
	return ((int)evr_state.cols);
}

int
evr_get_height(void)
{
	if (!evr_is_ready()) {
		return (0);
	}
	return ((int)evr_state.rows);
}

void
evr_set_colors(u32 fg, u32 bg)
{
	if (!evr_writer_enter()) {
		return;
	}
	evr_state.fg = fg & 0x00FFFFFF;
	evr_state.bg = bg & 0x00FFFFFF;
	evr_writer_leave();
}

void
evr_put_entry_at(char c, u32 x, u32 y)
{
	if (!evr_writer_enter()) {
		return;
	}
	evr_draw_glyph_locked(c, x, y);
	evr_writer_leave();
}

void
evr_putc(char c)
{
	if (!evr_writer_enter()) {
		return;
	}
	evr_putc_locked(c);
	evr_writer_leave();
}

void
evr_write(const char *s)
{
	const char	*p;

	if (s == NULL || !evr_writer_enter()) {
		return;
	}
	for (p = s; *p != '\0'; p++) {
		evr_putc_locked(*p);
	}
	evr_writer_leave();
}

void
evr_clear(void)
{
	if (!evr_writer_enter()) {
		return;
	}
	evr_clear_locked();
	evr_writer_leave();
}

void
evr_handoff(void)
{
	if (!evr_is_ready()) {
		return;
	}
	__atomic_store_n(&evr_state.active, 0, __ATOMIC_RELEASE);
	while (__atomic_load_n(&evr_state.writers, __ATOMIC_ACQUIRE) != 0) {
		__asm__ volatile("pause");
	}
}

static devclass_t	evr_devclass = {
	.name = "evr",
	.maxunit = 1,
};

static driver_t	evr_driver = {
	.name = "evr",
	.identify = evr_identify,
	.probe = evr_probe,
	.attach = evr_attach,
	.shutdown = evr_shutdown,
};

FIRMWARE_DRIVER_MODULE(evr, evr_driver, evr_devclass,
    NEWBUS_PASS_FIRMWARE, NEWBUS_ORDER_EARLY);
