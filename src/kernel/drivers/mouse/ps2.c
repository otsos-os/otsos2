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

$define %func ps2_aux_command as function with args u8
$define %func ps2_aux_command_arg as function with args u8, u8
$define %func ps2_aux_get_id as function with args u8 *
$define %func ps2_aux_enable_wheel as procedure with args void
$define %func mouse_input_put as procedure with args s32, s32, s32, u32, u32
$define %func ps2_mouse_process_packet as procedure with args void
$define %func ps2_mouse_process_byte as procedure with args u8
$define %func ps2_mouse_drain as procedure with args const char *
$define %func ps2_mouse_init as function with args void
$define %func ps2_mouse_handler as procedure with args void
$define %func ps2_mouse_poll as procedure with args void
$define %func ps2_mouse_is_ready as function with args void

*/

/* !SPACE!

$space %internal ps2_aux_command, ps2_aux_command_arg, ps2_aux_get_id
$space %internal ps2_aux_enable_wheel, mouse_input_put
$space %internal ps2_mouse_process_packet, ps2_mouse_process_byte
$space %internal ps2_mouse_drain
$space %export ps2_mouse_init, ps2_mouse_handler, ps2_mouse_poll
$space %export ps2_mouse_is_ready

*/

#include <kernel/drivers/input/i8042.h>
#include <kernel/drivers/input/input.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/timer.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	PS2_MOUSE_ACK		0xFA
#define	PS2_MOUSE_RESEND	0xFE
#define	PS2_MOUSE_RESET		0xFF
#define	PS2_MOUSE_DEFAULTS	0xF6
#define	PS2_MOUSE_DISABLE	0xF5
#define	PS2_MOUSE_ENABLE	0xF4
#define	PS2_MOUSE_SAMPLE_RATE	0xF3
#define	PS2_MOUSE_GET_ID	0xF2

static int			mouse_ready;
static int			mouse_packet_index;
static int			mouse_packet_size;
static u8			mouse_packet[4];
static s32			mouse_x;
static s32			mouse_y;
static u32			mouse_buttons;

static int
ps2_aux_command(u8 cmd)
{
	u8	resp;
	int	attempt;

	for (attempt = 0; attempt < 3; attempt++) {
		if (i8042_write_aux(cmd) != 0) {
			return (-1);
		}
		if (i8042_read_aux(&resp) != 0) {
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
	return (i8042_read_aux(id));
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
mouse_input_put(s32 dx, s32 dy, s32 dz, u32 buttons, u32 flags)
{
	u64			irq_flags;
	u64			timestamp;
	s32			x, y;

	irq_flags = i8042_irq_save();
	mouse_x += dx;
	mouse_y += dy;
	x = mouse_x;
	y = mouse_y;
	timestamp = timer_get_ticks();
	i8042_irq_restore(irq_flags);

	input_event_mouse(timestamp, x, y, dx, dy, dz, buttons, flags);
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
		mouse_input_put(dx, dy, dz, buttons, flags);
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
	while (i8042_status() & I8042_STATUS_OBF) {
		status = i8042_status();
		if ((status & I8042_STATUS_AUX) == 0) {
			return;
		}
		if (i8042_read_data(&data) != 0) {
			return;
		}
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

	mouse_ready = 0;
	mouse_packet_index = 0;
	mouse_packet_size = 3;
	mouse_x = 0;
	mouse_y = 0;
	mouse_buttons = 0;

	if (i8042_status() == 0xFF) {
		return (-1);
	}

	ret = -1;
	port1_disabled = 0;
	irq_flags = i8042_irq_save();

	if (i8042_write_cmd(I8042_CMD_DISABLE_PORT1) == 0) {
		port1_disabled = 1;
	}
	i8042_flush_output();
	if (i8042_write_cmd(I8042_CMD_ENABLE_PORT2) != 0) {
		drivers_log("[MOUSE] timeout enabling ps/2 port2\n");
		goto out;
	}

	i8042_flush_output();
	resp = 0xFF;
	if (i8042_test_port2(&resp) != 0) {
		drivers_log("[MOUSE] ps/2 port2 test timeout, "
		    "probing aux\n");
	} else if (resp != 0x00) {
		drivers_log("[MOUSE] ps/2 port2 test failed: 0x%x\n",
		    resp);
		goto out;
	}

	i8042_flush_output();
	if (i8042_read_config(&config) != 0) {
		drivers_log("[MOUSE] timeout reading ps/2 config\n");
		goto out;
	}
	config |= I8042_CONFIG_PORT1_IRQ;
	config |= I8042_CONFIG_PORT2_IRQ;
	config |= I8042_CONFIG_TRANSLATION;
	config &= ~I8042_CONFIG_PORT1_CLOCK;
	config &= ~I8042_CONFIG_PORT2_CLOCK;
	if (i8042_write_config(config) != 0) {
		drivers_log("[MOUSE] timeout writing ps/2 config\n");
		goto out;
	}

	i8042_flush_output();
	if (ps2_aux_command(PS2_MOUSE_RESET) != 0 ||
	    i8042_read_aux(&resp) != 0 || resp != 0xAA ||
	    i8042_read_aux(&resp) != 0) {
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
		(void)i8042_write_cmd(I8042_CMD_ENABLE_PORT1);
	}
	i8042_irq_restore(irq_flags);
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

	status = i8042_status();
	if ((status & (I8042_STATUS_OBF | I8042_STATUS_AUX)) ==
	    (I8042_STATUS_OBF | I8042_STATUS_AUX)) {
		ps2_mouse_drain("poll");
	}
}

int
ps2_mouse_is_ready(void)
{
	return (mouse_ready);
}
