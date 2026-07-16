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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type char as 8 bit signed

$define %func outb as procedure with args u16, u8
$define %func inb as function with args u16
$define %func serial_ready as function with args void
$define %func serial_putc as procedure with args char
$define %func vga_putc as procedure with args char
$define %func bios_console_init as procedure with args void
$define %func bios_console_putc as procedure with args char
$define %func bios_console_puts as procedure with args const char *
$define %func bios_console_puthex as procedure with args u32

*/

/* !SPACE!

$space %internal outb, inb, serial_ready, serial_putc, vga_putc
$space %export bios_console_init, bios_console_putc
$space %export bios_console_puts, bios_console_puthex

*/

#include <boot/bootloader/bios/bios.h>

#define COM1		0x3f8
#define VGA_TEXT	((u16 *)0xb8000)
#define VGA_W		80
#define VGA_H		25

static u32	vga_x;
static u32	vga_y;

static void
outb(u16 port, u8 value)
{
	__asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static u8
inb(u16 port)
{
	u8	value;

	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return (value);
}

static int
serial_ready(void)
{
	return ((inb(COM1 + 5) & 0x20) != 0);
}

static void
serial_putc(char c)
{
	while (!serial_ready()) {
		;
	}
	outb(COM1, (u8)c);
}

static void
vga_putc(char c)
{
	u32	i;

	if (c == '\n') {
		vga_x = 0;
		vga_y++;
	} else {
		VGA_TEXT[vga_y * VGA_W + vga_x] = 0x0f00 | (u8)c;
		vga_x++;
	}
	if (vga_x >= VGA_W) {
		vga_x = 0;
		vga_y++;
	}
	if (vga_y >= VGA_H) {
		for (i = 0; i < VGA_W * VGA_H; i++) {
			VGA_TEXT[i] = 0x0f20;
		}
		vga_x = 0;
		vga_y = 0;
	}
}

void
bios_console_init(void)
{
	u32	i;

	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x80);
	outb(COM1 + 0, 0x03);
	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x03);
	outb(COM1 + 2, 0xc7);
	outb(COM1 + 4, 0x0b);
	vga_x = 0;
	vga_y = 0;
	for (i = 0; i < VGA_W * VGA_H; i++) {
		VGA_TEXT[i] = 0x0f20;
	}
}

void
bios_console_putc(char c)
{
	if (c == '\n') {
		serial_putc('\r');
	}
	serial_putc(c);
	vga_putc(c);
}

void
bios_console_puts(const char *str)
{
	while (*str) {
		bios_console_putc(*str);
		str++;
	}
}

void
bios_console_puthex(u32 value)
{
	const char	*hex;
	int		i;

	hex = "0123456789abcdef";
	bios_console_puts("0x");
	for (i = 28; i >= 0; i -= 4) {
		bios_console_putc(hex[(value >> i) & 0xf]);
	}
}
