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
$define %type pc_stream_handle_t as opaque miniport stream instance
$define %type ks_format_t as canonical sample format descriptor
$define %type dma_seg_t as one scatter/gather segment

$const PC_FAILURE_* as port-class request failure codes
$const PC_STREAM_* as miniport stream creation flags

$define %func pc_miniport_probe_format as function with args const pc_miniport_t *, const ks_format_t *
$define %func pc_miniport_create_stream as function with args const pc_miniport_t *, const ks_format_t *, uint, pc_stream_handle_t **
$define %func pc_miniport_destroy_stream as procedure with args const pc_miniport_t *, pc_stream_handle_t *
*/

/* !SPACE!

$space %export pc_miniport_probe_format, pc_miniport_create_stream
$space %export pc_miniport_destroy_stream, pc_miniport_set_state
$space %export pc_miniport_position, pc_miniport_map_buffer

*/

#ifndef KERNEL_AUDIO_PORTCLS_PCMINIPORT_H
#define KERNEL_AUDIO_PORTCLS_PCMINIPORT_H

#include <mlibc/mlibc.h>
#include <kernel/mm/dma/dma.h>
#include <kernel/audio/ks/kstypes.h>

typedef struct pc_miniport pc_miniport_t;
struct mp_stream;
typedef struct mp_stream mp_stream_t;

#define	PC_FAILURE_INVALID_FORMAT	(-1)
#define	PC_FAILURE_INVALID_STATE	(-2)
#define	PC_FAILURE_NO_RESOURCE		(-3)
#define	PC_FAILURE_NOT_SUPPORTED	(-4)

#define	PC_STREAM_RENDER	0x00000001
#define	PC_STREAM_CAPTURE	0x00000002

struct pc_miniport {
	int	(*probe_format)(const pc_miniport_t *mp,
		    const ks_format_t *fmt);

	int	(*create_stream)(const pc_miniport_t *mp,
		    const ks_format_t *fmt, u32 flags,
		    mp_stream_t **handle);
	int	(*destroy_stream)(const pc_miniport_t *mp,
		    mp_stream_t *stream);
	int	(*set_state)(const pc_miniport_t *mp, mp_stream_t *stream,
		    ks_state_t state);
	int	(*position)(const pc_miniport_t *mp, mp_stream_t *stream,
		    ks_position_t *pos);

	int	(*map_buffer)(const pc_miniport_t *mp, mp_stream_t *stream,
		    dma_seg_t *segs, u32 maxsegs, u32 *nsegs);

	int	(*get_buffer)(const pc_miniport_t *mp, mp_stream_t *stream,
		    u8 **virt, u64 *len);
	void	*device_ctx;
};


int
pc_miniport_probe_format(const pc_miniport_t *mp, const ks_format_t *fmt);

int
pc_miniport_create_stream(const pc_miniport_t *mp, const ks_format_t *fmt,
    u32 flags, mp_stream_t **handle);

int
pc_miniport_destroy_stream(const pc_miniport_t *mp, mp_stream_t *stream);

int
pc_miniport_set_state(const pc_miniport_t *mp, mp_stream_t *stream,
    ks_state_t state);

int
pc_miniport_position(const pc_miniport_t *mp, mp_stream_t *stream,
    ks_position_t *pos);

#endif