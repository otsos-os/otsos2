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

$define %const PIT_FREQUENCY as 1193182
$define %const PIT_COMMAND as 0x43
$define %const PIT_CHANNEL0 as 0x40
$define %const PIT_MAX_DIVISOR as 65535

*/

/* !SPACE!

$space %export pit_init

*/

#include <kernel/drivers/eventtimer.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define PIT_FREQUENCY	1193182ULL
#define PIT_COMMAND	0x43
#define PIT_CHANNEL0	0x40
#define PIT_MAX_DIVISOR	65535

static struct eventtimer	pit_et;

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
	(void)dev;
	pit_init();
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
