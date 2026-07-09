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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s32 as 32 bit signed
$define %type int as 32 bit signed
$define %type mouse_event as struct with normalized mouse input event

$define %func ps2_mouse_init as function with args void
$define %func ps2_mouse_handler as procedure with args void
$define %func ps2_mouse_poll as procedure with args void
$define %func ps2_mouse_is_ready as function with args void
$define %func mouse_event_get as function with args struct mouse_event *
$define %func mouse_event_count as function with args void
$define %func mouse_event_reset as procedure with args void

*/

/* !SPACE!

$space %export ps2_mouse_init, ps2_mouse_handler, ps2_mouse_poll
$space %export ps2_mouse_is_ready
$space %export mouse_event_get, mouse_event_count, mouse_event_reset

*/

#ifndef KERNEL_DRIVERS_MOUSE_MOUSE_H
#define KERNEL_DRIVERS_MOUSE_MOUSE_H

#include <mlibc/mlibc.h>

#define	MOUSE_BUTTON_LEFT	0x00000001
#define	MOUSE_BUTTON_RIGHT	0x00000002
#define	MOUSE_BUTTON_MIDDLE	0x00000004
#define	MOUSE_BUTTON_X1		0x00000008
#define	MOUSE_BUTTON_X2		0x00000010

#define	MOUSE_EVENT_MOVE	0x00000001
#define	MOUSE_EVENT_BUTTON	0x00000002
#define	MOUSE_EVENT_WHEEL	0x00000004
#define	MOUSE_EVENT_OVERFLOW	0x00000008

#define	MOUSE_EVENT_RING_SIZE	256

struct mouse_event {
	u64	timestamp;
	s32	x;
	s32	y;
	s32	dx;
	s32	dy;
	s32	dz;
	u32	buttons;
	u32	flags;
};

int	ps2_mouse_init(void);
void	ps2_mouse_handler(void);
void	ps2_mouse_poll(void);
int	ps2_mouse_is_ready(void);

int	mouse_event_get(struct mouse_event *out);
int	mouse_event_count(void);
void	mouse_event_reset(void);

#endif
