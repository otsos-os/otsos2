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
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type keyboard_driver_t as struct with driver name and function pointers
$define %type keyboard_scancode_callback_t as function pointer for raw scancode events

$define %func keyboard_manager_init as procedure with args void
$define %func keyboard_getchar as function with args void
$define %func keyboard_getchar_blocking as function with args void
$define %func keyboard_common_handler as procedure with args void
$define %func keyboard_poll as procedure with args void
$define %func keyboard_reset_state as procedure with args void
$define %func scanf as function with args const char *, ...
$define %func keyboard_set_scancode_callback as procedure with args keyboard_scancode_callback_t
$define %func keyboard_handle_scancode as procedure with args u8, int, int
$define %func keyboard_get_driver_name as function with args void

*/

/* !SPACE!

$space %export keyboard_manager_init, keyboard_getchar
$space %export keyboard_getchar_blocking, keyboard_common_handler
$space %export keyboard_poll, keyboard_reset_state, scanf
$space %export keyboard_set_scancode_callback, keyboard_handle_scancode
$space %export keyboard_get_driver_name

*/

#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/keyboard/ps2.h>
#include <kernel/drivers/timer.h>
#include <kernel/event/event.h>
#include <kernel/kshell/kshell.h>
#include <kernel/process.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define	KBD_STATUS_PORT	0x64

static keyboard_driver_t			*current_driver;
static keyboard_scancode_callback_t		scancode_callback;

static struct kbd_event	kbd_event_ring[KBD_EVENT_RING_SIZE];
static int			kbd_event_head;
static int			kbd_event_tail;

static keyboard_driver_t ps2_driver = {
	.name		= "PS/2 Keyboard",
	.init		= ps2_keyboard_init,
	.getchar	= ps2_keyboard_getchar,
	.handler	= ps2_keyboard_handler,
	.poll		= ps2_keyboard_poll,
};

void
keyboard_manager_init(void)
{
	u8	status;

	status = inb(KBD_STATUS_PORT);
	if (status == 0xFF) {
		com1_write_string("[KEYBOARD] no ps/2 detected "
		    "(Status 0xFF).\n");
		return;
	}

	current_driver = &ps2_driver;

	com1_write_string("[KEYBOARD] detected: ");
	com1_write_string((char *)current_driver->name);
	com1_write_string("\n");

	com1_write_string("[KEYBOARD] switch to driver: ");
	com1_write_string((char *)current_driver->name);
	com1_write_string("\n");

	if (current_driver->init) {
		if (current_driver->init() != 0) {
			com1_write_string("[KEYBOARD] init failed, "
			    "driver disabled.\n");
			current_driver = NULL;
		}
	}
}

char
keyboard_getchar(void)
{
	char	c;

	c = 0;

	if (kshell_try_open_if_requested()) {
		return (0);
	}

	if (current_driver) {
		if (current_driver->poll) {
			current_driver->poll();
		}
		if (current_driver->getchar) {
			c = current_driver->getchar();
		}
	}

	if (kshell_try_open_if_requested()) {
		return (0);
	}

	return (c);
}

char
keyboard_getchar_blocking(void)
{
	char	c;

	c = 0;
	while ((c = keyboard_getchar()) == 0) {
		if (kshell_try_open_if_requested()) {
			continue;
		}
		__asm__ volatile("hlt");
	}
	return (c);
}

void
keyboard_common_handler(void)
{
	if (!current_driver || !current_driver->handler) {
		return;
	}

	current_driver->handler();

	/* IRQ1 means a key was pressed — wake tty sleepers. */
	extern void *tty_get_input_channel(void);
	void	*ch;

	ch = tty_get_input_channel();
	if (ch) {
		proc_wakeup(ch);
	}
}

void
keyboard_poll(void)
{
	u8	status;

	if (!current_driver || !current_driver->poll) {
		return;
	}

	status = inb(0x64);
	if (!(status & 0x01)) {
		return;
	}

	current_driver->poll();

	/* Data was drained — wake any process sleeping on tty input. */
	extern void *tty_get_input_channel(void);
	void	*ch;

	ch = tty_get_input_channel();
	if (ch) {
		proc_wakeup(ch);
	}
}

void
keyboard_reset_state(void)
{
	if (current_driver == &ps2_driver) {
		ps2_keyboard_reset_state();
	}
}

void
keyboard_set_scancode_callback(keyboard_scancode_callback_t cb)
{
	scancode_callback = cb;
}

void
keyboard_handle_scancode(u8 scancode, int released, int extended)
{
	if (scancode_callback) {
		scancode_callback(scancode, released, extended);
	}
}

const char *
keyboard_get_driver_name(void)
{
	if (!current_driver) {
		return (NULL);
	}
	return (current_driver->name);
}

void
kbd_event_put(u16 scancode, u8 released, u8 extended, char ascii)
{
	struct kbd_event	*ev;
	int			next;

	next = (kbd_event_head + 1) % KBD_EVENT_RING_SIZE;
	if (next == kbd_event_tail) {
		return;
	}

	ev = &kbd_event_ring[kbd_event_head];
	ev->timestamp = timer_get_ticks();
	ev->scancode = scancode;
	ev->released = released;
	ev->extended = extended;
	ev->ascii = ascii;

	kbd_event_head = next;

	extern void knote_notify_all(s16 filter, u64 ident, u32 fflags,
	    s64 data);
	knote_notify_all(EVFILT_KBD, 0, 0, 1);
}

int
kbd_event_get(struct kbd_event *out)
{
	struct kbd_event	*ev;

	if (kbd_event_head == kbd_event_tail) {
		return (0);
	}

	ev = &kbd_event_ring[kbd_event_tail];
	if (out) {
		*out = *ev;
	}
	kbd_event_tail = (kbd_event_tail + 1) % KBD_EVENT_RING_SIZE;
	return (1);
}

int
kbd_event_count(void)
{
	int	delta;

	delta = kbd_event_head - kbd_event_tail;
	if (delta < 0) {
		delta += KBD_EVENT_RING_SIZE;
	}
	return (delta);
}

void
kbd_event_reset(void)
{
	kbd_event_head = 0;
	kbd_event_tail = 0;
	memset(kbd_event_ring, 0, sizeof(kbd_event_ring));
}

static char
helper_read_char(void)
{
	char	c;

	c = 0;
	while ((c = keyboard_getchar()) == 0) {
	}
	return (c);
}

int
scanf(const char *format, ...)
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
				while ((c = helper_read_char()) == ' ' ||
				    c == '\n' || c == '\t') {
				}
				sign = 1;
				if (c == '-') {
					helper_read_char();
				}
				num = 0;
				started = 0;
				is_neg = 0;
				while (1) {
					c = helper_read_char();
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
						num = num * 10 + (c - '0');
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
					c = helper_read_char();
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
				*ch = helper_read_char();
				count++;
			} else if (*format == 'x') {
				int	*val;
				int	num, started, digit;
				char	c;

				val = __builtin_va_arg(args, int *);
				num = 0;
				started = 0;
				while (1) {
					c = helper_read_char();
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
					} else if (c >= 'a' && c <= 'f') {
						digit = c - 'a' + 10;
					} else if (c >= 'A' && c <= 'F') {
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
