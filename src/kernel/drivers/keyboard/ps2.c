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
$define %type int as 32 bit signed
$define %type char as 8 bit signed

$define %func ps2_debug_status as procedure with args const char *, u8, u8
$define %func ps2_update_leds_locked as function with args void
$define %func ps2_update_leds as function with args void
$define %func ps2_keyboard_init_locked as function with args void
$define %func ps2_keyboard_init as function with args void
$define %func ps2_keyboard_sink as procedure with args u8
$define %func buffer_write as procedure with args char
$define %func ps2_mods as function with args void
$define %func ps2_update_modifier as procedure with args u16, int
$define %func ps2_keyboard_reset_state as procedure with args void
$define %func ps2_keyboard_flush as procedure with args void
$define %func ps2_keyboard_getchar as function with args void
$define %func ps2_process_scancode as procedure with args u8
$define %func ps2_keyboard_handler as procedure with args void
$define %func ps2_keyboard_poll as procedure with args void
$define %func ps2_read_char_blocking as function with args void
$define %func ps2Scanf as function with args const char *, ...

*/

/* !SPACE!

$space %internal ps2_debug_status, ps2_update_leds
$space %internal ps2_update_leds_locked, ps2_keyboard_init_locked
$space %internal buffer_write, ps2_mods
$space %internal ps2_update_modifier, ps2_process_scancode
$space %internal ps2_keyboard_sink, ps2_read_char_blocking
$space %export ps2_keyboard_init, ps2_keyboard_handler
$space %export ps2_keyboard_getchar, ps2_keyboard_poll
$space %export ps2_keyboard_reset_state, ps2_keyboard_flush, ps2Scanf

*/

#include <kernel/drivers/input/i8042.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/keyboard/keymap.h>
#include <kernel/drivers/keyboard/ps2.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/kshell/kshell.h>
#include <kernel/drivers/power/power.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	PS2_KBD_CMD_SET_LEDS	0xED
#define	PS2_KBD_ACK		0xFA
#define	PS2_KBD_RESEND		0xFE
#define	PS2_LED_SCROLL		0x01
#define	PS2_LED_NUM		0x02
#define	PS2_LED_CAPS		0x04

#define	KB_BUFFER_SIZE		256

static char	kb_buffer[KB_BUFFER_SIZE];
static int	kb_head;
static int	kb_tail;
static int	lshift_pressed;
static int	rshift_pressed;
static int	lctrl_pressed;
static int	rctrl_pressed;
static int	lalt_pressed;
static int	ralt_pressed;
static int	caps_lock;
static int	caps_lock_down;
static int	ps2_ready;
static int	scancode_extended;
static int	ps2_debug;
static int	kshell_hotkey_latch;
static keyboard_driver_t atkbd_keyboard_driver;
static int	ps2_keyboard_init_locked(void);
static int	ps2_update_leds_locked(void);
static int	ps2_update_leds(void);
static void	ps2_keyboard_sink(u8 scancode);

static void
ps2_debug_status(const char *tag, u8 status, u8 data)
{
	if (!ps2_debug) {
		return;
	}
	drivers_log("[PS2] %s: status=0x%x data=0x%x\n", tag,
	    status, data);
}

static int
ps2_update_leds_locked(void)
{
	u8	led_mask;
	int	attempt;

	led_mask = 0;
	if (caps_lock) {
		led_mask |= PS2_LED_CAPS;
	}

	for (attempt = 0; attempt < 3; attempt++) {
		u8	resp;

		if (i8042_write_data(PS2_KBD_CMD_SET_LEDS) != 0) {
			return (-1);
		}
		if (i8042_read_data(&resp) != 0) {
			return (-1);
		}
		if (resp == PS2_KBD_RESEND) {
			continue;
		}
		if (resp != PS2_KBD_ACK) {
			return (-1);
		}

		if (i8042_write_data(led_mask) != 0) {
			return (-1);
		}
		if (i8042_read_data(&resp) != 0) {
			return (-1);
		}
		if (resp == PS2_KBD_RESEND) {
			continue;
		}
		if (resp != PS2_KBD_ACK) {
			return (-1);
		}
		return (0);
	}

	return (-1);
}

static int
ps2_update_leds(void)
{
	int	ret;

	i8042_cmd_begin();
	ret = ps2_update_leds_locked();
	i8042_cmd_end();
	return (ret);
}

static int
ps2_keyboard_init_locked(void)
{
	u8	config;

	kb_head = 0;
	kb_tail = 0;
	lshift_pressed = 0;
	rshift_pressed = 0;
	lctrl_pressed = 0;
	rctrl_pressed = 0;
	lalt_pressed = 0;
	ralt_pressed = 0;
	caps_lock = 0;
	caps_lock_down = 0;
	ps2_ready = 0;
	scancode_extended = 0;

	if (i8042_write_cmd(I8042_CMD_DISABLE_PORT1) != 0) {
		drivers_log("[PS2] timeout disabling port1\n");
		return (-1);
	}
	i8042_write_cmd(I8042_CMD_DISABLE_PORT2);
	i8042_flush_output();

	config = 0;
	if (i8042_read_config(&config) != 0) {
		drivers_log("[PS2] timeout waiting config\n");
		return (-1);
	}
	if (ps2_debug) {
		drivers_log("[PS2] config before: 0x%x\n", config);
	}

	config |= 0x01;
	config &= ~0x10;
	config |= 0x40;

	if (i8042_write_config(config) != 0) {
		drivers_log("[PS2] timeout writing config\n");
		return (-1);
	}

	if (i8042_read_config(&config) == 0) {
		u8	verify;

		verify = config;
		if (ps2_debug) {
			drivers_log("[PS2] config after: 0x%x\n", verify);
		}
	}

	if (i8042_write_cmd(I8042_CMD_SELF_TEST) == 0) {
		u8	self_test;

		self_test = 0;
		if (i8042_read_data(&self_test) == 0 &&
		    self_test != 0x55) {
			drivers_log("[PS2] controller self-test "
			    "failed: 0x%x\n", self_test);
		}
	}

	if (i8042_write_cmd(I8042_CMD_ENABLE_PORT1) != 0) {
		drivers_log("[PS2] timeout enabling "
		    "port1\n");
		return (-1);
	}

	if (i8042_write_data(0xFF) == 0) {
		u8	resp;

		resp = 0;
		if (i8042_read_data(&resp) == 0) {
			if (resp != 0xFA) {
				drivers_log("[PS2] reset ack "
				    "unexpected: 0x%x\n", resp);
			}
		} else {
				drivers_log("[PS2] reset ack "
				    "timeout\n");
			}
		if (i8042_read_data(&resp) == 0) {
			if (resp != 0xAA) {
				drivers_log("[PS2] reset self-test "
				    "failed: 0x%x\n", resp);
			}
		} else {
			drivers_log("[PS2] reset self-test "
			    "timeout\n");
		}
	}

	if (i8042_write_data(0xF4) == 0) {
		u8	resp;

		resp = 0;
		if (i8042_read_data(&resp) == 0) {
			if (resp != 0xFA) {
				drivers_log("[PS2] enable scan ack "
				    "unexpected: 0x%x\n", resp);
			}
		} else {
			drivers_log("[PS2] enable scan ack "
			    "timeout\n");
		}
	}

	(void)ps2_update_leds();

	ps2_ready = 1;
	return (0);
}
int
ps2_keyboard_init(void)
{
	int	ret;

	i8042_cmd_begin();
	ret = ps2_keyboard_init_locked();
	i8042_cmd_end();

	if (ret == 0) {
		i8042_set_kbd_sink(ps2_keyboard_sink);
	}
	return (ret);
}

static void
buffer_write(char c)
{
	int	next;

	next = (kb_head + 1) % KB_BUFFER_SIZE;
	if (next != kb_tail) {
		kb_buffer[kb_head] = c;
		kb_head = next;
	}
	keyboard_char_put(&atkbd_keyboard_driver, c);
}

static u32
ps2_mods(void)
{
	u32	mods;

	mods = 0;
	if (lshift_pressed) {
		mods |= MOD_LSHIFT;
	}
	if (rshift_pressed) {
		mods |= MOD_RSHIFT;
	}
	if (lctrl_pressed) {
		mods |= MOD_LCTRL;
	}
	if (rctrl_pressed) {
		mods |= MOD_RCTRL;
	}
	if (lalt_pressed) {
		mods |= MOD_LALT;
	}
	if (ralt_pressed) {
		mods |= MOD_RALT;
	}
	if (caps_lock) {
		mods |= MOD_CAPS;
	}
	return (mods);
}

static void
ps2_update_modifier(u16 key, int released)
{
	switch (key) {
	case KEY_LSHIFT:
		lshift_pressed = released ? 0 : 1;
		break;
	case KEY_RSHIFT:
		rshift_pressed = released ? 0 : 1;
		break;
	case KEY_LCTRL:
		lctrl_pressed = released ? 0 : 1;
		break;
	case KEY_RCTRL:
		rctrl_pressed = released ? 0 : 1;
		break;
	case KEY_LALT:
		lalt_pressed = released ? 0 : 1;
		break;
	case KEY_RALT:
		ralt_pressed = released ? 0 : 1;
		break;
	case KEY_CAPSLOCK:
		if (released) {
			caps_lock_down = 0;
		} else if (!caps_lock_down) {
			caps_lock = !caps_lock;
			caps_lock_down = 1;
			(void)ps2_update_leds();
		}
		break;
	default:
		break;
	}
}

void
ps2_keyboard_reset_state(void)
{
	kb_head = 0;
	kb_tail = 0;
	lshift_pressed = 0;
	rshift_pressed = 0;
	lctrl_pressed = 0;
	rctrl_pressed = 0;
	lalt_pressed = 0;
	ralt_pressed = 0;
	caps_lock_down = 0;
	scancode_extended = 0;
	kshell_hotkey_latch = 0;
}

void
ps2_keyboard_flush(void)
{
	kb_head = 0;
	kb_tail = 0;
}

char
ps2_keyboard_getchar(void)
{
	char	c;

	if (kb_head == kb_tail) {
		return (0);
	}
	c = kb_buffer[kb_tail];
	kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
	return (c);
}

static void
ps2_process_scancode(u8 scancode)
{
	u32	flags;
	u32	mods;
	u32	ch;
	u16	key;
	int	released;
	u8	code;
	int	extended;

	if (scancode == 0xFA || scancode == 0xFE) {
		return;
	}

	if (scancode == 0xE0) {
		scancode_extended = 1;
		return;
	}

	if (scancode == 0xE1) {
		scancode_extended = 2;
		return;
	}

	extended = scancode_extended != 0;
	released = (scancode & 0x80) != 0;
	code = scancode & 0x7F;
	key = keymap_ps2_set1(scancode, extended);
	scancode_extended = 0;

	if (key == KEY_NONE) {
		return;
	}

	ps2_update_modifier(key, released);
	if (!keyboard_driver_is_active(&atkbd_keyboard_driver)) {
		return;
	}
	keyboard_handle_scancode(code, released, extended);

	mods = ps2_mods();
	flags = released ? KEY_EVENT_RELEASE : KEY_EVENT_PRESS;
	if (extended) {
		flags |= KEY_EVENT_EXTENDED;
	}
	ch = released ? 0 : keymap_ascii(key, mods);
	kbd_event_put(key, code, flags, mods, ch);

	if (!released && (mods & MOD_CTRL) && (mods & MOD_ALT) &&
	    (mods & MOD_SHIFT) && key == KEY_Z) {
		drivers_log("[PS2] Ctrl+Alt+Shift+Z pressed, "
		    "rebooting...\n");
		power_controller_reboot();
	}

	if (!released && (mods & MOD_CTRL) && (mods & MOD_SHIFT) &&
	    key == KEY_BACKSPACE) {
		kshell_request_open();
		return;
	}

	if (ch != 0) {
		buffer_write((char)ch);
	}
}

static void
ps2_keyboard_sink(u8 scancode)
{
	if (!ps2_ready) {
		return;
	}
	ps2_debug_status("sink", i8042_status(), scancode);
	ps2_process_scancode(scancode);
}

void
ps2_keyboard_handler(void)
{
	(void)i8042_dispatch();
}

void
ps2_keyboard_poll(void)
{
	(void)i8042_dispatch();
}

static char
ps2_read_char_blocking(void)
{
	char	c;

	c = 0;
	while ((c = ps2_keyboard_getchar()) == 0) {
		__asm__ volatile("nop");
	}
	return (c);
}

int
ps2Scanf(const char *format, ...)
{
	__builtin_va_list	args;
	int			count;

	__builtin_va_start(args, format);
	count = 0;

	while (*format) {
		if (*format == '%') {
			format++;
			if (*format == 'd') {
				int	*val;
				char	buf[32];
				int	i;
				char	c;
				int	sign, num, started, is_neg;

				val = __builtin_va_arg(args, int *);
				i = 0;
				while ((c = ps2_read_char_blocking()) ==
				    ' ' || c == '\n' || c == '\t') {
				}
				sign = 1;
				if (c == '-') {
					ps2_read_char_blocking();
				}
				num = 0;
				started = 0;
				is_neg = 0;
				while (1) {
					c = ps2_read_char_blocking();
					if (c != '\b') {
					}
					if (c == ' ' || c == '\n' ||
					    c == '\t') {
						if (started) {
							break;
						}
						continue;
					}
					if (c == '-' && !started) {
						is_neg = 1;
						started = 1;
						continue;
					}
					if (c >= '0' && c <= '9') {
						num = num * 10 +
						    (c - '0');
						started = 1;
					} else {
						break;
					}
				}
				if (is_neg) {
					num = -num;
				}
				*val = num;
				count++;
			} else if (*format == 's') {
				char	*str;
				char	c;
				int	started;

				str = __builtin_va_arg(args, char *);
				started = 0;
				while (1) {
					c = ps2_read_char_blocking();
					if (c == ' ' || c == '\n' ||
					    c == '\t') {
						if (started) {
							*str = 0;
							break;
						}
						continue;
					}
					*str++ = c;
					started = 1;
				}
				count++;
			} else if (*format == 'c') {
				char	*ch;

				ch = __builtin_va_arg(args, char *);
				*ch = ps2_read_char_blocking();
				count++;
			} else if (*format == 'x') {
				int	*val;
				int	num, started, digit;
				char	c;

				val = __builtin_va_arg(args, int *);
				num = 0;
				started = 0;
				while (1) {
					c = ps2_read_char_blocking();
					if (c == ' ' || c == '\n' ||
					    c == '\t') {
						if (started) {
							break;
						}
						continue;
					}
					digit = -1;
					if (c >= '0' && c <= '9') {
						digit = c - '0';
					} else if (c >= 'a' &&
					    c <= 'f') {
						digit = c - 'a' + 10;
					} else if (c >= 'A' &&
					    c <= 'F') {
						digit = c - 'A' + 10;
					}
					if (digit != -1) {
						num = num * 16 + digit;
						started = 1;
					} else {
						break;
					}
				}
				*val = num;
				count++;
			}
		} else {
		}
		format++;
	}

	__builtin_va_end(args);
	return (count);
}

static keyboard_driver_t atkbd_keyboard_driver = {
	.name		= "PS/2 Keyboard",
	.init		= ps2_keyboard_init,
	.getchar	= ps2_keyboard_getchar,
	.handler	= ps2_keyboard_handler,
	.poll		= ps2_keyboard_poll,
	.flush		= ps2_keyboard_flush,
	.reset		= ps2_keyboard_reset_state,
};

static int
atkbd_intr(void *arg)
{
	(void)arg;
	keyboard_driver_handler(&atkbd_keyboard_driver);
	return (0);
}

typedef struct atkbd_softc {
	resource_t	*irq;
	void		*irq_cookie;
} atkbd_softc_t;

static void
atkbd_poll(void *arg)
{
	(void)arg;
	keyboard_driver_handler(&atkbd_keyboard_driver);
}

static int
atkbd_probe(device_t dev)
{
	(void)dev;
	if (i8042_status() == 0xFF) {
		return (-1);
	}
	return (0);
}

static int
atkbd_attach(device_t dev)
{
	resource_t	*irq;
	atkbd_softc_t	*softc;
	int		irq_ok, rid;

	if (keyboard_register_driver(&atkbd_keyboard_driver) != 0) {
		return (-1);
	}
	softc = kmem_calloc(1, sizeof(*softc));
	if (softc == NULL) {
		return (-1);
	}
	device_set_softc(dev, softc);
	irq_ok = 0;
	rid = 0;
	irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (irq != NULL &&
	    bus_setup_intr(dev, irq, atkbd_intr, NULL,
	    &softc->irq_cookie) == 0) {
		irq_ok = 1;
		softc->irq = irq;
	}
	if (!irq_ok) {
		if (irq != NULL) {
			bus_release_resource(dev, SYS_RES_IRQ, irq->rid, irq);
		}
		kmem_free(softc);
		device_set_softc(dev, NULL);
		return (-1);
	}
	return (0);
}

static int
atkbd_detach(device_t dev)
{
	atkbd_softc_t	*softc;

	softc = device_get_softc(dev);
	if (softc != NULL && softc->irq_cookie != NULL) {
		bus_teardown_intr(dev, softc->irq, softc->irq_cookie);
	}
	if (softc != NULL && softc->irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, softc->irq->rid,
		    softc->irq);
	}
	kmem_free(softc);
	device_set_softc(dev, NULL);
	return (0);
}

static devclass_t atkbd_devclass = {
	.name		= "keyboard",
	.maxunit	= 1,
};

static driver_t atkbd_driver = {
	.name		= "atkbd",
	.identify	= NULL,
	.probe		= atkbd_probe,
	.attach		= atkbd_attach,
	.detach		= atkbd_detach,
};

DRIVER_MODULE(atkbd, i8042, atkbd_driver, atkbd_devclass,
    NEWBUS_PASS_INPUT, NEWBUS_ORDER_MIDDLE);
