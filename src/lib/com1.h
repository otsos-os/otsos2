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

#ifndef COM1_H
#define COM1_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#define COM1_PORT 0x3F8
#define COM1_DATA (COM1_PORT + 0)
#define COM1_INT_ENABLE (COM1_PORT + 1)
#define COM1_FIFO_CTRL (COM1_PORT + 2)
#define COM1_LINE_CTRL (COM1_PORT + 3)
#define COM1_MODEM_CTRL (COM1_PORT + 4)
#define COM1_LINE_STATUS (COM1_PORT + 5)
void com1_init(void);
void com1_write_byte(u8 c);
void com1_write_string(const char *str);
void com1_write_hex_byte(u8 value);
void com1_write_hex_word(u16 value);
void com1_write_hex_dword(u32 value);
void com1_write_hex_qword(u64 value);
void com1_newline(void);
u8 com1_read_byte(void);
int com1_has_data(void);
void com1_write_dec(u64 value);
void com1_printf(const char *format, ...);
void com1_set_mirror_callback(void (*callback)(char));
void com1_off_mirror_callback(void);
#endif