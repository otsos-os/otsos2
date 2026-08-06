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
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type knote as registered event state
$define %type kevent as native event descriptor
$define %type filter_ops as filter callback table
$define %type entity_id as 64 bit packed archetype/generation/index

$define %func filt_entity_attach as function with args knote *
$define %func filt_entity_detach as procedure with args knote *
$define %func filt_entity_event as function with args knote *, u32
$define %func filt_entity_touch as procedure with args knote *, kevent *

*/

/* !SPACE!

$space %internal filt_entity_attach, filt_entity_detach
$space %internal filt_entity_event, filt_entity_touch
$space %export filter_entity_ops

*/

#include <kernel/api/api.h>
#include <kernel/entity/entity.h>
#include <kernel/event/event.h>
#include <kernel/process.h>
#include <mlibc/mlibc.h>

static int
filt_entity_attach(knote_t *kn)
{
	process_t	*proc;
	entity_id_t	id;
	u32		access;

	proc = process_current();
	if (entity_handle_lookup(proc, (int)kn->ident, &id, &access) != 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	return (0);
}

static void
filt_entity_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_entity_event(knote_t *kn, u32 nevents)
{
	(void)nevents;
	return (1);
}

static void
filt_entity_touch(knote_t *kn, struct kevent *kev)
{
	(void)kn;
	(void)kev;
}

const filter_ops_t filter_entity_ops = {
	.filter	= EVFILT_ENTITY,
	.name	= "entity",
	.attach	= filt_entity_attach,
	.detach	= filt_entity_detach,
	.event	= filt_entity_event,
	.touch	= filt_entity_touch,
};
