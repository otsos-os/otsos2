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

$define %type pc_miniport_t as hardware ops the port-class drives
$define %type mp_stream_t as opaque miniport stream instance
$define %type ks_format_t as canonical sample format descriptor
$define %type ks_position_t as play/capture cursor position pair

$define %func pc_miniport_probe_format as function with args const pc_miniport_t *, const ks_format_t *
$define %func pc_miniport_create_stream as function with args const pc_miniport_t *, const ks_format_t *, uint, mp_stream_t **
$define %func pc_miniport_destroy_stream as function with args const pc_miniport_t *, mp_stream_t *
$define %func pc_miniport_set_state as function with args const pc_miniport_t *, mp_stream_t *, ks_state_t
$define %func pc_miniport_position as function with args const pc_miniport_t *, mp_stream_t *, ks_position_t *
*/

/* !SPACE!

$space %export pc_miniport_probe_format, pc_miniport_create_stream
$space %export pc_miniport_destroy_stream, pc_miniport_set_state
$space %export pc_miniport_position

*/

#include <kernel/audio/portcls/pcminiport.h>

int
pc_miniport_probe_format(const pc_miniport_t *mp, const ks_format_t *fmt)
{
	if (mp == NULL || fmt == NULL) {
		return (PC_FAILURE_INVALID_FORMAT);
	}
	if (mp->probe_format != NULL) {
		return (mp->probe_format(mp, fmt));
	}
	return (0);
}

int
pc_miniport_create_stream(const pc_miniport_t *mp, const ks_format_t *fmt,
    u32 flags, mp_stream_t **handle)
{
	if (mp == NULL || fmt == NULL || handle == NULL ||
	    mp->create_stream == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	return (mp->create_stream(mp, fmt, flags, handle));
}

int
pc_miniport_destroy_stream(const pc_miniport_t *mp, mp_stream_t *stream)
{
	if (mp == NULL || stream == NULL || mp->destroy_stream == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	return (mp->destroy_stream(mp, stream));
}

int
pc_miniport_set_state(const pc_miniport_t *mp, mp_stream_t *stream,
    ks_state_t state)
{
	if (mp == NULL || stream == NULL || mp->set_state == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	if (!ks_state_valid(state)) {
		return (PC_FAILURE_INVALID_STATE);
	}
	return (mp->set_state(mp, stream, state));
}

int
pc_miniport_position(const pc_miniport_t *mp, mp_stream_t *stream,
    ks_position_t *pos)
{
	if (mp == NULL || stream == NULL || pos == NULL ||
	    mp->position == NULL) {
		return (PC_FAILURE_NOT_SUPPORTED);
	}
	return (mp->position(mp, stream, pos));
}