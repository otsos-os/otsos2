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

/*
 * Kernel console layer.
 *
 * This is the kernel's primary text output path. It sits above the DRM
 * frontend (pixels) and the tty subsystem (virtual terminals). When the tty
 * layer is initialized, characters are routed to the active tty. Before that
 * (early boot), characters are drawn straight to the screen via the DRM
 * frontend using a simple built-in cursor.
 *
 * The old VGA text-mode driver has been removed; this module keeps the
 * familiar printf/clear_scr/... entry points that the rest of the kernel
 * relies on, but they are now thin wrappers over the DRM-based pipeline.
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <mlibc/mlibc.h>

/* Formatted print to the active console (kernel-side printf). */
void printf(const char *fmt, ...);

/* Write a single character. Routes to tty when ready, otherwise draws via
 * the DRM frontend using the console's own cursor. */
void console_putchar(char c);

/* Write a NUL-terminated string. */
void console_puts(const char *s);

/* Map a legacy 4-bit VGA attribute nibble to an RGB color. */
u32 console_color_rgb(u8 attr);

/* Draw a character cell at (x, y) with the given attribute. Used by panic
 * and kshell that manage their own cursors. */
void console_put_entry_at(char c, u8 color, int x, int y);

/* Set the active text attribute (affects early-boot drawing + tty color). */
void console_set_color(u8 color);

/* Clear the screen. */
void clear_scr(void);

/* Dimensions of the active display, in text cells (fallback 80x25). */
int console_get_width(void);
int console_get_height(void);

#endif
