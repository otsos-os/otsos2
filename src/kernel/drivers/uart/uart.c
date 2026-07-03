/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/drivers/uart/uart.h>

#define COM1_PORT		0x3F8
#define COM1_DATA		(COM1_PORT + 0)
#define COM1_INT_ENABLE		(COM1_PORT + 1)
#define COM1_FIFO_CTRL		(COM1_PORT + 2)
#define COM1_LINE_CTRL		(COM1_PORT + 3)
#define COM1_MODEM_CTRL		(COM1_PORT + 4)
#define COM1_LINE_STATUS	(COM1_PORT + 5)

static int	uart_available = 0;

static inline void
uart_outb(u16 port, u8 value)
{
	__asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8
uart_inb(u16 port)
{
	u8	value;

	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return (value);
}

int
uart_is_available(void)
{
	return (uart_available);
}

void
uart_init(void)
{
	if (uart_inb(COM1_LINE_STATUS) == 0xFF) {
		uart_available = 0;
		return;
	}

	uart_outb(COM1_INT_ENABLE, 0x00);
	uart_outb(COM1_LINE_CTRL, 0x80);
	uart_outb(COM1_DATA, 0x01);
	uart_outb(COM1_INT_ENABLE, 0x00);
	uart_outb(COM1_LINE_CTRL, 0x03);
	uart_outb(COM1_FIFO_CTRL, 0xC7);

	uart_outb(COM1_MODEM_CTRL, 0x1E);
	uart_outb(COM1_DATA, 0xAE);
	if (uart_inb(COM1_DATA) != 0xAE) {
		uart_available = 0;
		uart_outb(COM1_MODEM_CTRL, 0x00);
		return;
	}

	uart_outb(COM1_MODEM_CTRL, 0x0B);
	uart_available = 1;
}

void
uart_write_byte(u8 byte)
{
	if (!uart_available) {
		return;
	}
	while ((uart_inb(COM1_LINE_STATUS) & 0x20) == 0)
		;
	uart_outb(COM1_DATA, byte);
}

void
uart_write_string(const char *str)
{
	while (*str) {
		uart_write_byte(*str++);
	}
}

u8
uart_read_byte(void)
{
	if (!uart_available) {
		return (0);
	}
	while ((uart_inb(COM1_LINE_STATUS) & 0x01) == 0)
		;
	return (uart_inb(COM1_DATA));
}

int
uart_has_data(void)
{
	if (!uart_available) {
		return (0);
	}
	return (uart_inb(COM1_LINE_STATUS) & 0x01);
}
