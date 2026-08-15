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

$define %type char as 8 bit signed
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed

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

$space %export evr_is_ready, evr_is_active
$space %export evr_get_width, evr_get_height
$space %export evr_set_colors, evr_put_entry_at
$space %export evr_putc, evr_write, evr_clear, evr_handoff

*/

#ifndef KERNEL_DRIVERS_VIDEO_EVR_EVR_H
#define KERNEL_DRIVERS_VIDEO_EVR_EVR_H

#include <mlibc/mlibc.h>

int	evr_is_ready(void);
int	evr_is_active(void);
int	evr_get_width(void);
int	evr_get_height(void);
void	evr_set_colors(u32 fg, u32 bg);
void	evr_put_entry_at(char c, u32 x, u32 y);
void	evr_putc(char c);
void	evr_write(const char *s);
void	evr_clear(void);
void	evr_handoff(void);

#endif
