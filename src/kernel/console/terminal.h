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

$define %func terminal_read as function with args void *, u32
$define %func terminal_write as function with args const void *, u32
$define %func terminal_init as procedure with args void
$define %func terminal_is_initialized as function with args void
$define %func terminal_reinit as procedure with args void
$define %func terminal_putc_from_kernel as procedure with args char
$define %func terminal_flush_kernel as procedure with args void
$define %func terminal_set_color as procedure with args u8
$define %func terminal_clear_active as procedure with args void
$define %func terminal_log_mirror as procedure with args char
$define %func terminal_putc_to as procedure with args int, char
$define %func terminal_puts_to as procedure with args int, const char *
$define %func terminal_set_active as procedure with args int
$define %func terminal_restore_active_display as procedure with args void
$define %func terminal_update as procedure with args void
$define %func terminal_get_input_channel as function with args void
$define %func terminal_power_get as function with args int
$define %func terminal_power_set as function with args int, int
$define %func terminal_power_reset as function with args int
$define %func terminal_power_suspend_all as function with args void

*/

/* !SPACE!

$space %export terminal_read, terminal_write, terminal_init
$space %export terminal_is_initialized, terminal_reinit
$space %export terminal_putc_from_kernel, terminal_flush_kernel
$space %export terminal_set_color, terminal_clear_active, terminal_log_mirror
$space %export terminal_putc_to, terminal_puts_to
$space %export terminal_set_active, terminal_restore_active_display
$space %export terminal_update, terminal_get_input_channel
$space %export terminal_power_get, terminal_power_set, terminal_power_reset
$space %export terminal_power_suspend_all

*/

#ifndef KERNEL_CONSOLE_TERMINAL_H
#define KERNEL_CONSOLE_TERMINAL_H

#include <mlibc/mlibc.h>

#define	TERM_STATE_ACTIVE	0
#define	TERM_STATE_SUSPENDED	1
#define	TERM_STATE_DISABLED	2

int	terminal_read(void *buf, u32 count);
int	terminal_write(const void *buf, u32 count);
void	terminal_init(void);
int	terminal_is_initialized(void);
void	terminal_reinit(void);
void	terminal_putc_from_kernel(char c);
void	terminal_flush_kernel(void);
void	terminal_set_color(u8 color);
void	terminal_clear_active(void);
void	terminal_log_mirror(char c);
void	terminal_putc_to(int index, char c);
void	terminal_puts_to(int index, const char *s);
void	terminal_set_active(int index);
void	terminal_restore_active_display(void);
void	terminal_update(void);
void	*terminal_get_input_channel(void);
int	terminal_power_get(int index);
int	terminal_power_set(int index, int state);
int	terminal_power_reset(int index);
int	terminal_power_suspend_all(void);

#endif
