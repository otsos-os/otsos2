/*
 * Copyright (c) 2026, otsos team
 */

/* !DEFINES!

$define %type char as 8 bit signed
$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type kms_console_t as struct with KMS console state

$define %func console_color_rgb as function with args u8
$define %func console_get_width as function with args void
$define %func console_get_height as function with args void
$define %func console_init as procedure with args void
$define %func console_is_initialized as function with args void
$define %func console_reinit as procedure with args void
$define %func console_set_color as procedure with args u8
$define %func console_put_entry_at as procedure with args char, u8, int, int
$define %func early_scroll as procedure with args void
$define %func early_putc as procedure with args char
$define %func console_putchar as procedure with args char
$define %func console_puts as procedure with args const char *
$define %func clear_scr as procedure with args void
$define %func printf as procedure with args const char *, ...

*/

/* !SPACE!

$space %export console_color_rgb, console_get_width, console_get_height
$space %export console_init, console_is_initialized, console_reinit
$space %export console_set_color, console_put_entry_at, console_putchar
$space %export console_puts, clear_scr, printf
$space %internal early_scroll, early_putc

*/

#include <kernel/console/console.h>
#include <kernel/console/palette.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/video/drm/kms/console.h>
#include <kernel/drivers/video/drm/kms/crtc.h>
#include <mlibc/mlibc.h>
#include <stdarg.h>

static int	early_x = 0;
static int	early_y = 0;

u32
console_color_rgb(u8 attr)
{
	return (console_palette[attr & 0x0F]);
}

int
console_get_width(void)
{
	kms_console_t	*con;

	con = kms_kernel_console();
	if (con) {
		return ((int)con->cols);
	}
	if (drm_is_ready()) {
		return ((int)(drm_crtc_get_width() / 8));
	}
	return (80);
}

int
console_get_height(void)
{
	kms_console_t	*con;

	con = kms_kernel_console();
	if (con) {
		return ((int)con->rows);
	}
	if (drm_is_ready()) {
		return ((int)(drm_crtc_get_height() / 16));
	}
	return (25);
}

void
console_init(void)
{
	terminal_init();
}

int
console_is_initialized(void)
{
	return (terminal_is_initialized());
}

void
console_reinit(void)
{
	terminal_reinit();
}

void
console_set_color(u8 color)
{
	if (terminal_is_initialized()) {
		terminal_set_color(color);
	}
}

void
console_put_entry_at(char c, u8 color, int x, int y)
{
	kms_console_t	*con;

	if (terminal_is_initialized()) {
		return;
	}

	con = kms_kernel_console();
	if (!con) {
		return;
	}

	kms_console_glyph(con, (u32)(x * 8), (u32)(y * 16), c,
	    console_color_rgb(color), 0x000000);
	kms_console_flush(con);
}

static void
early_scroll(void)
{
	kms_console_t	*con;

	con = kms_kernel_console();
	if (con) {
		kms_console_scroll_up(con, 16, 0x000000);
	}
	if (early_y > 0) {
		early_y--;
	}
}

static void
early_putc(char c)
{
	kms_console_t	*con;
	int		 w;
	int		 h;

	con = kms_kernel_console();
	if (!con) {
		return;
	}

	w = (int)con->cols;
	h = (int)con->rows;
	if (w <= 0) {
		w = 80;
	}
	if (h <= 0) {
		h = 25;
	}

	if (c == '\n') {
		early_x = 0;
		early_y++;
		kms_console_flush(con);
	} else if (c == '\r') {
		early_x = 0;
	} else if (c == '\b') {
		if (early_x > 0) {
			early_x--;
		} else if (early_y > 0) {
			early_y--;
			early_x = w - 1;
		}
	} else {
		kms_console_glyph(con, (u32)(early_x * 8), (u32)(early_y * 16), c,
		    0xAAAAAA, 0x000000);
		early_x++;
	}

	if (early_x >= w) {
		early_x = 0;
		early_y++;
	}
	if (early_y >= h) {
		early_scroll();
		early_y = h - 1;
	}
}

void
console_putchar(char c)
{
	if (terminal_is_initialized()) {
		terminal_putc_from_kernel(c);
		return;
	}
	early_putc(c);
}

void
console_puts(const char *s)
{
	int	i;

	if (!s) {
		return;
	}
	for (i = 0; s[i]; i++) {
		console_putchar(s[i]);
	}
}

void
clear_scr(void)
{
	kms_console_t	*con;

	if (terminal_is_initialized()) {
		terminal_clear_active();
		terminal_flush_kernel();
		return;
	}

	con = kms_kernel_console();
	if (con) {
		kms_console_clear(con, 0x000000);
		kms_console_flush(con);
	}
	early_x = 0;
	early_y = 0;
}

void
printf(const char *fmt, ...)
{
	char	buffer[512];
	va_list	 args;

	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	console_puts(buffer);
}
