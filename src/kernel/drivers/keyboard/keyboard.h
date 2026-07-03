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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <mlibc/mlibc.h>

typedef int	(*keyboard_init_fn)(void);
typedef char	(*keyboard_getchar_fn)(void);
typedef void	(*keyboard_handler_fn)(void);
typedef void	(*keyboard_poll_fn)(void);

typedef struct {
	const char		*name;
	keyboard_init_fn	 init;
	keyboard_getchar_fn	 getchar;
	keyboard_handler_fn	 handler;
	keyboard_poll_fn	 poll;
} keyboard_driver_t;

typedef void (*keyboard_scancode_callback_t)(u8 scancode, int released,
    int extended);

/* Raw keyboard event for kqueue/EVFILT_KBD consumers.
 * Kept separate from the translated ASCII buffer so TTY and raw input
 * can coexist without stealing each other's data. */
struct kbd_event {
	u64	timestamp;
	u16	scancode;
	u8	released;
	u8	extended;
	char	ascii;
};

#define	KBD_EVENT_RING_SIZE	256

void		keyboard_manager_init(void);
char		keyboard_getchar(void);
char		keyboard_getchar_blocking(void);
void		keyboard_common_handler(void);
void		keyboard_poll(void);
void		keyboard_reset_state(void);
int		scanf(const char *format, ...);
void		keyboard_set_scancode_callback(
		    keyboard_scancode_callback_t cb);
void		keyboard_handle_scancode(u8 scancode, int released,
		    int extended);
const char	*keyboard_get_driver_name(void);

void		kbd_event_put(u16 scancode, u8 released, u8 extended,
		    char ascii);
int		kbd_event_get(struct kbd_event *out);
int		kbd_event_count(void);
void		kbd_event_reset(void);

#endif
