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
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/mlibc.h>

#define COM1_PORT		0x3F8
#define COM1_DATA		(COM1_PORT + 0)
#define COM1_INT_ENABLE		(COM1_PORT + 1)
#define COM1_INT_IDENT		(COM1_PORT + 2)
#define COM1_FIFO_CTRL		(COM1_PORT + 2)
#define COM1_LINE_CTRL		(COM1_PORT + 3)
#define COM1_MODEM_CTRL		(COM1_PORT + 4)
#define COM1_LINE_STATUS	(COM1_PORT + 5)
#define COM1_SCRATCH		(COM1_PORT + 7)

#define UART_LSR_DATA_READY	0x01
#define UART_LSR_THR_EMPTY	0x20
#define UART_TIMEOUT		100000
#define UART_RX_RING_SIZE	256
#define UART_IIR_NONE		0x01

static int	uart_available = 0;
static u8	uart_rx_ring[UART_RX_RING_SIZE];
static u16	uart_rx_head;
static u16	uart_rx_tail;
static resource_t *uart_irq_res;
static void	*uart_irq_cookie;

static int
uart_intr(void *arg)
{
	u16	next;
	u8	value;
	int	handled;

	(void)arg;
	handled = 0;
	while ((inb(COM1_INT_IDENT) & UART_IIR_NONE) == 0) {
		if ((inb(COM1_LINE_STATUS) & UART_LSR_DATA_READY) == 0) {
			(void)inb(COM1_LINE_STATUS);
			break;
		}
		value = inb(COM1_DATA);
		next = (u16)((uart_rx_head + 1) % UART_RX_RING_SIZE);
		if (next != uart_rx_tail) {
			uart_rx_ring[uart_rx_head] = value;
			uart_rx_head = next;
		}
		handled = 1;
	}
	return (handled ? 0 : -1);
}

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
	u8	value;

	if (!uart_available) {
		return (0);
	}
	if (uart_rx_head != uart_rx_tail) {
		value = uart_rx_ring[uart_rx_tail];
		uart_rx_tail = (u16)((uart_rx_tail + 1) % UART_RX_RING_SIZE);
		return (value);
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
	return (uart_rx_head != uart_rx_tail ||
	    (inb(COM1_LINE_STATUS) & UART_LSR_DATA_READY));
}

static void
uart_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "uart", 0) == NULL) {
		device_add_child(parent, "uart", 0);
	}
}

static int
uart_bus_probe(device_t dev)
{
	(void)dev;
	return (uart_probe() ? 0 : -1);
}

static int
uart_attach(device_t dev)
{
	int	rid;

	bus_set_resource(dev, SYS_RES_IOPORT, 0, COM1_PORT, 8, 0);
	bus_set_resource(dev, SYS_RES_IRQ, 0, 4, 1, RF_IRQ_ISA);
	uart_init();
	if (!uart_is_available()) {
		return (-1);
	}
	rid = 0;
	uart_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (uart_irq_res == NULL || bus_setup_intr(dev, uart_irq_res,
	    uart_intr, NULL, &uart_irq_cookie) != 0) {
		return (-1);
	}
	outb(COM1_INT_ENABLE, 0x01);
	return (0);
}

static int
uart_detach(device_t dev)
{
	outb(COM1_INT_ENABLE, 0x00);
	if (uart_irq_cookie != NULL) {
		bus_teardown_intr(dev, uart_irq_res, uart_irq_cookie);
		uart_irq_cookie = NULL;
	}
	if (uart_irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, uart_irq_res->rid,
		    uart_irq_res);
		uart_irq_res = NULL;
	}
	return (0);
}

static devclass_t uart_devclass = {
	.name		= "uart",
	.maxunit	= 1,
};

static driver_t uart_driver = {
	.name		= "uart",
	.identify	= uart_identify,
	.probe		= uart_bus_probe,
	.attach		= uart_attach,
	.detach		= uart_detach,
};

ISA_DRIVER_MODULE(uart, uart_driver, uart_devclass,
    NEWBUS_PASS_FIRMWARE, NEWBUS_ORDER_EARLY);
