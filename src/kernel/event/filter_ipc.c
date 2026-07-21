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

/* !DEFINES!

$define %type knote_t as registered event state
$define %type ipc_endpoint_t as IPC endpoint state
$define %func filt_ipc_endpoint as function with args knote_t *
$define %func filt_ipc_attach as function with args knote_t *
$define %func filt_ipc_detach as procedure with args knote_t *
$define %func filt_ipc_event as function with args knote_t *, u32
$define %func filt_ipc_touch as procedure with args knote_t *, kevent *

*/

/* !SPACE!

$space %internal filt_ipc_endpoint, filt_ipc_attach
$space %internal filt_ipc_detach, filt_ipc_event, filt_ipc_touch
$space %export filter_ipc_ops

*/

#include <kernel/api/api.h>
#include <kernel/event/event.h>
#include <kernel/ipc/ipc.h>

static ipc_endpoint_t *
filt_ipc_endpoint(knote_t *kn)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	int		fd, object_index;

	fd = (int)kn->ident;
	handles = api_get_handle_table();
	objects = api_get_object_table();
	if (!handles || fd < 0 || fd >= MAX_HANDLES ||
	    !handles[fd].used) {
		return (NULL);
	}
	object_index = handles[fd].object_index;
	if (object_index < 0 || object_index >= MAX_DATA_OBJECTS ||
	    !objects[object_index].used ||
	    objects[object_index].type != API_OBJECT_IPC) {
		return (NULL);
	}
	return ((ipc_endpoint_t *)objects[object_index].ipc);
}

static int
filt_ipc_attach(knote_t *kn)
{
	if (!filt_ipc_endpoint(kn)) {
		return (-API_ERR_BAD_HANDLE);
	}
	kn->fpriv = ((u64)filt_ipc_endpoint(kn)->peer_generation << 32) |
	    (kn->fflags != 0 ? kn->fflags : NOTE_IPC_ALL);
	return (0);
}

static void
filt_ipc_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_ipc_event(knote_t *kn, u32 nevents)
{
	ipc_endpoint_t	*endpoint;
	u32		mask, seen, state;

	(void)nevents;
	endpoint = filt_ipc_endpoint(kn);
	if (!endpoint) {
		kn->flags |= EV_EOF;
		kn->fflags = NOTE_IPC_HUP;
		kn->data = 0;
		return (1);
	}
	mask = (u32)kn->fpriv;
	seen = (u32)(kn->fpriv >> 32);
	state = 0;
	if (ipc_endpoint_readable(endpoint)) {
		state |= NOTE_IPC_READ;
	}
	if (ipc_endpoint_writable(endpoint)) {
		state |= NOTE_IPC_WRITE;
	}
	if (endpoint->role == IPC_ENDPOINT_SERVER &&
	    endpoint->peer_generation != seen) {
		state |= NOTE_IPC_PEER;
		seen = endpoint->peer_generation;
	}
	if (ipc_endpoint_hup(endpoint)) {
		state |= NOTE_IPC_HUP;
		kn->flags |= EV_EOF;
	}
	state &= mask;
	kn->fpriv = ((u64)seen << 32) | mask;
	kn->fflags = state;
	kn->data = ipc_endpoint_pending(endpoint);
	return (state != 0);
}

static void
filt_ipc_touch(knote_t *kn, struct kevent *kev)
{
	kn->fpriv = (kn->fpriv & 0xFFFFFFFF00000000ULL) |
	    (kev->fflags != 0 ? kev->fflags : NOTE_IPC_ALL);
}

const filter_ops_t filter_ipc_ops = {
	.filter	= EVFILT_IPC,
	.name	= "ipc",
	.attach	= filt_ipc_attach,
	.detach	= filt_ipc_detach,
	.event	= filt_ipc_event,
	.touch	= filt_ipc_touch,
};
