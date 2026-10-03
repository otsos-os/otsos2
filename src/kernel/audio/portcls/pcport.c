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
$define %type ks_format_t as canonical sample format descriptor
$define %type ks_position_t as play/capture cursor position pair

$define %func pc_port_create as function with args const pc_miniport_t *, const char *
$define %func pc_port_destroy as procedure with args pc_port_t *
$define %func pc_port_open_stream as function with args pc_port_t *, const ks_format_t *, uint
$define %func pc_port_close_stream as function with args pc_port_t *, pc_stream_t *
$define %func pc_port_process as procedure with args pc_port_t *
$define %func pc_port_start as function with args pc_port_t *, pc_stream_t *
$define %func pc_port_stop as function with args pc_port_t *, pc_stream_t *
$define %func pc_port_abort as function with args pc_port_t *, pc_stream_t *
$define %func pc_port_process as procedure with args pc_port_t *
$define %func pc_port_mix_render as procedure with args pc_port_t *, pc_stream_t *, u64
$define %func pc_stream_apply_volume as function with args pc_stream_t *, const u8 *, u64
$define %func pc_stream_capture_pass as procedure with args pc_port_t *, pc_stream_t *, u64
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
$space %internal pc_port_mix_render, pc_stream_apply_volume
$space %internal pc_stream_capture_pass

*/

#include <kernel/audio/portcls/pcport.h>
#include <kernel/audio/mix/mix.h>

static void	pc_port_mix_render(pc_port_t *port, pc_stream_t *stream,
		    u64 delta);
static void	pc_stream_capture_pass(pc_port_t *port, pc_stream_t *stream,
		    u64 delta);
static u64	pc_stream_apply_volume(pc_stream_t *stream, const u8 *src,
		    u64 count);
static int	pc_port_abort_locked(pc_port_t *port, pc_stream_t *stream);

pc_port_t *
pc_port_create(const pc_miniport_t *mp, const char *name)
{
	pc_port_t	*port;
	u32		len;

	if (mp == NULL || mp->create_stream == NULL ||
	    mp->destroy_stream == NULL || mp->set_state == NULL ||
	    mp->position == NULL) {
		return (NULL);
	}
	port = (pc_port_t *)kmem_calloc(1, sizeof(*port));
	if (port == NULL) {
		return (NULL);
	}
	port->mp = *mp;
	port->master_volume = PC_VOLUME_UNITY;
	spin_init(&port->lock, "pcport", LO_AUDIO);
	if (name != NULL) {
		len = strlen(name);
		if (len >= KS_DEVICE_NAME_MAX) {
			len = KS_DEVICE_NAME_MAX - 1;
		}
		memcpy(port->name, name, len);
		port->name[len] = '\0';
	}
	return (port);
}

void
pc_port_destroy(pc_port_t *port)
{
	u32	i;

	if (port == NULL) {
		return;
	}
	for (i = 0; i < port->stream_count; i++) {
		if (port->streams[i].handle != NULL) {
			port->mp.destroy_stream(&port->mp,
			    port->streams[i].handle);
			port->streams[i].handle = NULL;
		}
	}
	kmem_free(port);
}

pc_stream_t *
pc_port_open_stream(pc_port_t *port, const ks_format_t *fmt, u32 flags)
{
	pc_stream_t	*stream;
	u32		i, framesz;

	if (port == NULL || fmt == NULL ||
	    port->stream_count >= PC_MAX_STREAMS) {
		return (NULL);
	}
	if (port->mp.probe_format != NULL &&
	    port->mp.probe_format(&port->mp, fmt) != 0) {
		return (NULL);
	}
	stream = &port->streams[port->stream_count];
	if (port->mp.create_stream(&port->mp, fmt, flags,
	    &stream->handle) != 0) {
		return (NULL);
	}
	framesz = ks_format_bytes_per_frame(fmt);
	if (framesz == 0) {
		port->mp.destroy_stream(&port->mp, stream->handle);
		stream->handle = NULL;
		return (NULL);
	}

	stream->ring.base = NULL;
	stream->ring.length = 0;
	if (port->mp.get_buffer != NULL) {
		u64	buflen;

		buflen = 0;
		if (port->mp.get_buffer(&port->mp, stream->handle,
		    &stream->ring.base, &buflen) == 0 && buflen != 0) {
			ks_ring_set_length(&stream->ring, buflen);
		}
	}
	if (stream->ring.base == NULL || stream->ring.length == 0) {
		stream->ring.base = NULL;
		ks_ring_init(&stream->ring,
		    (u64)fmt->rate * (u64)framesz);
	}
	stream->format = *fmt;
	stream->flags = flags;
	stream->state = KS_STATE_STOP;
	stream->active = 0;
	stream->started = 0;
	stream->volume = PC_VOLUME_UNITY;
	stream->parent = port;
	stream->position = 0;
	stream->last_position = 0;
	spin_lock(&port->lock);
	port->stream_count++;
	for (i = 0; i < port->stream_count; i++) {
		port->enable_mask |= (1u << i);
	}
	spin_unlock(&port->lock);
	return (stream);
}

static int
pc_port_abort_locked(pc_port_t *port, pc_stream_t *stream)
{
	u32	i;

	if (port == NULL || stream == NULL ||
	    stream->handle == NULL) {
		return (-1);
	}
	if (stream->active || stream->state != KS_STATE_STOP) {
		if (port->mp.set_state(&port->mp, stream->handle,
		    KS_STATE_STOP) != 0) {
			return (-1);
		}
	}
	stream->state = KS_STATE_STOP;
	stream->active = 0;
	stream->started = 0;
	stream->drain_stop = 0;
	for (i = 0; i < port->stream_count; i++) {
		if (&port->streams[i] == stream) {
			port->enable_mask &= ~(1u << i);
			break;
		}
	}
	return (0);
}

int
pc_port_abort(pc_port_t *port, pc_stream_t *stream)
{
	int	ret;

	if (port == NULL || stream == NULL ||
	    stream->handle == NULL) {
		return (-1);
	}
	spin_lock(&port->lock);
	ret = pc_port_abort_locked(port, stream);
	spin_unlock(&port->lock);
	return (ret);
}

int
pc_port_close_stream(pc_port_t *port, pc_stream_t *stream)
{
	u32	i;
	int	ret;

	if (port == NULL || stream == NULL) {
		return (-1);
	}
	spin_lock(&port->lock);
	(void)pc_port_abort_locked(port, stream);
	ret = -1;
	for (i = 0; i < port->stream_count; i++) {
		if (&port->streams[i] != stream) {
			continue;
		}
		if (stream->handle != NULL) {
			port->mp.destroy_stream(&port->mp, stream->handle);
			stream->handle = NULL;
		}
		if (i != port->stream_count - 1) {
			port->streams[i] =
			    port->streams[port->stream_count - 1];
		}
		memset(&port->streams[port->stream_count - 1], 0,
		    sizeof(port->streams[0]));
		port->stream_count--;
		ret = 0;
		break;
	}
	spin_unlock(&port->lock);
	return (ret);
}

int
pc_port_start(pc_port_t *port, pc_stream_t *stream)
{
	ks_state_t	next;
	u32		i;

	if (port == NULL || stream == NULL ||
	    stream->handle == NULL) {
		return (-1);
	}
	spin_lock(&port->lock);
	next = KS_STATE_RUN;
	ks_ring_reset(&stream->ring);
	if (stream->ring.base != NULL) {
		memset(stream->ring.base, 0, (size_t)stream->ring.length);
	}
	if (port->mp.set_state(&port->mp, stream->handle, next) != 0) {
		spin_unlock(&port->lock);
		return (-1);
	}
	stream->state = next;
	stream->active = 1;
	stream->started = 0;
	stream->drain_stop = 0;
	for (i = 0; i < port->stream_count; i++) {
		if (&port->streams[i] == stream) {
			port->enable_mask |= (1u << i);
			break;
		}
	}
	spin_unlock(&port->lock);
	return (0);
}

int
pc_port_stop(pc_port_t *port, pc_stream_t *stream)
{
	u32		i;
	int		ret;

	if (port == NULL || stream == NULL ||
	    stream->handle == NULL) {
		return (-1);
	}
	spin_lock(&port->lock);
	if ((stream->flags & PC_STREAM_RENDER) && stream->active) {
		if (stream->ring.base != NULL && stream->ring.length != 0) {
			u64	tail;

			tail = stream->ring.write_cursor % stream->ring.length;
			if (tail != 0) {
				memset(stream->ring.base + tail, 0,
				    (size_t)(stream->ring.length - tail));
			}
		}
		stream->drain_stop = 1;
		stream->state = KS_STATE_STOP;
		spin_unlock(&port->lock);
		return (0);
	}
	ret = port->mp.set_state(&port->mp, stream->handle,
	    KS_STATE_STOP);
	if (ret != 0) {
		spin_unlock(&port->lock);
		return (-1);
	}
	stream->state = KS_STATE_STOP;
	stream->active = 0;
	stream->drain_stop = 0;
	for (i = 0; i < port->stream_count; i++) {
		if (&port->streams[i] == stream) {
			port->enable_mask &= ~(1u << i);
			break;
		}
	}
	spin_unlock(&port->lock);
	return (0);
}

void
pc_port_process(pc_port_t *port)
{
	ks_position_t	pos;
	pc_stream_t	*stream;
	u64		delta, avail, buffered;
	u32		i;

	if (port == NULL) {
		return;
	}
	spin_lock(&port->lock);
	for (i = 0; i < port->stream_count; i++) {
		stream = &port->streams[i];
		if (stream->handle == NULL || !stream->active) {
			continue;
		}
		if (port->mp.position(&port->mp, stream->handle,
		    &pos) != 0) {
			continue;
		}

		pos.play_offset = pos.play_offset % stream->ring.length;
		if (!stream->started) {
			stream->last_position = pos.play_offset;
			stream->started = 1;
			stream->position = pos.play_offset;
			continue;
		}
		stream->last_position = stream->position;
		stream->position = pos.play_offset;
		if (pos.play_offset >= stream->last_position) {
			delta = pos.play_offset - stream->last_position;
		} else {
			delta = stream->ring.length -
			    (stream->last_position - pos.play_offset);
		}
		if (delta == 0) {
			continue;
		}
		if (stream->flags & PC_STREAM_RENDER) {
			pc_port_mix_render(port, stream, delta);
		} else {
			pc_stream_capture_pass(port, stream, delta);
		}
		if ((stream->flags & PC_STREAM_RENDER) &&
		    stream->drain_stop &&
		    stream->ring.read_cursor >= stream->ring.write_cursor) {
			port->mp.set_state(&port->mp, stream->handle,
			    KS_STATE_STOP);
			stream->active = 0;
			stream->drain_stop = 0;
		}
		avail = ks_ring_bytes_available(&stream->ring);
		buffered = stream->ring.length;
		(void)avail;
		(void)buffered;
	}
	spin_unlock(&port->lock);
}

static void
pc_port_mix_render(pc_port_t *port, pc_stream_t *stream, u64 delta)
{
	(void)port;
	if (delta > ks_ring_bytes_available(&stream->ring)) {
		delta = ks_ring_bytes_available(&stream->ring);
	}
	stream->ring.read_cursor += delta;
}

static void
pc_stream_capture_pass(pc_port_t *port, pc_stream_t *stream, u64 delta)
{
	(void)port;
	if (delta > stream->ring.length -
	    (stream->ring.write_cursor - stream->ring.read_cursor)) {
		delta = stream->ring.length -
		    (stream->ring.write_cursor - stream->ring.read_cursor);
	}
	stream->ring.write_cursor += delta;
}

static u64
pc_stream_apply_volume(pc_stream_t *stream, const u8 *src, u64 count)
{
	u64		free_count;
	u64		base;
	u64		n;
	mix_gain_t	g;
	u32		w;

	if (stream == NULL || stream->ring.base == NULL || count == 0) {
		return (0);
	}
	free_count = ks_ring_bytes_free(&stream->ring);
	if (count > free_count) {
		count = free_count;
	}
	if (count == 0) {
		return (0);
	}
	w = mix_sample_bytes(stream->format.container);
	if (w == 0) {
		w = 1;
	}
	g = mix_gain_scale(stream->volume,
	    stream->parent != NULL ? stream->parent->master_volume :
	    PC_VOLUME_UNITY);
	if (g == MIX_GAIN_UNITY) {
		return (ks_ring_write(&stream->ring, src, count));
	}
	count -= count % w;
	if (count == 0) {
		return (0);
	}
	base = stream->ring.write_cursor;
	n = ks_ring_write(&stream->ring, src, count);
	if (n != 0) {
		u64	span;
		u64	chunk;
		u64	pos;

		span = n;
		pos = base % stream->ring.length;
		while (span != 0) {
			chunk = span;
			if (chunk > stream->ring.length - pos) {
				chunk = stream->ring.length - pos;
			}
			mix_apply_gain_c(stream->format.container,
			    stream->ring.base + pos,
			    stream->ring.base + pos, chunk / w, g);
			span -= chunk;
			pos = 0;
		}
	}
	return (n);
}

int
pc_port_set_master_volume(pc_port_t *port, u32 volume)
{
	if (port == NULL) {
		return (-1);
	}
	if (volume > PC_VOLUME_MAX) {
		volume = PC_VOLUME_MAX;
	}
	port->master_volume = volume;
	return (0);
}

u32
pc_port_get_master_volume(const pc_port_t *port)
{
	return (port == NULL ? PC_VOLUME_UNITY : port->master_volume);
}

int
pc_stream_set_volume(pc_stream_t *stream, u32 volume)
{
	if (stream == NULL) {
		return (-1);
	}
	if (volume > PC_VOLUME_MAX) {
		volume = PC_VOLUME_MAX;
	}
	stream->volume = volume;
	return (0);
}

u32
pc_stream_get_volume(const pc_stream_t *stream)
{
	return (stream == NULL ? PC_VOLUME_UNITY : stream->volume);
}

u64
pc_stream_write(pc_stream_t *stream, const u8 *src, u64 count)
{
	if (stream == NULL) {
		return (0);
	}
	return (pc_stream_apply_volume(stream, src, count));
}

u64
pc_stream_read(pc_stream_t *stream, u8 *dst, u64 count)
{
	if (stream == NULL) {
		return (0);
	}
	return (ks_ring_read(&stream->ring, dst, count));
}

u64
pc_stream_bytes_free(pc_stream_t *stream)
{
	return (stream == NULL ? 0 : ks_ring_bytes_free(&stream->ring));
}

u64
pc_stream_bytes_available(pc_stream_t *stream)
{
	return (stream == NULL ? 0 :
	    ks_ring_bytes_available(&stream->ring));
}
