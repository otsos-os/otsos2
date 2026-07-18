/* !DEFINES!

$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type process_t as struct with process control block
$define %type api_trace_handle_t as native trace handle slot
$define %type trace_session_filter_t as core trace session predicate set

$define %func api_trace_user_range as function with args void *, size_t, int
$define %func api_trace_copy_name as procedure with args char *, u32, char *
$define %func api_trace_alloc_handle as function with args void
$define %func api_trace_get_handle as function with args int
$define %func api_trace_default_filter as procedure with args filter
$define %func api_trace_filter_from_user as function with args filter, user
$define %func api_trace_filter_allowed as function with args filter, process
$define %func api_trace_fill_stats as procedure with args api stats
$define %func api_trace_fill_event as procedure with args api event, desc
$define %func api_trace_fill_source as procedure with args api source, u16
$define %func api_trace_info_stats as function with args api stats *
$define %func api_trace_info_events as function with args api events *
$define %func api_trace_info_sources as function with args api sources *
$define %func api_trace_info_pmu as function with args api pmu *
$define %func api_trace_open as function with args u32
$define %func api_trace_close as function with args int
$define %func api_trace_read as function with args int, api read *
$define %func api_trace_ctl as function with args int, u32, void *
$define %func api_trace_info as function with args u32, void *
$define %func api_trace_mark as function with args u32, u64, u64, u64, u64, u64
$define %func api_trace_cleanup_process as procedure with args process_t *

*/

/* !SPACE!

$space %internal api_trace_user_range, api_trace_copy_name
$space %internal api_trace_alloc_handle, api_trace_get_handle
$space %internal api_trace_default_filter, api_trace_filter_from_user
$space %internal api_trace_filter_allowed, api_trace_fill_stats
$space %internal api_trace_fill_event, api_trace_fill_source
$space %internal api_trace_info_stats, api_trace_info_events
$space %internal api_trace_info_sources, api_trace_info_pmu
$space %export api_trace_open, api_trace_close, api_trace_read
$space %export api_trace_ctl, api_trace_info, api_trace_mark
$space %export api_trace_cleanup_process

*/

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

#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/trace/trace.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

#define	API_TRACE_MAX_HANDLES	32
#define	API_TRACE_OPEN_KNOWN \
	(API_TRACE_OPEN_SYSTEM | API_TRACE_OPEN_KERNEL_STACK)

typedef struct api_trace_handle {
	int	used;
	int	session_id;
	u32	owner_pid;
	u32	flags;
} api_trace_handle_t;

typedef char api_trace_record_size_check[
	(sizeof(struct api_trace_record) == sizeof(trace_record_t)) ? 1 : -1];

static api_trace_handle_t	g_api_trace_handles[API_TRACE_MAX_HANDLES];

static int
api_trace_user_range(void *ptr, size_t size, int write)
{
	if (size == 0) {
		return (1);
	}
	if (!is_user_address(ptr, size)) {
		return (0);
	}
	return (user_range_fault_in(ptr, size, write));
}

static void
api_trace_copy_name(char *dst, u32 size, const char *src)
{
	u32	i;

	if (size == 0) {
		return;
	}
	memset(dst, 0, size);
	if (src == NULL) {
		return;
	}
	for (i = 0; i + 1 < size && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}
}

static int
api_trace_alloc_handle(void)
{
	int	i;

	for (i = 0; i < API_TRACE_MAX_HANDLES; i++) {
		if (!g_api_trace_handles[i].used) {
			return (i);
		}
	}
	return (-API_ERR_HANDLES_FULL);
}

static api_trace_handle_t *
api_trace_get_handle(int trace)
{
	api_trace_handle_t	*handle;
	process_t		*proc;

	if (trace < 0 || trace >= API_TRACE_MAX_HANDLES) {
		return (NULL);
	}
	handle = &g_api_trace_handles[trace];
	if (!handle->used) {
		return (NULL);
	}

	proc = process_current();
	if (proc == NULL || handle->owner_pid != proc->pid) {
		return (NULL);
	}
	return (handle);
}

static void
api_trace_default_filter(trace_session_filter_t *filter)
{
	int	i;

	memset(filter, 0, sizeof(*filter));
	filter->source_mask = TRACE_SOURCE_MASK_ALL;
	filter->pid = -1;
	filter->tid = -1;
	filter->cpu = -1;
	for (i = 0; i < TRACE_EVENT_WORDS; i++) {
		filter->event_mask[i] = ~0ULL;
	}
}

static int
api_trace_filter_from_user(trace_session_filter_t *dst,
    const struct api_trace_filter *src)
{
	u32	known_flags;
	int	i;

	known_flags = API_TRACE_FILTER_HAS_PID | API_TRACE_FILTER_HAS_TID |
	    API_TRACE_FILTER_HAS_CPU;
	if ((src->flags & ~known_flags) != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if ((src->source_mask & ~TRACE_SOURCE_MASK_ALL) != 0) {
		return (-API_ERR_BAD_VALUE);
	}

	memset(dst, 0, sizeof(*dst));
	dst->source_mask = src->source_mask;
	for (i = 0; i < TRACE_EVENT_WORDS; i++) {
		dst->event_mask[i] = src->event_mask[i];
	}
	dst->pid = -1;
	dst->tid = -1;
	dst->cpu = -1;
	dst->flags = src->flags;

	if (src->flags & API_TRACE_FILTER_HAS_PID) {
		if (src->pid < 0) {
			return (-API_ERR_BAD_VALUE);
		}
		dst->pid = src->pid;
	}
	if (src->flags & API_TRACE_FILTER_HAS_TID) {
		if (src->tid < 0) {
			return (-API_ERR_BAD_VALUE);
		}
		dst->tid = src->tid;
	}
	if (src->flags & API_TRACE_FILTER_HAS_CPU) {
		if (src->cpu < 0 || src->cpu >= TRACE_MAX_CPUS) {
			return (-API_ERR_BAD_VALUE);
		}
		dst->cpu = src->cpu;
	}
	return (0);
}

static int
api_trace_filter_allowed(const trace_session_filter_t *filter,
    process_t *proc)
{
	if (proc_has_privilege(proc)) {
		return (1);
	}
	if (proc == NULL) {
		return (0);
	}
	if ((filter->flags & TRACE_FILTER_HAS_PID) == 0) {
		return (0);
	}
	if (filter->pid != (int)proc->pid) {
		return (0);
	}
	return (1);
}

static void
api_trace_fill_stats(struct api_trace_stats *out)
{
	trace_stats_t	stats;
	u32		i;

	memset(out, 0, sizeof(*out));
	trace_get_stats(&stats);
	out->records_written = stats.records_written;
	out->records_lost = stats.records_lost;
	out->ring_records = stats.ring_records;
	out->session_count = stats.session_count;
	out->enabled = stats.enabled;
	out->initialized = stats.initialized;
	for (i = 0; i < API_TRACE_MAX_EVENTS; i++) {
		out->event_count[i] = stats.event_count[i];
	}
	for (i = 0; i < API_TRACE_SOURCE_COUNT; i++) {
		out->source_count[i] = stats.source_count[i];
	}
}

static void
api_trace_fill_event(struct api_trace_event *out,
    const trace_event_desc_t *desc)
{
	const trace_field_desc_t	*field;
	u32			i, count;

	memset(out, 0, sizeof(*out));
	out->id = desc->id;
	out->source = desc->source;
	out->flags = desc->flags;
	out->enabled = trace_event_enabled(desc->id) ? 1 : 0;
	api_trace_copy_name(out->provider, API_TRACE_PROVIDER_LEN,
	    desc->provider);
	api_trace_copy_name(out->name, API_TRACE_NAME_LEN, desc->name);

	count = desc->field_count;
	if (desc->fields == NULL) {
		count = 0;
	}
	if (count > API_TRACE_MAX_FIELDS) {
		count = API_TRACE_MAX_FIELDS;
	}
	out->field_count = count;
	for (i = 0; i < count; i++) {
		field = &desc->fields[i];
		api_trace_copy_name(out->fields[i].name, API_TRACE_NAME_LEN,
		    field->name);
		out->fields[i].index = field->index;
		out->fields[i].flags = field->flags;
	}
}

static void
api_trace_fill_source(struct api_trace_source *out, u16 source)
{
	memset(out, 0, sizeof(*out));
	out->id = source;
	out->enabled = trace_source_enabled(source) ? 1 : 0;
	api_trace_copy_name(out->name, API_TRACE_NAME_LEN,
	    trace_source_name(source));
}

static int
api_trace_info_stats(struct api_trace_stats *uarg)
{
	struct api_trace_stats	out;

	if (!api_trace_user_range(uarg, sizeof(*uarg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	api_trace_fill_stats(&out);
	*uarg = out;
	return (0);
}

static int
api_trace_info_events(struct api_trace_events *uarg)
{
	struct api_trace_events	 local;
	const trace_event_desc_t	*desc;
	size_t			 bytes;
	u32			 i, max, total, written;

	if (!api_trace_user_range(uarg, sizeof(*uarg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	local = *uarg;
	max = local.max_events;
	if (max > API_TRACE_MAX_EVENTS) {
		max = API_TRACE_MAX_EVENTS;
	}
	if (max > 0) {
		bytes = (size_t)max * sizeof(struct api_trace_event);
		if (!api_trace_user_range(local.events, bytes, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
	}

	total = 0;
	written = 0;
	for (i = 0; i < TRACE_MAX_EVENTS; i++) {
		desc = trace_event_desc((u16)i);
		if (desc == NULL) {
			continue;
		}
		if (written < max) {
			api_trace_fill_event(&local.events[written], desc);
			written++;
		}
		total++;
	}
	uarg->count = total;
	return (0);
}

static int
api_trace_info_sources(struct api_trace_sources *uarg)
{
	struct api_trace_sources	local;
	size_t			bytes;
	u32			i, max;

	if (!api_trace_user_range(uarg, sizeof(*uarg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	local = *uarg;
	max = local.max_sources;
	if (max > TRACE_SOURCE_COUNT) {
		max = TRACE_SOURCE_COUNT;
	}
	if (max > 0) {
		bytes = (size_t)max * sizeof(struct api_trace_source);
		if (!api_trace_user_range(local.sources, bytes, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
	}

	for (i = 0; i < max; i++) {
		api_trace_fill_source(&local.sources[i], (u16)i);
	}
	uarg->count = TRACE_SOURCE_COUNT;
	return (0);
}

static int
api_trace_info_pmu(struct api_trace_pmu *uarg)
{
	struct api_trace_pmu	local;
	size_t			bytes;
	u32			i, max, total;

	if (!api_trace_user_range(uarg, sizeof(*uarg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	local = *uarg;
	total = trace_pmu_counter_count();
	max = local.max_counters;
	if (max > total) {
		max = total;
	}
	if (max > API_TRACE_MAX_PMU_COUNTERS) {
		max = API_TRACE_MAX_PMU_COUNTERS;
	}
	if (max > 0) {
		bytes = (size_t)max * sizeof(struct api_trace_pmu_counter);
		if (!api_trace_user_range(local.counters, bytes, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
	}

	for (i = 0; i < max; i++) {
		memset(&local.counters[i], 0,
		    sizeof(struct api_trace_pmu_counter));
		local.counters[i].id = i;
		local.counters[i].enabled =
		    trace_pmu_counter_active(i) ? 1 : 0;
		api_trace_copy_name(local.counters[i].name,
		    API_TRACE_NAME_LEN, trace_pmu_counter_name(i));
	}
	uarg->count = total;
	uarg->source_enabled = trace_source_enabled(TRACE_SOURCE_PMU) ? 1 : 0;
	uarg->events_enabled =
	    (trace_event_enabled(TRACE_EV_PROFILE_SAMPLE) ||
	    trace_event_enabled(TRACE_EV_PMU_COUNTERS)) ? 1 : 0;
	return (0);
}

int
api_trace_open(u32 flags)
{
	api_trace_handle_t	*handle;
	process_t		*proc;
	trace_session_filter_t	filter;
	int			trace, session;

	if ((flags & ~API_TRACE_OPEN_KNOWN) != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!trace_is_initialized()) {
		return (-API_ERR_NODEV);
	}

	proc = process_current();
	if (!proc_has_privilege(proc) &&
	    (flags & API_TRACE_OPEN_KNOWN) != 0) {
		return (-API_ERR_PERM);
	}

	trace = api_trace_alloc_handle();
	if (trace < 0) {
		return (trace);
	}
	session = trace_session_open(flags);
	if (session < 0) {
		return (-API_ERR_BUSY);
	}

	handle = &g_api_trace_handles[trace];
	memset(handle, 0, sizeof(*handle));
	handle->used = 1;
	handle->session_id = session;
	handle->flags = flags;
	if (proc != NULL) {
		handle->owner_pid = proc->pid;
	}

	if (proc != NULL &&
	    (!proc_has_privilege(proc) ||
	    (flags & API_TRACE_OPEN_SYSTEM) == 0)) {
		api_trace_default_filter(&filter);
		filter.flags |= TRACE_FILTER_HAS_PID;
		filter.pid = (int)proc->pid;
		if (trace_session_filter(session, &filter) != 0) {
			trace_session_close(session);
			memset(handle, 0, sizeof(*handle));
			return (-API_ERR_BAD_VALUE);
		}
	}
	return (trace);
}

int
api_trace_close(int trace)
{
	api_trace_handle_t	*handle;

	handle = api_trace_get_handle(trace);
	if (handle == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	trace_session_close(handle->session_id);
	memset(handle, 0, sizeof(*handle));
	return (0);
}

int
api_trace_read(int trace, struct api_trace_read *uarg)
{
	api_trace_handle_t	*handle;
	struct api_trace_read	 local;
	trace_session_stats_t	 stats;
	size_t			 bytes;
	int			 ret;

	handle = api_trace_get_handle(trace);
	if (handle == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (!api_trace_user_range(uarg, sizeof(*uarg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}

	local = *uarg;
	if (local.max_records > API_TRACE_READ_MAX_RECORDS) {
		return (-API_ERR_TOO_BIG);
	}
	if (local.max_records > 0) {
		bytes = (size_t)local.max_records *
		    sizeof(struct api_trace_record);
		if (!api_trace_user_range(local.records, bytes, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
	}

	ret = 0;
	if (local.max_records > 0) {
		ret = trace_session_read(handle->session_id,
		    (trace_record_t *)local.records, local.max_records);
		if (ret < 0) {
			return (-API_ERR_BAD_HANDLE);
		}
	}
	memset(&stats, 0, sizeof(stats));
	trace_session_stats(handle->session_id, &stats);
	uarg->records_read = (u32)ret;
	uarg->read_records = stats.read_records;
	uarg->lost_records = stats.lost_records;
	return (ret);
}

int
api_trace_ctl(int trace, u32 op, void *arg)
{
	api_trace_handle_t	*handle;
	process_t		*proc;
	trace_session_filter_t	filter;
	struct api_trace_filter	user_filter;
	struct api_trace_toggle	toggle;
	int			ret;

	handle = api_trace_get_handle(trace);
	if (handle == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	proc = process_current();

	switch (op) {
	case API_TRACE_OP_START:
		ret = trace_session_start(handle->session_id);
		break;
	case API_TRACE_OP_STOP:
		ret = trace_session_stop(handle->session_id);
		break;
	case API_TRACE_OP_FLUSH:
		ret = trace_session_flush(handle->session_id);
		break;
	case API_TRACE_OP_SET_FILTER:
		if (!api_trace_user_range(arg, sizeof(user_filter), 0)) {
			return (-API_ERR_BAD_ADDR);
		}
		user_filter = *(struct api_trace_filter *)arg;
		ret = api_trace_filter_from_user(&filter, &user_filter);
		if (ret != 0) {
			return (ret);
		}
		if (!api_trace_filter_allowed(&filter, proc)) {
			return (-API_ERR_PERM);
		}
		ret = trace_session_filter(handle->session_id, &filter);
		break;
	case API_TRACE_OP_ENABLE_EVENT:
		if (!api_trace_user_range(arg, sizeof(toggle), 0)) {
			return (-API_ERR_BAD_ADDR);
		}
		toggle = *(struct api_trace_toggle *)arg;
		ret = trace_session_enable_event(handle->session_id,
		    (u16)toggle.id, toggle.enabled != 0);
		break;
	case API_TRACE_OP_ENABLE_SOURCE:
		if (!api_trace_user_range(arg, sizeof(toggle), 0)) {
			return (-API_ERR_BAD_ADDR);
		}
		toggle = *(struct api_trace_toggle *)arg;
		ret = trace_session_enable_source(handle->session_id,
		    (u16)toggle.id, toggle.enabled != 0);
		break;
	case API_TRACE_OP_SET_PMU:
	case API_TRACE_OP_LOAD_PROGRAM:
	case API_TRACE_OP_UNLOAD_PROGRAM:
		return (-API_ERR_NOT_SUPPORTED);
	default:
		return (-API_ERR_BAD_VALUE);
	}

	if (ret != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	return (0);
}

int
api_trace_info(u32 op, void *arg)
{
	switch (op) {
	case API_TRACE_INFO_STATS:
		return (api_trace_info_stats((struct api_trace_stats *)arg));
	case API_TRACE_INFO_EVENTS:
		return (api_trace_info_events((struct api_trace_events *)arg));
	case API_TRACE_INFO_SOURCES:
		return (api_trace_info_sources(
		    (struct api_trace_sources *)arg));
	case API_TRACE_INFO_PMU:
		return (api_trace_info_pmu((struct api_trace_pmu *)arg));
	default:
		return (-API_ERR_BAD_VALUE);
	}
}

int
api_trace_mark(u32 id, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4)
{
	trace_emit(TRACE_EV_USER_MARK, TRACE_REC_F_USER, NULL, id,
	    a0, a1, a2, a3, a4);
	return (0);
}

void
api_trace_cleanup_process(struct process *proc)
{
	api_trace_handle_t	*handle;
	u32			pid;
	int			i;

	if (proc == NULL) {
		return;
	}
	pid = proc->pid;
	for (i = 0; i < API_TRACE_MAX_HANDLES; i++) {
		handle = &g_api_trace_handles[i];
		if (!handle->used || handle->owner_pid != pid) {
			continue;
		}
		trace_session_close(handle->session_id);
		memset(handle, 0, sizeof(*handle));
	}
}
