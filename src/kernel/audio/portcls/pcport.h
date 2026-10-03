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

$define %type pc_port_t as one managed device port and its stream set
$define %type pc_stream_t as one managed stream bound to a miniport stream
$define %type pc_miniport_t as hardware ops the port-class drives
$define %type ks_ring_t as shared ring buffer

$const PC_MAX_STREAMS as streams one port may manage
$const PC_TICK_GRANULARITY as engine advancement quantum in bytes

$define %func pc_port_create as function with args const pc_miniport_t *, const char *
$define %func pc_port_destroy as procedure with args pc_port_t *
$define %func pc_port_open_stream as function with args pc_port_t *, const ks_format_t *, uint
$define %func pc_port_close_stream as function with args pc_port_t *, pc_stream_t *
$define %func pc_port_process as procedure with args pc_port_t *
$define %func pc_port_start as function with args pc_port_t *, pc_stream_t *
$define %func pc_port_stop as function with args pc_port_t *, pc_stream_t *
$define %func pc_port_abort as function with args pc_port_t *, pc_stream_t *
$define %func pc_port_set_master_volume as function with args pc_port_t *, uint
$define %func pc_port_get_master_volume as function with args const pc_port_t *
$define %func pc_stream_set_volume as function with args pc_stream_t *, uint
$define %func pc_stream_get_volume as function with args const pc_stream_t *
*/

/* !SPACE!

$space %export pc_port_create, pc_port_destroy
$space %export pc_port_open_stream, pc_port_close_stream
$space %export pc_port_process, pc_port_start, pc_port_stop
$space %export pc_port_abort
$space %export pc_port_set_master_volume, pc_port_get_master_volume
$space %export pc_stream_set_volume, pc_stream_get_volume

*/

#ifndef KERNEL_AUDIO_PORTCLS_PCPORT_H
#define KERNEL_AUDIO_PORTCLS_PCPORT_H

#include <mlibc/mlibc.h>
#include <kernel/audio/portcls/pcminiport.h>
#include <kernel/audio/ks/ksobj.h>
#include <kernel/audio/ks/ksstream.h>
#include <kernel/audio/mix/mix.h>
#include <kernel/sync/sync.h>

#define	PC_MAX_STREAMS		16
#define	PC_GAIN_STEPS_PER_MS	4
#define	PC_DRAIN_FADE_STEPS	16

typedef struct pc_port pc_port_t;
typedef struct pc_stream pc_stream_t;

struct pc_stream {
	mp_stream_t		*handle;
	struct pc_port		*parent;
	ks_ring_t		ring;
	ks_format_t		format;
	ks_state_t		state;
	u32			flags;
	u32			active;
	u32			started;
	u32			drain_stop;
	u64			last_position;
	u64			position;
	u32			volume;		/* Q15 linear per-stream gain */
};

struct pc_port {
	pc_miniport_t		mp;
	pc_stream_t		streams[PC_MAX_STREAMS];
	u32			stream_count;
	char			name[KS_DEVICE_NAME_MAX];
	u32			enable_mask;
	u32			master_volume;	/* Q15 linear master gain */
	spin_t			lock;
};

pc_port_t	*pc_port_create(const pc_miniport_t *mp, const char *name);
void		pc_port_destroy(pc_port_t *port);
pc_stream_t	*pc_port_open_stream(pc_port_t *port,
		    const ks_format_t *fmt, u32 flags);
int		pc_port_close_stream(pc_port_t *port, pc_stream_t *stream);
void		pc_port_process(pc_port_t *port);
int		pc_port_start(pc_port_t *port, pc_stream_t *stream);
int		pc_port_stop(pc_port_t *port, pc_stream_t *stream);
int		pc_port_abort(pc_port_t *port, pc_stream_t *stream);
int		pc_port_set_master_volume(pc_port_t *port, u32 volume);
u32		pc_port_get_master_volume(const pc_port_t *port);
int		pc_stream_set_volume(pc_stream_t *stream, u32 volume);
u32		pc_stream_get_volume(const pc_stream_t *stream);
u64		pc_stream_write(pc_stream_t *stream, const u8 *src, u64 count);
u64		pc_stream_read(pc_stream_t *stream, u8 *dst, u64 count);
u64		pc_stream_bytes_free(pc_stream_t *stream);
u64		pc_stream_bytes_available(pc_stream_t *stream);

#endif
