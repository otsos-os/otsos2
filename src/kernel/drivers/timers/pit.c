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

$define %type u64 as 64 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u8 as 8 bit unsigned
$define %type int as 32 bit signed

$define %func pit_init as procedure with args void
$define %func pit_irq_disable as procedure with args void
$define %func pit_ch2_wait as procedure with args u16
$define %func pit_delay_us as procedure with args u32
$define %func pit_stop as function with args struct eventtimer *

$define %const PIT_FREQUENCY as 1193182
$define %const PIT_COMMAND as 0x43
$define %const PIT_CHANNEL0 as 0x40
$define %const PIT_MAX_DIVISOR as 65535

*/

/* !SPACE!

$space %internal pit_ch2_wait, pit_stop
$space %export pit_init, pit_irq_disable, pit_delay_us

*/

#include <kernel/drivers/eventtimer.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/interrupts/irq.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define PIT_FREQUENCY	1193182ULL
#define PIT_COMMAND	0x43
#define PIT_CHANNEL0	0x40
#define PIT_CHANNEL2	0x42
#define PIT_MAX_DIVISOR	65535
#define PIT_NMI_CTRL	0x61
#define PIT_NMI_GATE2	0x01
#define PIT_NMI_SPKR2	0x02
#define PIT_NMI_OUT2	0x20
#define PIT_CH2_MODE0	0xB0
#define PIT_CH2_MAX_US	50000U

static struct eventtimer	pit_et;
static void			*pit_irq_cookie;

extern irq_result_t irq_system_tick(registers_t *regs, void *arg);
extern void	pic_mask_irq(unsigned char irq);
static int	pit_stop(struct eventtimer *et);

void
pit_irq_disable(void)
{
	if (pit_irq_cookie != NULL) {
		irq_release(pit_irq_cookie);
		pit_irq_cookie = NULL;
	}
	(void)pit_stop(&pit_et);
	pic_mask_irq(0);
}


static void
pit_ch2_wait(u16 count)
{
	u8	ctrl;
	u32	guard;

	ctrl = inb(PIT_NMI_CTRL);
	outb(PIT_NMI_CTRL, (u8)((ctrl & ~(PIT_NMI_GATE2 |
	    PIT_NMI_SPKR2))));

	outb(PIT_COMMAND, PIT_CH2_MODE0);
	outb(PIT_CHANNEL2, (u8)(count & 0xFF));
	outb(PIT_CHANNEL2, (u8)((count >> 8) & 0xFF));

	outb(PIT_NMI_CTRL, (u8)(((ctrl & ~PIT_NMI_SPKR2) |
	    PIT_NMI_GATE2)));

	guard = (u32)count * 2U + 4096U;
	while ((inb(PIT_NMI_CTRL) & PIT_NMI_OUT2) == 0) {
		if (guard-- == 0) {
			break;
		}
	}

	outb(PIT_NMI_CTRL, (u8)(ctrl & ~(PIT_NMI_GATE2 |
	    PIT_NMI_SPKR2)));
}

void
pit_delay_us(u32 us)
{
	u32	chunk;
	u64	count;

	while (us > 0) {
		chunk = (us > PIT_CH2_MAX_US) ? PIT_CH2_MAX_US : us;
		count = (PIT_FREQUENCY * (u64)chunk + 999999ULL) /
		    1000000ULL;
		if (count < 1)
			count = 1;
		else if (count > PIT_MAX_DIVISOR)
			count = PIT_MAX_DIVISOR;
		pit_ch2_wait((u16)count);
		us -= chunk;
	}
}

static u64
pit_ns_to_divisor(u64 period_ns)
{
	u64	divisor;

	divisor = (PIT_FREQUENCY * period_ns + 500000000ULL) /
	    1000000000ULL;
	if (divisor < 1)
		divisor = 1;
	else if (divisor > PIT_MAX_DIVISOR)
		divisor = PIT_MAX_DIVISOR;
	return (divisor);
}

static int
pit_start(struct eventtimer *et, u64 first, u64 period)
{
	u64	divisor;
	u8	l, h;

	(void)first;
	(void)et;

	divisor = pit_ns_to_divisor(period);
	l = (u8)(divisor & 0xFF);
	h = (u8)((divisor >> 8) & 0xFF);

	outb(PIT_COMMAND, 0x36);
	outb(PIT_CHANNEL0, l);
	outb(PIT_CHANNEL0, h);
	return (0);
}

static int
pit_stop(struct eventtimer *et)
{

	(void)et;
	outb(PIT_COMMAND, 0x36);
	outb(PIT_CHANNEL0, 0xFF);
	outb(PIT_CHANNEL0, 0xFF);
	return (0);
}

void
pit_init(void)
{
	u64	max_period_ns;

	max_period_ns = (u64)PIT_MAX_DIVISOR * 1000000000ULL /
	    PIT_FREQUENCY;

	pit_et.et_name = "i8254";
	pit_et.et_flags = ET_FLAGS_PERIODIC;
	pit_et.et_quality = 100;
	pit_et.et_frequency = PIT_FREQUENCY;
	pit_et.et_min_period = 1;
	pit_et.et_max_period = max_period_ns;
	pit_et.et_start = pit_start;
	pit_et.et_stop = pit_stop;
	pit_et.et_event_cb = NULL;
	pit_et.et_deregister_cb = NULL;
	pit_et.et_arg = NULL;
	pit_et.et_priv = NULL;

	et_register(&pit_et);
}

static void
pit_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "pit", 0) == NULL) {
		device_add_child(parent, "pit", 0);
	}
}

static int
pit_attach(device_t dev)
{
	irq_source_t	source;

	pit_init();
	source = irq_source_isa(0);
	if (irq_request(source, irq_system_tick, NULL, "pit",
	    &pit_irq_cookie) != 0) {
		return (-1);
	}
	return (0);
}

static devclass_t pit_devclass = {
	.name		= "pit",
	.maxunit	= 1,
};

static driver_t pit_driver = {
	.name		= "pit",
	.identify	= pit_identify,
	.probe		= NULL,
	.attach		= pit_attach,
};

ISA_DRIVER_MODULE(pit, pit_driver, pit_devclass,
    NEWBUS_PASS_TIMER, NEWBUS_ORDER_FIRST);
