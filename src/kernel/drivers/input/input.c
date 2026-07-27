/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

#include <kernel/drivers/input/input.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/event/event.h>
#include <mlibc/mlibc.h>

static struct api_input_event	input_event_ring[INPUT_EVENT_RING_SIZE];
static u64			input_event_seq;

static u64
input_irq_save(void)
{
	u64	flags;

	__asm__ volatile("pushfq; popq %0; cli"
	    : "=r"(flags)
	    :
	    : "memory");
	return (flags);
}

static void
input_irq_restore(u64 flags)
{
	if (flags & 0x200) {
		__asm__ volatile("sti" ::: "memory");
	}
}

static void
input_event_put(struct api_input_event *event)
{
	u64	flags;
	u64	seq;

	flags = input_irq_save();
	seq = input_event_seq++;
	event->seq = seq;
	input_event_ring[seq % INPUT_EVENT_RING_SIZE] = *event;
	input_irq_restore(flags);

	knote_notify_all(EVFILT_INPUT, 0, 0, 1);
}

void
input_event_keyboard(u64 timestamp, u16 key, u16 raw, u32 flags, u32 mods,
    u32 ch)
{
	struct api_input_event	event;

	memset(&event, 0, sizeof(event));
	event.timestamp = timestamp;
	event.type = API_INPUT_TYPE_KEYBOARD;
	event.device = API_INPUT_DEVICE_KEYBOARD0;
	event.flags = flags;
	event.key = key;
	event.raw = raw;
	event.mods = mods;
	event.ch = ch;
	input_event_put(&event);
}

void
input_event_mouse(u64 timestamp, s32 x, s32 y, s32 dx, s32 dy, s32 dz,
    u32 buttons, u32 flags)
{
	struct api_input_event	event;

	memset(&event, 0, sizeof(event));
	event.timestamp = timestamp;
	event.type = API_INPUT_TYPE_MOUSE;
	event.device = API_INPUT_DEVICE_MOUSE0;
	event.flags = flags;
	event.x = x;
	event.y = y;
	event.dx = dx;
	event.dy = dy;
	event.dz = dz;
	event.buttons = buttons;
	input_event_put(&event);
}

u64
input_event_next_seq(void)
{
	u64	flags;
	u64	seq;

	flags = input_irq_save();
	seq = input_event_seq;
	input_irq_restore(flags);
	return (seq);
}

int
input_event_pending(u64 cursor)
{
	u64	flags;
	u64	oldest, seq;
	u64	count;

	flags = input_irq_save();
	seq = input_event_seq;
	oldest = 0;
	if (seq > INPUT_EVENT_RING_SIZE) {
		oldest = seq - INPUT_EVENT_RING_SIZE;
	}
	if (cursor < oldest) {
		cursor = oldest;
	}
	count = cursor < seq ? seq - cursor : 0;
	input_irq_restore(flags);
	if (count > INPUT_EVENT_RING_SIZE) {
		count = INPUT_EVENT_RING_SIZE;
	}
	return ((int)count);
}

int
input_event_get_after(u64 *cursor, struct api_input_event *out)
{
	struct api_input_event	event;
	u64			flags;
	u64			oldest, seq;
	u32			lost;

	if (!cursor) {
		return (0);
	}

	flags = input_irq_save();
	seq = input_event_seq;
	oldest = 0;
	if (seq > INPUT_EVENT_RING_SIZE) {
		oldest = seq - INPUT_EVENT_RING_SIZE;
	}

	lost = 0;
	if (*cursor < oldest) {
		lost = (u32)(oldest - *cursor);
		*cursor = oldest;
	}
	if (*cursor >= seq) {
		input_irq_restore(flags);
		return (0);
	}

	event = input_event_ring[*cursor % INPUT_EVENT_RING_SIZE];
	(*cursor)++;
	input_irq_restore(flags);

	if (lost != 0) {
		event.flags |= API_INPUT_FLAG_DROPPED;
		event.lost += lost;
	}
	if (out) {
		*out = event;
	}
	return (1);
}

void
input_event_reset(void)
{
	u64	flags;

	flags = input_irq_save();
	input_event_seq = 0;
	memset(input_event_ring, 0, sizeof(input_event_ring));
	input_irq_restore(flags);
}

static void
input_core_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "input_core", 0) == NULL) {
		device_add_child(parent, "input_core", 0);
	}
}

static int
input_core_attach(device_t dev)
{
	(void)dev;
	input_event_reset();
	return (0);
}

static devclass_t input_core_devclass = {
	.name		= "input",
	.maxunit	= 1,
};

static driver_t input_core_driver = {
	.name		= "input_core",
	.identify	= input_core_identify,
	.probe		= NULL,
	.attach		= input_core_attach,
};

PSEUDO_DRIVER_MODULE(input_core, input_core_driver,
    input_core_devclass, NEWBUS_PASS_CORE, NEWBUS_ORDER_MIDDLE);
