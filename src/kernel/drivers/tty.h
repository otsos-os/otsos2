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

$define %type u32 as 32 bit unsigned
$define %type u8 as 8 bit unsigned
$define %type int as 32 bit signed

$define %func tty_read as function with args void *, u32
$define %func tty_write as function with args const void *, u32
$define %func tty_init as procedure with args void
$define %func tty_is_initialized as function with args void
$define %func tty_reinit as procedure with args void
$define %func tty_putc_from_kernel as procedure with args char
$define %func tty_flush_kernel as procedure with args void
$define %func tty_set_color as procedure with args u8
$define %func tty_clear_active as procedure with args void
$define %func tty_com1_mirror as procedure with args char
$define %func tty_set_active as procedure with args int
$define %func tty_restore_active_display as procedure with args void
$define %func tty_update as procedure with args void
$define %func tty_get_input_channel as function with args void
$define %func tty_power_get as function with args int
$define %func tty_power_set as function with args int, int
$define %func tty_power_reset as function with args int

*/

/* !SPACE!

$space %export tty_read, tty_write, tty_init, tty_is_initialized
$space %export tty_reinit, tty_putc_from_kernel, tty_flush_kernel
$space %export tty_set_color, tty_clear_active, tty_com1_mirror
$space %export tty_set_active, tty_restore_active_display
$space %export tty_update, tty_get_input_channel
$space %export tty_power_get, tty_power_set, tty_power_reset

*/

#ifndef TTY_H
#define TTY_H

#include <mlibc/mlibc.h>

int	tty_read(void *buf, u32 count);
int	tty_write(const void *buf, u32 count);
void	tty_init(void);
int	tty_is_initialized(void);
void	tty_reinit(void);
void	tty_putc_from_kernel(char c);
void	tty_flush_kernel(void);
void	tty_set_color(u8 color);
void	tty_clear_active(void);
void	tty_com1_mirror(char c);
void	tty_set_active(int index);
void	tty_restore_active_display(void);
void	tty_update(void);
void	*tty_get_input_channel(void);

#define	TTY_STATE_ACTIVE	0
#define	TTY_STATE_SUSPENDED	1
#define	TTY_STATE_DISABLED	2

int	tty_power_get(int index);
int	tty_power_set(int index, int state);
int	tty_power_reset(int index);
int	tty_power_suspend_all(void);

#endif
