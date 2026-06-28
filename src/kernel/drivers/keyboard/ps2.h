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

$define %type char as 8 bit signed
$define %type int as 32 bit signed

$define %func ps2_keyboard_init as function with args void
$define %func ps2_keyboard_handler as procedure with args void
$define %func ps2_keyboard_getchar as function with args void
$define %func ps2_keyboard_poll as procedure with args void
$define %func ps2_keyboard_reset_state as procedure with args void
$define %func ps2Scanf as function with args const char *, ...

*/

/* !SPACE!

$space %export ps2_keyboard_init, ps2_keyboard_handler
$space %export ps2_keyboard_getchar, ps2_keyboard_poll
$space %export ps2_keyboard_reset_state, ps2Scanf

*/

#ifndef PS2_H
#define PS2_H

int	ps2_keyboard_init(void);
void	ps2_keyboard_handler(void);
char	ps2_keyboard_getchar(void);
void	ps2_keyboard_poll(void);
void	ps2_keyboard_reset_state(void);
int	ps2Scanf(const char *format, ...);

#endif
