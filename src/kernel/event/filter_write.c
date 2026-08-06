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
$define %type int as 32 bit signed
$define %type knote_t as struct with registered event state
$define %type kevent as struct with event ident, filter, flags, fflags, data, udata
$define %type filter_ops_t as struct with filter callbacks vtable
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type net_endpoint as native network endpoint state
$define %type pipe_t as struct with pipe ring buffer
$define %type net_endpoint_t as native network endpoint state

$define %func filt_write_attach as function with args knote_t *
$define %func filt_write_detach as procedure with args knote_t *
$define %func filt_write_event as function with args knote_t *, u32
$define %func filt_write_touch as procedure with args knote_t *, struct kevent *

*/

/* !SPACE!

$space %internal filt_write_attach, filt_write_detach
$space %internal filt_write_event, filt_write_touch
$space %export filter_write_ops

*/

#include <kernel/event/event.h>
#include <kernel/api/api.h>
#include <kernel/net/endpoint.h>
#include <kernel/process.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int
filt_write_attach(knote_t *kn)
{
	int			fd;
	process_t		*proc;
	entity_id_t		id;
	u32			access;
	int			ret;

	fd = (int)kn->ident;

	if (fd == 1 || fd == 2) {
		return (0);
	}
	proc = process_current();
	ret = entity_handle_lookup(proc, fd, &id, &access);
	if (ret != 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	return (0);
}

static void
filt_write_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_write_event(knote_t *kn, u32 nevents)
{
	int			fd;
	entity_id_t		id;
	u32			access;
	u16			arch;
	pipe_t			*p;
	u32			space;
	process_t		*proc;
	int			ret;

	(void)nevents;
	fd = (int)kn->ident;

	if (fd == 1 || fd == 2) {
		kn->data = PIPE_BUF_SIZE;
		return (1);
	}

	proc = process_current();
	ret = entity_handle_lookup(proc, fd, &id, &access);
	if (ret != 0) {
		return (0);
	}
	arch = entity_arch(id);
	if (arch == ENTITY_ARCH_PIPE) {
		p = (pipe_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
		if (!p) {
			return (0);
		}
		space = PIPE_BUF_SIZE - p->size;
		if (space > 0) {
			kn->data = space;
			return (1);
		}
		if (p->readers == 0) {
			kn->flags |= EV_EOF;
			return (1);
		}
		return (0);
	}

	if (arch == ENTITY_ARCH_NET) {
		net_endpoint_t	*ep;

		ep = (net_endpoint_t *)entity_io_ptr(id,
		    ENTITY_IO_PTR_BACKING);
		if (ep && net_endpoint_writable(ep)) {
			kn->data = net_endpoint_write_space(ep);
			return (1);
		}
		return (0);
	}

	if (arch == ENTITY_ARCH_FILE || arch == ENTITY_ARCH_VNODE) {
		kn->data = PIPE_BUF_SIZE;
		return (1);
	}

	return (0);
}

static void
filt_write_touch(knote_t *kn, struct kevent *kev)
{
	(void)kn;
	(void)kev;
}

const filter_ops_t filter_write_ops = {
	.filter	= EVFILT_WRITE,
	.name	= "write",
	.attach	= filt_write_attach,
	.detach	= filt_write_detach,
	.event	= filt_write_event,
	.touch	= filt_write_touch,
};
