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
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type api_audio_info_t as public endpoint descriptor
$define %type api_audio_format_t as public format descriptor
$define %type api_audio_position_t as public cursor snapshot
$define %type s32 as 32 bit signed
$define %type u32 as 32 bit unsigned

$define %func api_audio_init as procedure with args void
$define %func api_audio_publish as function with args const char *, uint, uint, void *
$define %func api_audio_endpoint_count as function with args void
$define %func api_audio_slot as function with args entity id
$define %func api_audio_ioctl as function with args entity id, command, argument
$define %func api_audio_read as function with args entity id, buffer, count, offset
$define %func api_audio_write as function with args entity id, buffer, count, offset
$define %func api_audio_release as procedure with args entity id
*/

/* !SPACE!

$space %internal api_audio_slot, api_audio_ioctl
$space %internal api_audio_read, api_audio_write, api_audio_release
$space %export api_audio_init, api_audio_publish
$space %export api_audio_endpoint_count

*/

#include <kernel/audio/api/api_audio.h>
#include <kernel/audio/portcls/pcport.h>
#include <kernel/api/api.h>
#include <kernel/entity/entity.h>
#include <mlibc/stdio.h>

typedef struct api_audio_entry {
	char			name[AUDIO_NAME_MAX];
	void			*port;
	void			*stream;
	u32			flow;
	u32			pin_id;
	u32			state;
	u32			used;
	api_audio_format_t	format;
	api_audio_position_t	position;
} api_audio_entry_t;

static api_audio_entry_t	api_audio_entries[AUDIO_MAX_ENDPOINTS];
static const entity_io_ops_t	*api_audio_io_ops;

_Static_assert(sizeof(api_audio_info_t) == 96, "api_audio_info_t ABI size");
_Static_assert(sizeof(api_audio_format_t) == 20, "api_audio_format_t ABI size");
_Static_assert(sizeof(api_audio_position_t) == 16, "api_audio_position_t ABI size");
_Static_assert(__builtin_offsetof(api_audio_info_t, name) == 32,
    "api_audio_info_t name offset");

static api_audio_entry_t *
api_audio_slot(entity_id_t id)
{
	api_audio_entry_t	*entry;
	s32			index;

	if (id == 0 || entity_arch(id) != ENTITY_ARCH_AUDIO) {
		return (NULL);
	}
	if (entity_io_i32(id, 0, &index) != 0 || index < 0 ||
	    index >= AUDIO_MAX_ENDPOINTS) {
		return (NULL);
	}
	entry = &api_audio_entries[index];
	if (!entry->used) {
		return (NULL);
	}
	return (entry);
}


static void *
api_audio_stream_ensure(api_audio_entry_t *entry)
{
	pc_port_t	*port;
	ks_format_t	fmt;
	u32		flags;

	if (entry == NULL) {
		return (NULL);
	}
	if (entry->stream != NULL) {
		return (entry->stream);
	}
	port = (pc_port_t *)entry->port;
	if (port == NULL) {
		return (NULL);
	}
	fmt.container = (u32)entry->format.container;
	fmt.valid_bits = (u32)entry->format.valid_bits;
	fmt.channels = (u32)entry->format.channels;
	fmt.rate = (u32)entry->format.rate;
	fmt.channel_mask = (u32)entry->format.channel_mask;
	flags = (entry->flow == KS_DATAFLOW_OUT) ? PC_STREAM_RENDER :
	    PC_STREAM_CAPTURE;
	entry->stream = pc_port_open_stream(port, &fmt, flags);
	return (entry->stream);
}

static int
api_audio_write(entity_id_t id, const void *buf, u64 count, u64 offset)
{
	api_audio_entry_t	*entry;
	pc_stream_t		*stream;

	(void)offset;
	entry = api_audio_slot(id);
	if (entry == NULL || entry->flow != KS_DATAFLOW_OUT) {
		return (0);
	}
	stream = (pc_stream_t *)api_audio_stream_ensure(entry);
	if (stream == NULL) {
		return (0);
	}
	return ((int)pc_stream_write(stream, (const u8 *)buf, count));
}

static int
api_audio_read(entity_id_t id, void *buf, u64 count, u64 offset)
{
	api_audio_entry_t	*entry;
	pc_stream_t		*stream;

	(void)offset;
	entry = api_audio_slot(id);
	if (entry == NULL || entry->flow != KS_DATAFLOW_IN) {
		return (0);
	}
	stream = (pc_stream_t *)api_audio_stream_ensure(entry);
	if (stream == NULL) {
		return (0);
	}
	return ((int)pc_stream_read(stream, (u8 *)buf, count));
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
api_audio_set_format(entity_id_t id, void *arg)
{
	api_audio_entry_t	*entry;
	api_audio_format_t	format;

	entry = api_audio_slot(id);
	if (entry == NULL || arg == NULL) {
		return (-1);
	}
	memcpy(&format, arg, sizeof(format));
	if (format.channels == 0 || format.channels > KS_MAX_CHANNELS ||
	    format.container == 0 || format.rate == 0) {
		return (-1);
	}
	entry->format = format;
	return (0);
}

static int
api_audio_get_position(entity_id_t id, void *arg)
{
	api_audio_entry_t	*entry;

	entry = api_audio_slot(id);
	if (entry == NULL || arg == NULL) {
		return (-1);
	}
	memcpy(arg, &entry->position, sizeof(entry->position));
	return (0);
}

static int
api_audio_set_state(entity_id_t id, void *arg)
{
	api_audio_entry_t	*entry;
	pc_port_t		*port;
	pc_stream_t		*stream;
	u32			state;

	entry = api_audio_slot(id);
	if (entry == NULL || arg == NULL) {
		return (-1);
	}
	state = *(const u32 *)arg;
	if (state > KS_STATE_RUN) {
		return (-1);
	}
	entry->state = state;
	port = (pc_port_t *)entry->port;
	stream = (pc_stream_t *)api_audio_stream_ensure(entry);
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
api_audio_ioctl(entity_id_t id, u64 cmd, void *arg)
{
	switch (cmd) {
	case AUDIO_IOCTL_GET_INFO:
		return (api_audio_info_fill(id, arg));
	case AUDIO_IOCTL_SET_FORMAT:
		return (api_audio_set_format(id, arg));
	case AUDIO_IOCTL_GET_POSITION:
		return (api_audio_get_position(id, arg));
	case AUDIO_IOCTL_SET_STATE:
		return (api_audio_set_state(id, arg));
	default:
		return (-1);
	}
}

static void
api_audio_release(entity_id_t id)
{
	(void)id;
}

static const entity_io_ops_t api_audio_ops = {
	.read	= api_audio_read,
	.write	= api_audio_write,
	.seek	= NULL,
	.ioctl	= api_audio_ioctl,
	.stat	= NULL,
};

void
api_audio_init(void)
{
	if (api_audio_io_ops != NULL) {
		return;
	}
	memset(api_audio_entries, 0, sizeof(api_audio_entries));
	entity_arch_release_register(ENTITY_ARCH_AUDIO, api_audio_release);
	entity_arch_io_register(ENTITY_ARCH_AUDIO, &api_audio_ops);
	api_audio_io_ops = &api_audio_ops;
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
	strncpy(entry->name, name, AUDIO_NAME_MAX - 1);
	entry->name[AUDIO_NAME_MAX - 1] = '\0';
	entry->port = port;
	entry->stream = NULL;
	entry->flow = flow;
	entry->pin_id = pin_id;
	entry->state = KS_STATE_STOP;
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
