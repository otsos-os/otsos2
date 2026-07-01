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

$define %type int as 32 bit signed
$define %type char as 8 bit signed

$define %func kshell_set_boot_info as procedure with args int
$define %func kshell_run as procedure with args void
$define %func kshell_request_open as procedure with args void
$define %func kshell_try_open_if_requested as function with args void
$define %func kshell_console_clear as procedure with args void
$define %func kshell_console_putc as procedure with args char
$define %func kshell_console_write as procedure with args const char *
$define %func kshell_console_write_int as procedure with args int
$define %func kshell_console_write_ptr as procedure with args const void *
$define %func kshell_parse_line as function with args char *, char *[], int

*/

/* !SPACE!

$space %export kshell_set_boot_info, kshell_run
$space %export kshell_request_open, kshell_try_open_if_requested
$space %export kshell_console_clear, kshell_console_putc
$space %export kshell_console_write, kshell_console_write_int
$space %export kshell_console_write_ptr, kshell_parse_line

*/


#include <kernel/kshell/kshell.h>

void
kshell_set_boot_info(int is_multiboot2)
{
	(void)is_multiboot2;
}

void
kshell_run(void)
{
}

void
kshell_request_open(void)
{
}

int
kshell_try_open_if_requested(void)
{
	return (0);
}

void
kshell_console_clear(void)
{
}

void
kshell_console_putc(char c)
{
	(void)c;
}

void
kshell_console_write(const char *s)
{
	(void)s;
}

void
kshell_console_write_int(int value)
{
	(void)value;
}

void
kshell_console_write_ptr(const void *ptr)
{
	(void)ptr;
}

int
kshell_parse_line(char *line, char *argv[], int max_args)
{
	(void)line;
	(void)argv;
	(void)max_args;
	return (0);
}

int
kshell_is_multiboot2(void)
{
	return (0);
}
