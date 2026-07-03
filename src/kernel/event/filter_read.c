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
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type knote_t as struct with registered event state
$define %type kevent as struct with event ident, filter, flags, fflags, data, udata
$define %type filter_ops_t as struct with filter callbacks vtable
$define %type api_handle_t as struct with handle table entry
$define %type api_object_t as struct with object table entry
$define %type pipe_t as struct with pipe ring buffer

$define %func filt_read_attach as function with args knote_t *
$define %func filt_read_detach as procedure with args knote_t *
$define %func filt_read_event as function with args knote_t *, u32
$define %func filt_read_touch as procedure with args knote_t *, struct kevent *

*/

/* !SPACE!

$space %internal filt_read_attach, filt_read_detach
$space %internal filt_read_event, filt_read_touch
$space %export filter_read_ops

*/

#include <kernel/event/event.h>
#include <kernel/api/api.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int
filt_read_attach(knote_t *kn)
{
	int			fd;
	api_handle_t		*handles;

	fd = (int)kn->ident;

	if (fd == 0) {
		return (0);
	}

	handles = api_get_handle_table();
	if (!handles) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (fd < 0 || fd >= MAX_HANDLES || !handles[fd].used) {
		printk("[EVFILT_READ] attach: bad fd %d\n",
		    fd);
		return (-API_ERR_BAD_HANDLE);
	}

	return (0);
}

static void
filt_read_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_read_event(knote_t *kn, u32 nevents)
{
	int			fd;
	api_handle_t		*handles;
	api_object_t		*objects;
	int			obj_idx;
	api_object_t		*obj;
	pipe_t			*p;
	char			c;

	fd = (int)kn->ident;

	if (fd == 0) {
		c = keyboard_getchar();
		if (c) {
			kn->data = 1;
			return (1);
		}
		return (0);
	}

	handles = api_get_handle_table();
	if (!handles || fd < 0 || fd >= MAX_HANDLES ||
	    !handles[fd].used) {
		return (0);
	}

	objects = api_get_object_table();
	obj_idx = handles[fd].object_index;
	if (obj_idx < 0 || obj_idx >= MAX_DATA_OBJECTS ||
	    !objects[obj_idx].used) {
		return (0);
	}

	obj = &objects[obj_idx];

	if (obj->type == API_OBJECT_PIPE) {
		p = (pipe_t *)obj->pipe;
		if (!p) {
			return (0);
		}
		if (p->size > 0) {
			kn->data = p->size;
			return (1);
		}
		if (p->writers == 0) {
			kn->flags |= EV_EOF;
			kn->data = 0;
			return (1);
		}
		return (0);
	}

	if (obj->type == API_OBJECT_FILE) {
		kn->data = 1;
		return (1);
	}

	return (0);
}

static void
filt_read_touch(knote_t *kn, struct kevent *kev)
{
	(void)kn;
	(void)kev;
}

const filter_ops_t filter_read_ops = {
	.filter	= EVFILT_READ,
	.name	= "read",
	.attach	= filt_read_attach,
	.detach	= filt_read_detach,
	.event	= filt_read_event,
	.touch	= filt_read_touch,
};
