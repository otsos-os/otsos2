/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type char as 8 bit signed
$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed

$define %func console_init as procedure with args void
$define %func console_is_initialized as function with args void
$define %func console_reinit as procedure with args void
$define %func console_putchar as procedure with args char
$define %func console_puts as procedure with args const char *
$define %func printf as procedure with args const char *, ...
$define %func clear_scr as procedure with args void
$define %func console_set_color as procedure with args u8
$define %func console_color_rgb as function with args u8
$define %func console_put_entry_at as procedure with args char, u8, int, int
$define %func console_get_width as function with args void
$define %func console_get_height as function with args void

*/

/* !SPACE!

$space %export console_init, console_is_initialized, console_reinit
$space %export console_putchar, console_puts, printf, clear_scr
$space %export console_set_color, console_color_rgb, console_put_entry_at
$space %export console_get_width, console_get_height

*/

#ifndef KERNEL_CONSOLE_CONSOLE_H
#define KERNEL_CONSOLE_CONSOLE_H

#include <mlibc/mlibc.h>
void	console_init(void);
int	console_is_initialized(void);
void	console_reinit(void);
void	console_putchar(char c);
void	console_puts(const char *s);
void	printf(const char *fmt, ...);
void	clear_scr(void);
void	console_set_color(u8 color);
u32	console_color_rgb(u8 attr);
void	console_put_entry_at(char c, u8 color, int x, int y);
int	console_get_width(void);
int	console_get_height(void);
#endif
