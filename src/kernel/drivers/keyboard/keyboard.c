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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type keyboard_driver_t as struct with driver name and function pointers
$define %type keyboard_scancode_callback_t as function pointer for raw scancode events
$define %type kbd_event as struct with normalized keyboard input event

$define %func keyboard_manager_init as procedure with args void
$define %func keyboard_register_driver as function with args keyboard_driver_t *
$define %func keyboard_getchar as function with args void
$define %func keyboard_getchar_blocking as function with args void
$define %func keyboard_common_handler as procedure with args void
$define %func keyboard_poll as procedure with args void
$define %func keyboard_reset_state as procedure with args void
$define %func keyboard_flush_chars as procedure with args void
$define %func keyboard_flush_input as procedure with args void
$define %func keyboard_start_direct_input as procedure with args void
$define %func keyboard_stop_direct_input as procedure with args void
$define %func scanf as function with args const char *, ...
$define %func keyboard_set_scancode_callback as procedure with args keyboard_scancode_callback_t
$define %func keyboard_handle_scancode as procedure with args u8, int, int
$define %func keyboard_get_driver_name as function with args void
$define %func keyboard_driver_is_active as function with args keyboard_driver_t *
$define %func keyboard_driver_handler as procedure with args keyboard_driver_t *
$define %func keyboard_input_settle as procedure with args void
$define %func keyboard_cm_update as function with args u32
$define %func keyboard_char_put as procedure with args keyboard_driver_t *, char
$define %func keyboard_char_get as function with args void
$define %func keyboard_char_reset as procedure with args void
$define %func keyboard_poll_drivers as procedure with args void
$define %func keyboard_driver_priority as function with args keyboard_driver_t *
$define %func keyboard_driver_matches as function with args keyboard_driver_t *, const char *
$define %func keyboard_find_preferred as function with args const char *
$define %func kbd_event_put as procedure with args u16, u16, u32, u32, u32
$define %func kbd_event_get as function with args struct kbd_event *
$define %func kbd_event_count as function with args void
$define %func kbd_event_reset as procedure with args void

*/

/* !SPACE!

$space %export keyboard_manager_init, keyboard_getchar
$space %export keyboard_register_driver
$space %export keyboard_switch_driver
$space %export keyboard_getchar_blocking, keyboard_common_handler
$space %export keyboard_poll, keyboard_reset_state
$space %export keyboard_flush_chars, keyboard_flush_input
$space %export keyboard_start_direct_input, keyboard_stop_direct_input, scanf
$space %export keyboard_set_scancode_callback, keyboard_handle_scancode
$space %export keyboard_get_driver_name
$space %export keyboard_driver_is_active, keyboard_driver_handler
$space %export keyboard_input_settle
$space %export keyboard_cm_update
$space %internal keyboard_driver_priority, keyboard_driver_matches
$space %internal keyboard_find_preferred
$space %export keyboard_char_put
$space %internal keyboard_char_get, keyboard_char_reset, keyboard_poll_drivers
$space %export kbd_event_put, kbd_event_get, kbd_event_count, kbd_event_reset

*/

#include <kernel/api/errno.h>
#include <kernel/api/posix/posix.h>
#include <kernel/cm/cm.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/input/input.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/drivers/timer.h>
#include <kernel/event/event.h>
#include <kernel/kshell/kshell.h>
#include <kernel/process.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	KBD_STATUS_PORT	0x64
#define	KEYBOARD_MAX_DRIVERS	8
#define	KEYBOARD_PREFERENCE_LEN	16
#define	KEYBOARD_CHAR_RING_SIZE	256

static keyboard_driver_t			*current_driver;
static keyboard_driver_t			*keyboard_drivers[
    KEYBOARD_MAX_DRIVERS];
static int					keyboard_driver_count;
static int					keyboard_initialized;
static keyboard_scancode_callback_t		scancode_callback;
static volatile int				direct_input_depth;
static char				keyboard_preferred_driver[
    KEYBOARD_PREFERENCE_LEN] = "auto";

static struct kbd_event	kbd_event_ring[KBD_EVENT_RING_SIZE];
static int			kbd_event_head;
static int			kbd_event_tail;
static char			keyboard_char_ring[KEYBOARD_CHAR_RING_SIZE];
static int			keyboard_char_head;
static int			keyboard_char_tail;

static char	keyboard_char_get(void);
static void	keyboard_char_reset(void);
static void	keyboard_poll_drivers(void);

static int
keyboard_driver_priority(keyboard_driver_t *driver)
{
	if (!driver || !driver->name) {
		return (-1);
	}
	if (strcmp(driver->name, "USB Keyboard") == 0) {
		return (2);
	}
	if (strcmp(driver->name, "PS/2 Keyboard") == 0) {
		return (1);
	}
	return (0);
}

static int
keyboard_driver_matches(keyboard_driver_t *driver, const char *preference)
{
	if (!driver || !driver->name || !preference) {
		return (0);
	}
	if (strcmp(preference, "usb") == 0) {
		return (strcmp(driver->name, "USB Keyboard") == 0);
	}
	if (strcmp(preference, "ps2") == 0) {
		return (strcmp(driver->name, "PS/2 Keyboard") == 0);
	}
	return (0);
}

static keyboard_driver_t *
keyboard_find_preferred(const char *preference)
{
	keyboard_driver_t	*best;
	int			 best_priority;
	int			 i;
	int			 priority;

	best = NULL;
	best_priority = -1;
	for (i = 0; i < keyboard_driver_count; i++) {
		if (strcmp(preference, "auto") != 0) {
			if (keyboard_driver_matches(keyboard_drivers[i], preference)) {
				return (keyboard_drivers[i]);
			}
			continue;
		}
		priority = keyboard_driver_priority(keyboard_drivers[i]);
		if (priority > best_priority) {
			best = keyboard_drivers[i];
			best_priority = priority;
		}
	}
	return (best);
}

void
keyboard_manager_init(void)
{
	if (keyboard_initialized) {
		return;
	}
	keyboard_driver_count = 0;
	current_driver = NULL;
	keyboard_char_reset();
	kbd_event_reset();
	cm_register_consumer(CM_CONSUMER_INPUT, "input", keyboard_cm_update);
	keyboard_initialized = 1;
	drivers_log("[KEYBOARD] manager initialized\n");
}

int
keyboard_register_driver(keyboard_driver_t *driver)
{
	int	i;

	if (driver == NULL || driver->name == NULL) {
		return (-1);
	}
	if (!keyboard_initialized) {
		keyboard_manager_init();
	}
	for (i = 0; i < keyboard_driver_count; i++) {
		if (keyboard_drivers[i] == driver) {
			return (0);
		}
	}
	if (keyboard_driver_count >= KEYBOARD_MAX_DRIVERS) {
		drivers_log("[KEYBOARD] driver limit reached (%d)\n",
		    KEYBOARD_MAX_DRIVERS);
		return (-1);
	}
	if (driver->init != NULL && driver->init() != 0) {
		drivers_log("[KEYBOARD] %s init failed, not registered\n",
		    (char *)driver->name);
		return (-1);
	}
	keyboard_drivers[keyboard_driver_count++] = driver;
	if (current_driver == NULL) {
		current_driver = driver;
	}

	drivers_log("[KEYBOARD] detected: %s\n",
	    (char *)driver->name);
	return (0);
}

int
keyboard_switch_driver(keyboard_driver_t *driver)
{
	int	i;

	if (driver == NULL || driver->name == NULL) {
		return (-1);
	}
	if (current_driver == driver) {
		return (0);
	}
	for (i = 0; i < keyboard_driver_count; i++) {
		if (keyboard_drivers[i] == driver) {
			break;
		}
	}
	if (i == keyboard_driver_count) {
		return (-1);
	}
	current_driver = driver;
	drivers_log("[KEYBOARD] switch to driver: %s\n",
	    (char *)current_driver->name);
	return (0);
}

int
keyboard_cm_update(u32 flags)
{
	keyboard_driver_t	*driver;
	char			 preference[KEYBOARD_PREFERENCE_LEN];

	(void)flags;
	if (!cm_is_initialized()) {
		return (-API_ERR_NOT_FOUND);
	}
	(void)cm_get_string_default("SYSTEM", "Input.Keyboard",
	    "PreferredDriver", preference, sizeof(preference), "auto");
	if (strcmp(preference, "auto") != 0 &&
	    strcmp(preference, "ps2") != 0 &&
	    strcmp(preference, "usb") != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	strcpy(keyboard_preferred_driver, preference);
	driver = keyboard_find_preferred(preference);
	if (driver == NULL) {
		drivers_log("[KEYBOARD] preferred driver %s is not present\n",
		    preference);
		return (0);
	}
	(void)keyboard_switch_driver(driver);
	return (0);
}

static void
keyboard_poll_drivers(void)
{
	int	i;

	for (i = 0; i < keyboard_driver_count; i++) {
		if (keyboard_drivers[i]->poll == NULL ||
		    !keyboard_driver_is_active(keyboard_drivers[i])) {
			continue;
		}
		keyboard_drivers[i]->poll();
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

	keyboard_poll_drivers();
	c = keyboard_char_get();

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
keyboard_input_settle(void)
{
	void	*ch;

	if (direct_input_depth == 0) {
		terminal_input_poll();
	}

	ch = terminal_get_input_channel();
	if (ch) {
		proc_wakeup(ch);
	}
	posix_poll_notify();
}

void
keyboard_driver_handler(keyboard_driver_t *driver)
{
	if (!driver || !driver->handler) {
		return;
	}
	driver->handler();
	if (!keyboard_driver_is_active(driver)) {
		return;
	}
	keyboard_input_settle();
}

void
keyboard_common_handler(void)
{
	keyboard_driver_handler(current_driver);
}

void
keyboard_poll(void)
{
	if (keyboard_driver_count == 0) {
		return;
	}
	keyboard_poll_drivers();
	keyboard_input_settle();
}

void
keyboard_reset_state(void)
{
	int	i;

	for (i = 0; i < keyboard_driver_count; i++) {
		if (keyboard_drivers[i]->reset != NULL) {
			keyboard_drivers[i]->reset();
		}
	}
}

void
keyboard_flush_chars(void)
{
	int	i;

	keyboard_char_reset();
	for (i = 0; i < keyboard_driver_count; i++) {
		if (keyboard_drivers[i]->flush != NULL) {
			keyboard_drivers[i]->flush();
		}
	}
}

void
keyboard_flush_input(void)
{
	keyboard_flush_chars();
	kbd_event_reset();
}

void
keyboard_start_direct_input(void)
{
	direct_input_depth++;
}

void
keyboard_stop_direct_input(void)
{
	if (direct_input_depth > 0) {
		direct_input_depth--;
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

int
keyboard_driver_is_active(keyboard_driver_t *driver)
{
	if (driver == NULL) {
		return (0);
	}
	if (strcmp(keyboard_preferred_driver, "auto") == 0) {
		return (1);
	}
	return (keyboard_driver_matches(driver, keyboard_preferred_driver));
}

void
keyboard_char_put(keyboard_driver_t *driver, char c)
{
	int	next;

	if (c == 0) {
		return;
	}
	next = (keyboard_char_head + 1) % KEYBOARD_CHAR_RING_SIZE;
	if (next == keyboard_char_tail) {
		return;
	}
	keyboard_char_ring[keyboard_char_head] = c;
	keyboard_char_head = next;
	if (driver != NULL) {
		current_driver = driver;
	}
}

static char
keyboard_char_get(void)
{
	char	c;

	if (keyboard_char_head == keyboard_char_tail) {
		return (0);
	}
	c = keyboard_char_ring[keyboard_char_tail];
	keyboard_char_tail = (keyboard_char_tail + 1) % KEYBOARD_CHAR_RING_SIZE;
	return (c);
}

static void
keyboard_char_reset(void)
{
	keyboard_char_head = 0;
	keyboard_char_tail = 0;
}

void
kbd_event_put(u16 key, u16 raw, u32 flags, u32 mods, u32 ch)
{
	struct kbd_event	*ev;
	u64			timestamp;
	int			next;

	timestamp = timer_get_ticks();
	next = (kbd_event_head + 1) % KBD_EVENT_RING_SIZE;
	if (next == kbd_event_tail) {
		kbd_event_tail = (kbd_event_tail + 1) % KBD_EVENT_RING_SIZE;
	}

	ev = &kbd_event_ring[kbd_event_head];
	ev->timestamp = timestamp;
	ev->key = key;
	ev->raw = raw;
	ev->flags = flags;
	ev->mods = mods;
	ev->ch = ch;
	kbd_event_head = next;

	input_event_keyboard(timestamp, key, raw, flags, mods, ch);
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

static void
keyboard_core_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "keyboard_core", 0) == NULL) {
		device_add_child(parent, "keyboard_core", 0);
	}
}

static int
keyboard_core_attach(device_t dev)
{
	(void)dev;
	keyboard_manager_init();
	return (0);
}

static devclass_t keyboard_core_devclass = {
	.name		= "keyboard",
	.maxunit	= 1,
};

static driver_t keyboard_core_driver = {
	.name		= "keyboard_core",
	.identify	= keyboard_core_identify,
	.probe		= NULL,
	.attach		= keyboard_core_attach,
};

PSEUDO_DRIVER_MODULE(keyboard_core, keyboard_core_driver,
    keyboard_core_devclass, NEWBUS_PASS_CORE, NEWBUS_ORDER_MIDDLE);
