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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s32 as 32 bit signed
$define %type int as 32 bit signed
$define %type mouse_event as struct with normalized mouse input event

$define %func ps2_wait_input_clear as function with args void
$define %func ps2_wait_output_full as function with args void
$define %func ps2_irq_save as function with args void
$define %func ps2_irq_restore as procedure with args u64
$define %func ps2_flush_output as procedure with args void
$define %func ps2_read_data as function with args u8 *
$define %func ps2_read_aux as function with args u8 *
$define %func ps2_write_cmd as function with args u8
$define %func ps2_write_data as function with args u8
$define %func ps2_write_aux as function with args u8
$define %func ps2_aux_command as function with args u8
$define %func ps2_aux_command_arg as function with args u8, u8
$define %func ps2_aux_get_id as function with args u8 *
$define %func ps2_aux_enable_wheel as procedure with args void
$define %func mouse_event_put as procedure with args s32, s32, s32, u32, u32
$define %func ps2_mouse_process_packet as procedure with args void
$define %func ps2_mouse_process_byte as procedure with args u8
$define %func ps2_mouse_drain as procedure with args const char *
$define %func ps2_mouse_init as function with args void
$define %func ps2_mouse_handler as procedure with args void
$define %func ps2_mouse_poll as procedure with args void
$define %func ps2_mouse_is_ready as function with args void
$define %func mouse_event_get as function with args struct mouse_event *
$define %func mouse_event_count as function with args void
$define %func mouse_event_reset as procedure with args void

*/

/* !SPACE!

$space %internal ps2_wait_input_clear, ps2_wait_output_full
$space %internal ps2_irq_save, ps2_irq_restore, ps2_flush_output
$space %internal ps2_read_data, ps2_read_aux, ps2_write_cmd
$space %internal ps2_write_data, ps2_write_aux, ps2_aux_command
$space %internal ps2_aux_command_arg, ps2_aux_get_id
$space %internal ps2_aux_enable_wheel, mouse_event_put
$space %internal ps2_mouse_process_packet, ps2_mouse_process_byte
$space %internal ps2_mouse_drain
$space %export ps2_mouse_init, ps2_mouse_handler, ps2_mouse_poll
$space %export ps2_mouse_is_ready
$space %export mouse_event_get, mouse_event_count, mouse_event_reset

*/

#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/timer.h>
#include <kernel/event/event.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	PS2_DATA_PORT		0x60
#define	PS2_STATUS_PORT		0x64
#define	PS2_CMD_PORT		0x64

#define	PS2_STATUS_OBF		0x01
#define	PS2_STATUS_IBF		0x02
#define	PS2_STATUS_AUX		0x20

#define	PS2_CMD_READ_CONFIG	0x20
#define	PS2_CMD_WRITE_CONFIG	0x60
#define	PS2_CMD_DISABLE_PORT1	0xAD
#define	PS2_CMD_ENABLE_PORT1	0xAE
#define	PS2_CMD_ENABLE_PORT2	0xA8
#define	PS2_CMD_TEST_PORT2	0xA9
#define	PS2_CMD_WRITE_AUX	0xD4

#define	PS2_CONFIG_PORT1_IRQ	0x01
#define	PS2_CONFIG_PORT2_IRQ	0x02
#define	PS2_CONFIG_PORT1_CLOCK	0x10
#define	PS2_CONFIG_PORT2_CLOCK	0x20
#define	PS2_CONFIG_TRANSLATION	0x40
#define	PS2_RFLAGS_IF		0x200

#define	PS2_MOUSE_ACK		0xFA
#define	PS2_MOUSE_RESEND	0xFE
#define	PS2_MOUSE_RESET		0xFF
#define	PS2_MOUSE_DEFAULTS	0xF6
#define	PS2_MOUSE_DISABLE	0xF5
#define	PS2_MOUSE_ENABLE	0xF4
#define	PS2_MOUSE_SAMPLE_RATE	0xF3
#define	PS2_MOUSE_GET_ID	0xF2

static struct mouse_event	mouse_event_ring[MOUSE_EVENT_RING_SIZE];
static int			mouse_event_head;
static int			mouse_event_tail;
static int			mouse_ready;
static int			mouse_packet_index;
static int			mouse_packet_size;
static u8			mouse_packet[4];
static s32			mouse_x;
static s32			mouse_y;
static u32			mouse_buttons;

static int
ps2_wait_input_clear(void)
{
	u32	i;

	for (i = 0; i < 100000; i++) {
		if ((inb(PS2_STATUS_PORT) & PS2_STATUS_IBF) == 0) {
			return (0);
		}
	}
	return (-1);
}

static int
ps2_wait_output_full(void)
{
	u32	i;

	for (i = 0; i < 100000; i++) {
		if (inb(PS2_STATUS_PORT) & PS2_STATUS_OBF) {
			return (0);
		}
	}
	return (-1);
}

static u64
ps2_irq_save(void)
{
	u64	flags;

	__asm__ volatile("pushfq; popq %0; cli"
	    : "=r"(flags)
	    :
	    : "memory");
	return (flags);
}

static void
ps2_irq_restore(u64 flags)
{
	if (flags & PS2_RFLAGS_IF) {
		__asm__ volatile("sti" ::: "memory");
	}
}

static void
ps2_flush_output(void)
{
	u32	i;

	for (i = 0; i < 1024; i++) {
		if ((inb(PS2_STATUS_PORT) & PS2_STATUS_OBF) == 0) {
			return;
		}
		(void)inb(PS2_DATA_PORT);
	}
}

static int
ps2_read_data(u8 *data)
{
	if (ps2_wait_output_full() != 0) {
		return (-1);
	}
	*data = inb(PS2_DATA_PORT);
	return (0);
}

static int
ps2_read_aux(u8 *data)
{
	u32	i;
	u8	status;

	for (i = 0; i < 100000; i++) {
		status = inb(PS2_STATUS_PORT);
		if ((status & PS2_STATUS_OBF) == 0) {
			continue;
		}
		if ((status & PS2_STATUS_AUX) == 0) {
			(void)inb(PS2_DATA_PORT);
			continue;
		}
		*data = inb(PS2_DATA_PORT);
		return (0);
	}
	return (-1);
}

static int
ps2_write_cmd(u8 cmd)
{
	if (ps2_wait_input_clear() != 0) {
		return (-1);
	}
	outb(PS2_CMD_PORT, cmd);
	return (0);
}

static int
ps2_write_data(u8 data)
{
	if (ps2_wait_input_clear() != 0) {
		return (-1);
	}
	outb(PS2_DATA_PORT, data);
	return (0);
}

static int
ps2_write_aux(u8 data)
{
	if (ps2_write_cmd(PS2_CMD_WRITE_AUX) != 0) {
		return (-1);
	}
	return (ps2_write_data(data));
}

static int
ps2_aux_command(u8 cmd)
{
	u8	resp;
	int	attempt;

	for (attempt = 0; attempt < 3; attempt++) {
		if (ps2_write_aux(cmd) != 0) {
			return (-1);
		}
		if (ps2_read_aux(&resp) != 0) {
			return (-1);
		}
		if (resp == PS2_MOUSE_RESEND) {
			continue;
		}
		if (resp != PS2_MOUSE_ACK) {
			return (-1);
		}
		return (0);
	}
	return (-1);
}

static int
ps2_aux_command_arg(u8 cmd, u8 arg)
{
	if (ps2_aux_command(cmd) != 0) {
		return (-1);
	}
	return (ps2_aux_command(arg));
}

static int
ps2_aux_get_id(u8 *id)
{
	if (ps2_aux_command(PS2_MOUSE_GET_ID) != 0) {
		return (-1);
	}
	return (ps2_read_aux(id));
}

static void
ps2_aux_enable_wheel(void)
{
	u8	id;

	id = 0;
	if (ps2_aux_command_arg(PS2_MOUSE_SAMPLE_RATE, 200) != 0 ||
	    ps2_aux_command_arg(PS2_MOUSE_SAMPLE_RATE, 100) != 0 ||
	    ps2_aux_command_arg(PS2_MOUSE_SAMPLE_RATE, 80) != 0 ||
	    ps2_aux_get_id(&id) != 0) {
		mouse_packet_size = 3;
		return;
	}
	if (id == 3) {
		mouse_packet_size = 4;
	}

	if (ps2_aux_command_arg(PS2_MOUSE_SAMPLE_RATE, 200) != 0 ||
	    ps2_aux_command_arg(PS2_MOUSE_SAMPLE_RATE, 200) != 0 ||
	    ps2_aux_command_arg(PS2_MOUSE_SAMPLE_RATE, 80) != 0 ||
	    ps2_aux_get_id(&id) != 0) {
		return;
	}
	if (id == 4) {
		mouse_packet_size = 4;
	}
}

static void
mouse_event_put(s32 dx, s32 dy, s32 dz, u32 buttons, u32 flags)
{
	struct mouse_event	*ev;
	int			next;

	mouse_x += dx;
	mouse_y += dy;

	next = (mouse_event_head + 1) % MOUSE_EVENT_RING_SIZE;
	if (next == mouse_event_tail) {
		return;
	}

	ev = &mouse_event_ring[mouse_event_head];
	ev->timestamp = timer_get_ticks();
	ev->x = mouse_x;
	ev->y = mouse_y;
	ev->dx = dx;
	ev->dy = dy;
	ev->dz = dz;
	ev->buttons = buttons;
	ev->flags = flags;

	mouse_event_head = next;
	knote_notify_all(EVFILT_MOUSE, 0, 0, 1);
}

static void
ps2_mouse_process_packet(void)
{
	s32	dx, dy, dz;
	u32	buttons, flags;
	u8	first, wheel;

	first = mouse_packet[0];
	flags = 0;
	if (first & 0xC0) {
		flags |= MOUSE_EVENT_OVERFLOW;
	}

	dx = (s32)mouse_packet[1];
	dy = (s32)mouse_packet[2];
	if (first & 0x10) {
		dx -= 256;
	}
	if (first & 0x20) {
		dy -= 256;
	}
	dy = -dy;

	dz = 0;
	if (mouse_packet_size == 4) {
		wheel = mouse_packet[3] & 0x0F;
		if (wheel & 0x08) {
			dz = (s32)wheel - 16;
		} else {
			dz = (s32)wheel;
		}
	}

	buttons = 0;
	if (first & 0x01) {
		buttons |= MOUSE_BUTTON_LEFT;
	}
	if (first & 0x02) {
		buttons |= MOUSE_BUTTON_RIGHT;
	}
	if (first & 0x04) {
		buttons |= MOUSE_BUTTON_MIDDLE;
	}
	if (mouse_packet_size == 4 && (mouse_packet[3] & 0x10)) {
		buttons |= MOUSE_BUTTON_X1;
	}
	if (mouse_packet_size == 4 && (mouse_packet[3] & 0x20)) {
		buttons |= MOUSE_BUTTON_X2;
	}

	if (dx != 0 || dy != 0) {
		flags |= MOUSE_EVENT_MOVE;
	}
	if (dz != 0) {
		flags |= MOUSE_EVENT_WHEEL;
	}
	if (buttons != mouse_buttons) {
		flags |= MOUSE_EVENT_BUTTON;
	}
	mouse_buttons = buttons;

	if (flags != 0) {
		mouse_event_put(dx, dy, dz, buttons, flags);
	}
}

static void
ps2_mouse_process_byte(u8 data)
{
	if (mouse_packet_index == 0 && (data & 0x08) == 0) {
		return;
	}

	mouse_packet[mouse_packet_index++] = data;
	if (mouse_packet_index < mouse_packet_size) {
		return;
	}

	ps2_mouse_process_packet();
	mouse_packet_index = 0;
}

static void
ps2_mouse_drain(const char *tag)
{
	u8	status, data;

	(void)tag;
	while (inb(PS2_STATUS_PORT) & PS2_STATUS_OBF) {
		status = inb(PS2_STATUS_PORT);
		if ((status & PS2_STATUS_AUX) == 0) {
			return;
		}
		data = inb(PS2_DATA_PORT);
		ps2_mouse_process_byte(data);
	}
}

int
ps2_mouse_init(void)
{
	u64	irq_flags;
	u8	config, resp;
	int	port1_disabled;
	int	ret;

	mouse_event_reset();
	mouse_ready = 0;
	mouse_packet_index = 0;
	mouse_packet_size = 3;
	mouse_x = 0;
	mouse_y = 0;
	mouse_buttons = 0;

	if (inb(PS2_STATUS_PORT) == 0xFF) {
		return (-1);
	}

	ret = -1;
	port1_disabled = 0;
	irq_flags = ps2_irq_save();

	if (ps2_write_cmd(PS2_CMD_DISABLE_PORT1) == 0) {
		port1_disabled = 1;
	}
	ps2_flush_output();
	if (ps2_write_cmd(PS2_CMD_ENABLE_PORT2) != 0) {
		drivers_log("[MOUSE] timeout enabling ps/2 port2\n");
		goto out;
	}

	ps2_flush_output();
	resp = 0xFF;
	if (ps2_write_cmd(PS2_CMD_TEST_PORT2) != 0 ||
	    ps2_read_data(&resp) != 0) {
		drivers_log("[MOUSE] ps/2 port2 test timeout, "
		    "probing aux\n");
	} else if (resp != 0x00) {
		drivers_log("[MOUSE] ps/2 port2 test failed: 0x%x\n",
		    resp);
		goto out;
	}

	ps2_flush_output();
	if (ps2_write_cmd(PS2_CMD_READ_CONFIG) != 0 ||
	    ps2_read_data(&config) != 0) {
		drivers_log("[MOUSE] timeout reading ps/2 config\n");
		goto out;
	}
	config |= PS2_CONFIG_PORT1_IRQ;
	config |= PS2_CONFIG_PORT2_IRQ;
	config |= PS2_CONFIG_TRANSLATION;
	config &= ~PS2_CONFIG_PORT1_CLOCK;
	config &= ~PS2_CONFIG_PORT2_CLOCK;
	if (ps2_write_cmd(PS2_CMD_WRITE_CONFIG) != 0 ||
	    ps2_write_data(config) != 0) {
		drivers_log("[MOUSE] timeout writing ps/2 config\n");
		goto out;
	}

	ps2_flush_output();
	if (ps2_aux_command(PS2_MOUSE_RESET) != 0 ||
	    ps2_read_aux(&resp) != 0 || resp != 0xAA ||
	    ps2_read_aux(&resp) != 0) {
		drivers_log("[MOUSE] ps/2 mouse reset failed\n");
		goto out;
	}

	if (ps2_aux_command(PS2_MOUSE_DISABLE) != 0 ||
	    ps2_aux_command(PS2_MOUSE_DEFAULTS) != 0) {
		drivers_log("[MOUSE] ps/2 mouse defaults failed\n");
		goto out;
	}

	ps2_aux_enable_wheel();

	if (ps2_aux_command(PS2_MOUSE_ENABLE) != 0) {
		drivers_log("[MOUSE] ps/2 mouse enable failed\n");
		goto out;
	}

	mouse_ready = 1;
	drivers_log("[MOUSE] PS/2 mouse ready (%d-byte packets)\n",
	    mouse_packet_size);
	ret = 0;

out:
	if (port1_disabled) {
		(void)ps2_write_cmd(PS2_CMD_ENABLE_PORT1);
	}
	ps2_irq_restore(irq_flags);
	return (ret);
}

void
ps2_mouse_handler(void)
{
	if (!mouse_ready) {
		return;
	}
	ps2_mouse_drain("irq");
}

void
ps2_mouse_poll(void)
{
	u8	status;

	if (!mouse_ready) {
		return;
	}

	status = inb(PS2_STATUS_PORT);
	if ((status & (PS2_STATUS_OBF | PS2_STATUS_AUX)) ==
	    (PS2_STATUS_OBF | PS2_STATUS_AUX)) {
		ps2_mouse_drain("poll");
	}
}

int
ps2_mouse_is_ready(void)
{
	return (mouse_ready);
}

int
mouse_event_get(struct mouse_event *out)
{
	struct mouse_event	*ev;

	if (mouse_event_head == mouse_event_tail) {
		return (0);
	}

	ev = &mouse_event_ring[mouse_event_tail];
	if (out) {
		*out = *ev;
	}
	mouse_event_tail = (mouse_event_tail + 1) %
	    MOUSE_EVENT_RING_SIZE;
	return (1);
}

int
mouse_event_count(void)
{
	int	delta;

	delta = mouse_event_head - mouse_event_tail;
	if (delta < 0) {
		delta += MOUSE_EVENT_RING_SIZE;
	}
	return (delta);
}

void
mouse_event_reset(void)
{
	mouse_event_head = 0;
	mouse_event_tail = 0;
	memset(mouse_event_ring, 0, sizeof(mouse_event_ring));
}
