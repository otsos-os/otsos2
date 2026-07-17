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
#include <mlibc/mlibc.h>

#define COM1_PORT		0x3F8
#define COM1_DATA		(COM1_PORT + 0)
#define COM1_INT_ENABLE		(COM1_PORT + 1)
#define COM1_FIFO_CTRL		(COM1_PORT + 2)
#define COM1_LINE_CTRL		(COM1_PORT + 3)
#define COM1_MODEM_CTRL		(COM1_PORT + 4)
#define COM1_LINE_STATUS	(COM1_PORT + 5)
#define COM1_SCRATCH		(COM1_PORT + 7)

#define UART_LSR_DATA_READY	0x01
#define UART_LSR_THR_EMPTY	0x20
#define UART_TIMEOUT		100000

static int	uart_available = 0;

static int
uart_wait_status(u8 mask)
{
	int	timeout;

	timeout = UART_TIMEOUT;
	while (timeout-- > 0) {
		if (inb(COM1_LINE_STATUS) & mask) {
			return (1);
		}
	}
	return (0);
}

static int
uart_probe(void)
{
	u8	saved, value;

	value = inb(COM1_LINE_STATUS);
	if (value == 0xFF) {
		return (0);
	}

	saved = inb(COM1_SCRATCH);
	outb(COM1_SCRATCH, 0x55);
	if (inb(COM1_SCRATCH) != 0x55) {
		outb(COM1_SCRATCH, saved);
		return (0);
	}
	outb(COM1_SCRATCH, 0xAA);
	if (inb(COM1_SCRATCH) != 0xAA) {
		outb(COM1_SCRATCH, saved);
		return (0);
	}
	outb(COM1_SCRATCH, saved);
	return (1);
}

int
uart_is_available(void)
{
	return (uart_available);
}

void
uart_init(void)
{
	if (!uart_probe()) {
		uart_available = 0;
		return;
	}

	outb(COM1_INT_ENABLE, 0x00);
	outb(COM1_LINE_CTRL, 0x80);
	outb(COM1_DATA, 0x01);
	outb(COM1_INT_ENABLE, 0x00);
	outb(COM1_LINE_CTRL, 0x03);
	outb(COM1_FIFO_CTRL, 0xC7);
	outb(COM1_MODEM_CTRL, 0x0B);
	uart_available = 1;
}

void
uart_write_byte(u8 byte)
{
	if (!uart_available) {
		return;
	}
	if (!uart_wait_status(UART_LSR_THR_EMPTY)) {
		return;
	}
	outb(COM1_DATA, byte);
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
	if (!uart_wait_status(UART_LSR_DATA_READY)) {
		return (0);
	}
	return (inb(COM1_DATA));
}

int
uart_has_data(void)
{
	if (!uart_available) {
		return (0);
	}
	return (inb(COM1_LINE_STATUS) & UART_LSR_DATA_READY);
}
