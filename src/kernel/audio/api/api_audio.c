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

$define %type api_audio_entry_t as one published endpoint slot
$define %type api_audio_stream_t as one open stream bound to an endpoint
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type api_audio_info_t as public endpoint descriptor
$define %type api_audio_format_t as public format descriptor
$define %type api_audio_position_t as public cursor snapshot
$define %type pc_stream_t as one managed stream bound to a miniport stream
$define %type pc_port_t as one managed device port and its stream set
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s32 as 32 bit signed

$define %func api_audio_init as procedure with args void
$define %func api_audio_publish as function with args const char *, uint, uint, void *
$define %func api_audio_endpoint_count as function with args void
$define %func api_audio_slot as function with args entity id
$define %func api_audio_stream_slot as function with args entity id
$define %func api_audio_ioctl as function with args entity id, command, argument
$define %func api_audio_read as function with args entity id, buffer, count, offset
$define %func api_audio_write as function with args entity id, buffer, count, offset
$define %func api_audio_release as procedure with args entity id
$define %func api_audio_stream_release as procedure with args entity id
$define %func api_audio_open_stream as function with args entity id, argument
*/

/* !SPACE!

$space %internal api_audio_slot, api_audio_stream_slot, api_audio_ioctl
$space %internal api_audio_read, api_audio_write, api_audio_release
$space %internal api_audio_stream_release, api_audio_open_stream
$space %internal api_audio_read_stream, api_audio_write_stream
$space %internal api_audio_ioctl_stream
$space %export api_audio_init, api_audio_publish
$space %export api_audio_endpoint_count

*/

#include <kernel/audio/api/api_audio.h>
#include <kernel/audio/portcls/pcport.h>
#include <kernel/api/api.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <mlibc/stdio.h>

#define	API_AUDIO_MAX_STREAMS	64

typedef struct api_audio_entry {
	char			name[AUDIO_NAME_MAX];
	void			*port;
	u32			flow;
	u32			pin_id;
	u32			state;
	u32			used;
	u32			volume;
	api_audio_format_t	format;
	api_audio_position_t	position;
} api_audio_entry_t;

typedef struct api_audio_stream {
	u32			used;
	void			*port;
	void			*stream;
	u32			flow;
	u32			pin_id;
	u32			state;
	u32			volume;
	char			name[AUDIO_NAME_MAX];
	api_audio_format_t	format;
} api_audio_stream_t;

static api_audio_entry_t	api_audio_entries[AUDIO_MAX_ENDPOINTS];
static api_audio_stream_t	api_audio_streams[API_AUDIO_MAX_STREAMS];
static const entity_io_ops_t	*api_audio_io_ops;
static const entity_io_ops_t	*api_audio_stream_io_ops;

_Static_assert(sizeof(api_audio_info_t) == 96, "api_audio_info_t ABI size");
_Static_assert(sizeof(api_audio_format_t) == 20, "api_audio_format_t ABI size");
_Static_assert(sizeof(api_audio_position_t) == 16, "api_audio_position_t ABI size");
_Static_assert(__builtin_offsetof(api_audio_info_t, name) == 32,
    "api_audio_info_t name offset");

static api_audio_stream_t	*api_audio_stream_slot(entity_id_t id);
static int			api_audio_open_stream(entity_id_t id, void *arg);

static s32
api_audio_index(entity_id_t id)
{
	s32	index;

	if (id == 0) {
		return (-1);
	}
	if (entity_io_i32(id, 0, &index) != 0 || index < 0) {
		return (-1);
	}
	return (index);
}

static api_audio_entry_t *
api_audio_slot(entity_id_t id)
{
	s32	index;

	if (entity_arch(id) != ENTITY_ARCH_AUDIO) {
		return (NULL);
	}
	index = api_audio_index(id);
	if (index >= AUDIO_MAX_ENDPOINTS) {
		return (NULL);
	}
	if (!api_audio_entries[index].used) {
		return (NULL);
	}
	return (&api_audio_entries[index]);
}

static api_audio_stream_t *
api_audio_stream_slot(entity_id_t id)
{
	s32	index;

	if (entity_arch(id) != ENTITY_ARCH_AUDIO_STREAM) {
		return (NULL);
	}
	index = api_audio_index(id);
	if (index >= API_AUDIO_MAX_STREAMS) {
		return (NULL);
	}
	if (!api_audio_streams[index].used) {
		return (NULL);
	}
	return (&api_audio_streams[index]);
}

static pc_stream_t *
api_audio_stream_ensure(api_audio_stream_t *st)
{
	pc_port_t	*port;
	ks_format_t	fmt;
	u32		flags;

	if (st == NULL) {
		return (NULL);
	}
	if (st->stream != NULL) {
		return ((pc_stream_t *)st->stream);
	}
	port = (pc_port_t *)st->port;
	if (port == NULL) {
		return (NULL);
	}
	fmt.container = st->format.container;
	fmt.valid_bits = st->format.valid_bits;
	fmt.channels = st->format.channels;
	fmt.rate = st->format.rate;
	fmt.channel_mask = st->format.channel_mask;
	flags = (st->flow == KS_DATAFLOW_OUT) ? PC_STREAM_RENDER :
	    PC_STREAM_CAPTURE;
	st->stream = pc_port_open_stream(port, &fmt, flags);
	if (st->stream != NULL) {
		(void)pc_stream_set_volume((pc_stream_t *)st->stream,
		    st->volume);
	}
	return ((pc_stream_t *)st->stream);
}

static int
api_audio_info_fill(entity_id_t id, void *arg)
{
	api_audio_entry_t	*entry;
	api_audio_info_t	info;

	entry = api_audio_slot(id);
	if (entry == NULL || arg == NULL) {
		return (-1);
	}
	memset(&info, 0, sizeof(info));
	info.id = entry->pin_id;
	info.flow = entry->flow;
	info.state = entry->state;
	info.formats_count = 1;
	info.channels = entry->format.channels;
	info.rate = entry->format.rate;
	info.container = entry->format.container;
	info.valid_bits = entry->format.valid_bits;
	memcpy(info.name, entry->name, sizeof(info.name));
	memcpy(arg, &info, sizeof(info));
	return (0);
}

static int
api_audio_ioctl(entity_id_t id, u64 cmd, void *arg)
{
	switch (cmd) {
	case AUDIO_IOCTL_GET_INFO:
		return (api_audio_info_fill(id, arg));
	case AUDIO_IOCTL_OPEN_STREAM:
		return (api_audio_open_stream(id, arg));
	default:
		return (-1);
	}
}

static int
api_audio_read(entity_id_t id, void *buf, u64 count, u64 offset)
{
	(void)id;
	(void)buf;
	(void)count;
	(void)offset;
	return (0);
}

static int
api_audio_write(entity_id_t id, const void *buf, u64 count, u64 offset)
{
	(void)id;
	(void)buf;
	(void)count;
	(void)offset;
	return (0);
}

static void
api_audio_release(entity_id_t id)
{
	(void)id;
}

static int
api_audio_write_stream(entity_id_t id, const void *buf, u64 count, u64 offset)
{
	api_audio_stream_t	*st;
	pc_stream_t		*stream;

	(void)offset;
	st = api_audio_stream_slot(id);
	if (st == NULL || st->flow != KS_DATAFLOW_OUT) {
		return (0);
	}
	stream = api_audio_stream_ensure(st);
	if (stream == NULL) {
		return (0);
	}
	return ((int)pc_stream_write(stream, (const u8 *)buf, count));
}

static int
api_audio_read_stream(entity_id_t id, void *buf, u64 count, u64 offset)
{
	api_audio_stream_t	*st;
	pc_stream_t		*stream;

	(void)offset;
	st = api_audio_stream_slot(id);
	if (st == NULL || st->flow != KS_DATAFLOW_IN) {
		return (0);
	}
	stream = api_audio_stream_ensure(st);
	if (stream == NULL) {
		return (0);
	}
	return ((int)pc_stream_read(stream, (u8 *)buf, count));
}

static int
api_audio_stream_set_format(entity_id_t id, void *arg)
{
	api_audio_stream_t	*st;
	api_audio_format_t	format;

	st = api_audio_stream_slot(id);
	if (st == NULL || arg == NULL) {
		return (-1);
	}
	memcpy(&format, arg, sizeof(format));
	if (format.channels == 0 || format.channels > KS_MAX_CHANNELS ||
	    format.container == 0 || format.rate == 0) {
		return (-1);
	}
	st->format = format;
	return (0);
}

static int
api_audio_stream_get_position(entity_id_t id, void *arg)
{
	api_audio_stream_t	*st;
	api_audio_position_t	pos;
	pc_stream_t		*stream;

	st = api_audio_stream_slot(id);
	if (st == NULL || arg == NULL) {
		return (-1);
	}
	memset(&pos, 0, sizeof(pos));
	stream = (pc_stream_t *)st->stream;
	if (stream != NULL) {
		pos.play_offset = stream->ring.read_cursor;
		pos.write_offset = stream->ring.write_cursor;
	}
	memcpy(arg, &pos, sizeof(pos));
	return (0);
}

static int
api_audio_stream_set_state(entity_id_t id, void *arg)
{
	api_audio_stream_t	*st;
	pc_port_t		*port;
	pc_stream_t		*stream;
	u32			state;

	st = api_audio_stream_slot(id);
	if (st == NULL || arg == NULL) {
		return (-1);
	}
	state = *(const u32 *)arg;
	if (state > KS_STATE_RUN) {
		return (-1);
	}
	st->state = state;
	port = (pc_port_t *)st->port;
	stream = api_audio_stream_ensure(st);
	if (stream == NULL || port == NULL) {
		return (0);
	}
	if (state == KS_STATE_RUN) {
		pc_port_start(port, stream);
	} else {
		pc_port_stop(port, stream);
	}
	return (0);
}

static int
api_audio_stream_get_volume(entity_id_t id, void *arg)
{
	api_audio_stream_t	*st;

	st = api_audio_stream_slot(id);
	if (st == NULL || arg == NULL) {
		return (-1);
	}
	memset(arg, 0, sizeof(u32));
	*(u32 *)arg = st->volume;
	return (0);
}

static int
api_audio_stream_set_volume(entity_id_t id, void *arg)
{
	api_audio_stream_t	*st;
	pc_stream_t		*stream;
	u32			volume;

	st = api_audio_stream_slot(id);
	if (st == NULL || arg == NULL) {
		return (-1);
	}
	volume = *(const u32 *)arg;
	if (volume > AUDIO_VOLUME_MAX) {
		volume = AUDIO_VOLUME_MAX;
	}
	st->volume = volume;
	stream = (pc_stream_t *)st->stream;
	if (stream != NULL) {
		(void)pc_stream_set_volume(stream, volume);
	}
	return (0);
}

static int
api_audio_stream_get_info(entity_id_t id, void *arg)
{
	api_audio_stream_t	*st;
	api_audio_info_t	info;

	st = api_audio_stream_slot(id);
	if (st == NULL || arg == NULL) {
		return (-1);
	}
	memset(&info, 0, sizeof(info));
	info.id = st->pin_id;
	info.flow = st->flow;
	info.state = st->state;
	info.formats_count = 1;
	info.channels = st->format.channels;
	info.rate = st->format.rate;
	info.container = st->format.container;
	info.valid_bits = st->format.valid_bits;
	memcpy(info.name, st->name, sizeof(info.name));
	memcpy(arg, &info, sizeof(info));
	return (0);
}

static int
api_audio_ioctl_stream(entity_id_t id, u64 cmd, void *arg)
{
	switch (cmd) {
	case AUDIO_IOCTL_GET_INFO:
		return (api_audio_stream_get_info(id, arg));
	case AUDIO_IOCTL_SET_FORMAT:
		return (api_audio_stream_set_format(id, arg));
	case AUDIO_IOCTL_GET_POSITION:
		return (api_audio_stream_get_position(id, arg));
	case AUDIO_IOCTL_SET_STATE:
		return (api_audio_stream_set_state(id, arg));
	case AUDIO_IOCTL_GET_VOLUME:
		return (api_audio_stream_get_volume(id, arg));
	case AUDIO_IOCTL_SET_VOLUME:
		return (api_audio_stream_set_volume(id, arg));
	default:
		return (-1);
	}
}

static int
api_audio_open_stream(entity_id_t id, void *arg)
{
	api_audio_entry_t	*ep;
	api_audio_stream_t	*st;
	entity_id_t		sid;
	struct process		*proc;
	u32			i;
	u32			access;
	int			handle;

	(void)arg;
	ep = api_audio_slot(id);
	if (ep == NULL) {
		return (-1);
	}
	for (i = 0; i < API_AUDIO_MAX_STREAMS; i++) {
		if (!api_audio_streams[i].used) {
			break;
		}
	}
	if (i >= API_AUDIO_MAX_STREAMS) {
		return (-1);
	}
	st = &api_audio_streams[i];
	memset(st, 0, sizeof(*st));
	st->used = 1;
	st->port = ep->port;
	st->flow = ep->flow;
	st->pin_id = ep->pin_id;
	st->state = KS_STATE_STOP;
	st->volume = ep->volume;
	st->format = ep->format;
	memcpy(st->name, ep->name, sizeof(st->name));

	proc = process_current();
	access = ENTITY_ACCESS_READ | ENTITY_ACCESS_WRITE;
	sid = entity_create(ENTITY_ARCH_AUDIO_STREAM, 0,
	    proc ? proc->pid : 0,
	    proc ? proc->uid : 0, proc ? proc->gid : 0,
	    proc ? proc->euid : 0, proc ? proc->egid : 0,
	    proc ? proc->kusr_auth : 0);
	if (sid == 0) {
		st->used = 0;
		return (-1);
	}
	entity_io_set_i32(sid, 0, (s32)i);
	handle = entity_handle_alloc(proc, sid, access);
	if (handle < 0) {
		entity_destroy(sid);
		st->used = 0;
		return (-1);
	}
	entity_release(sid);
	return (handle);
}

static void
api_audio_stream_release(entity_id_t id)
{
	api_audio_stream_t	*st;
	pc_port_t		*port;
	pc_stream_t		*stream;

	st = api_audio_stream_slot(id);
	if (st == NULL) {
		return;
	}
	port = (pc_port_t *)st->port;
	stream = (pc_stream_t *)st->stream;
	if (port != NULL && stream != NULL) {
		if (stream->drain_stop) {
			stream->release_pending = 1;
			st->stream = NULL;
			memset(st, 0, sizeof(*st));
			return;
		}
		(void)pc_port_abort(port, stream);
		(void)pc_port_close_stream(port, stream);
	}
	memset(st, 0, sizeof(*st));
}

static const entity_io_ops_t api_audio_ops = {
	.read	= api_audio_read,
	.write	= api_audio_write,
	.seek	= NULL,
	.ioctl	= api_audio_ioctl,
	.stat	= NULL,
};

static const entity_io_ops_t api_audio_stream_ops = {
	.read	= api_audio_read_stream,
	.write	= api_audio_write_stream,
	.seek	= NULL,
	.ioctl	= api_audio_ioctl_stream,
	.stat	= NULL,
};

void
api_audio_init(void)
{
	if (api_audio_io_ops != NULL) {
		return;
	}
	memset(api_audio_entries, 0, sizeof(api_audio_entries));
	memset(api_audio_streams, 0, sizeof(api_audio_streams));
	entity_arch_release_register(ENTITY_ARCH_AUDIO, api_audio_release);
	entity_arch_release_register(ENTITY_ARCH_AUDIO_STREAM,
	    api_audio_stream_release);
	entity_arch_io_register(ENTITY_ARCH_AUDIO, &api_audio_ops);
	entity_arch_io_register(ENTITY_ARCH_AUDIO_STREAM,
	    &api_audio_stream_ops);
	api_audio_io_ops = &api_audio_ops;
	api_audio_stream_io_ops = &api_audio_stream_ops;
}

int
api_audio_publish(const char *name, u32 flow, u32 pin_id, void *port)
{
	api_audio_entry_t	*entry;
	entity_id_t		id;
	char			path[AUDIO_NAME_MAX + 32];
	u32			i;

	if (name == NULL || name[0] == '\0') {
		return (-1);
	}
	for (i = 0; i < AUDIO_MAX_ENDPOINTS; i++) {
		if (!api_audio_entries[i].used) {
			break;
		}
	}
	if (i >= AUDIO_MAX_ENDPOINTS) {
		return (-1);
	}
	entry = &api_audio_entries[i];
	memset(entry, 0, sizeof(*entry));
	strncpy(entry->name, name, AUDIO_NAME_MAX - 1);
	entry->name[AUDIO_NAME_MAX - 1] = '\0';
	entry->port = port;
	entry->flow = flow;
	entry->pin_id = pin_id;
	entry->state = KS_STATE_STOP;
	entry->volume = AUDIO_VOLUME_UNITY;
	entry->format.container = KS_DATARANGE_PCM_S16LE;
	entry->format.valid_bits = 16;
	entry->format.channels = 2;
	entry->format.rate = 48000;
	entry->format.channel_mask = 0x3;
	entry->position.play_offset = 0;
	entry->position.write_offset = 0;
	entry->used = 1;

	id = entity_create(ENTITY_ARCH_AUDIO, 0, 0, 0, 0, 0, 0, 1);
	if (id == 0) {
		drivers_log("[AUDIO] entity_create failed for '%s'\n", name);
		entry->used = 0;
		return (-1);
	}
	entity_io_set_i32(id, 0, (s32)i);
	snprintf(path, sizeof(path), "/Entity/Interface/Audio/%s", name);
	drivers_log("[AUDIO] binding entity id=%llu path=%s\n",
	    (unsigned long long)id, path);
	if (entity_ns_bind(path, id) != 0) {
		drivers_log("[AUDIO] entity_ns_bind failed for %s\n", path);
		entity_destroy(id);
		entry->used = 0;
		return (-1);
	}
	drivers_log("[AUDIO] endpoint %s\n", path);
	return (0);
}

void
api_audio_unpublish_port(void *port)
{
	api_audio_entry_t	*entry;
	entity_id_t		id;
	u32			i;

	for (i = 0; i < AUDIO_MAX_ENDPOINTS; i++) {
		entry = &api_audio_entries[i];
		if (!entry->used || entry->port != port) {
			continue;
		}
		id = entity_id_at(ENTITY_ARCH_AUDIO, i);
		if (id != 0 && entity_valid(id)) {
			entity_ns_unbind_all_id(id);
			entity_destroy(id);
		}
		memset(entry, 0, sizeof(*entry));
		entry->used = 0;
	}
}

u32
api_audio_endpoint_count(void)
{
	u32	i, count;

	count = 0;
	for (i = 0; i < AUDIO_MAX_ENDPOINTS; i++) {
		if (api_audio_entries[i].used) {
			count++;
		}
	}
	return (count);
}
