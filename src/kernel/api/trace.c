/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type process_t as struct with process control block
$define %type api_trace_handle_t as native trace handle slot
$define %type trace_program_spec_t as kernel trace program descriptor

$define %func api_trace_user_range as function with args void *, size_t, int
$define %func api_trace_copy_name as procedure with args char *, u32, char *
$define %func api_trace_alloc_handle as function with args void
$define %func api_trace_get_handle as function with args int
$define %func api_trace_program_from_user as procedure with args spec, user
$define %func api_trace_fill_stats as procedure with args api stats
$define %func api_trace_fill_provider as procedure with args api provider, info
$define %func api_trace_fill_probe as procedure with args api probe, info
$define %func api_trace_info_stats as function with args api stats *
$define %func api_trace_info_providers as function with args api providers *
$define %func api_trace_info_probes as function with args api probes *
$define %func api_trace_info_pmu as function with args api pmu *
$define %func api_trace_info_aggs as function with args api aggs *
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
$space %internal api_trace_program_from_user
$space %internal api_trace_fill_stats, api_trace_fill_provider
$space %internal api_trace_fill_probe
$space %internal api_trace_info_stats, api_trace_info_providers
$space %internal api_trace_info_probes, api_trace_info_pmu
$space %internal api_trace_info_aggs
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
	(API_TRACE_OPEN_PRIVILEGED | API_TRACE_OPEN_KERNEL_STACK)

typedef struct api_trace_handle {
	int	used;
	int	session_id;
	u32	owner_pid;
	u32	flags;
	entity_id_t	entity;
	int	entity_handle;
} api_trace_handle_t;

typedef char api_trace_record_size_check[
	(sizeof(struct api_trace_record) == sizeof(trace_record_t)) ? 1 : -1];
typedef char api_trace_agg_size_check[
	(sizeof(struct api_trace_agg) == sizeof(trace_aggregation_t)) ? 1 : -1];

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
api_trace_alloc_handle(process_t *proc)
{
	int	i, handle;

	for (i = 0; i < API_TRACE_MAX_HANDLES; i++) {
		if (!g_api_trace_handles[i].used) {
			entity_id_t	id;

			id = entity_io_create_raw(ENTITY_ARCH_TRACE, 0);
			if (id == 0) {
				return (-API_ERR_NO_MEMORY);
			}
			entity_io_set_ptr(id, ENTITY_IO_PTR_BACKING,
			    &g_api_trace_handles[i]);
			handle = entity_io_attach(id,
			    ENTITY_ACCESS_READ | ENTITY_ACCESS_WRITE);
			if (handle < 0) {
				entity_destroy(id);
				return (handle);
			}
			memset(&g_api_trace_handles[i], 0,
			    sizeof(g_api_trace_handles[i]));
			g_api_trace_handles[i].used = 1;
			g_api_trace_handles[i].entity = id;
			g_api_trace_handles[i].entity_handle = handle;
			if (proc) {
				g_api_trace_handles[i].owner_pid =
				    proc->pid;
			}
			return (handle);
		}
	}
	return (-API_ERR_HANDLES_FULL);
}

static api_trace_handle_t *
api_trace_get_handle(int trace)
{
	api_trace_handle_t	*handle;
	process_t		*proc;
	entity_id_t		id;
	u32			access;
	int			ret;

	proc = process_current();
	ret = entity_handle_lookup(proc, trace, &id, &access);
	if (ret != 0) {
		return (NULL);
	}
	if (entity_arch(id) != ENTITY_ARCH_TRACE) {
		return (NULL);
	}
	handle = (api_trace_handle_t *)entity_io_ptr(id,
	    ENTITY_IO_PTR_BACKING);
	if (!handle || !handle->used) {
		return (NULL);
	}
	if (proc == NULL || handle->owner_pid != proc->pid) {
		return (NULL);
	}
	return (handle);
}

static void
api_trace_program_from_user(trace_program_spec_t *dst,
    const struct api_trace_program *src)
{
	u32	i;

	memset(dst, 0, sizeof(*dst));
	dst->probe_id = src->probe_id;
	dst->flags = src->flags;
	dst->predicate_count = src->predicate_count;
	dst->action_count = src->action_count;
	for (i = 0; i < API_TRACE_MAX_PREDICATES; i++) {
		dst->predicates[i].field = src->predicates[i].field;
		dst->predicates[i].op = src->predicates[i].op;
		dst->predicates[i].value = src->predicates[i].value;
	}
	for (i = 0; i < API_TRACE_MAX_ACTIONS; i++) {
		dst->actions[i].kind = src->actions[i].kind;
		dst->actions[i].arg = src->actions[i].arg;
		dst->actions[i].key = src->actions[i].key;
		dst->actions[i].id = src->actions[i].id;
		dst->actions[i].value = src->actions[i].value;
	}
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
	out->action_hits = stats.action_hits;
	out->aggregation_updates = stats.aggregation_updates;
	out->provider_count = stats.provider_count;
	out->probe_count = stats.probe_count;
	out->session_count = stats.session_count;
	out->ring_records = stats.ring_records;
	out->enabled = stats.enabled;
	out->initialized = stats.initialized;
	for (i = 0; i < API_TRACE_MAX_PROBES; i++) {
		out->probe_hits[i] = stats.probe_hits[i];
	}
}

static void
api_trace_fill_provider(struct api_trace_provider *out,
    const trace_provider_info_t *info)
{
	memset(out, 0, sizeof(*out));
	out->id = info->id;
	out->enabled = info->enabled;
	out->probe_count = info->probe_count;
	api_trace_copy_name(out->name, API_TRACE_NAME_LEN, info->name);
}

static void
api_trace_fill_probe(struct api_trace_probe *out,
    const trace_probe_info_t *info)
{
	u32	i, argc;

	memset(out, 0, sizeof(*out));
	out->id = info->id;
	out->provider = info->provider;
	out->enabled = info->enabled;
	out->argc = info->argc;
	out->flags = info->flags;
	api_trace_copy_name(out->provider_name, API_TRACE_NAME_LEN,
	    info->provider_name);
	api_trace_copy_name(out->module, API_TRACE_NAME_LEN, info->module);
	api_trace_copy_name(out->function, API_TRACE_NAME_LEN, info->function);
	api_trace_copy_name(out->name, API_TRACE_NAME_LEN, info->name);
	argc = info->argc;
	if (argc > API_TRACE_MAX_ARGS) {
		argc = API_TRACE_MAX_ARGS;
	}
	for (i = 0; i < argc; i++) {
		api_trace_copy_name(out->args[i].name, API_TRACE_NAME_LEN,
		    info->args[i].name);
		out->args[i].type = info->args[i].type;
		out->args[i].flags = info->args[i].flags;
	}
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
api_trace_info_providers(struct api_trace_providers *uarg)
{
	struct api_trace_providers	local;
	trace_provider_info_t		info;
	size_t				bytes;
	u32				i, max, total;

	if (!api_trace_user_range(uarg, sizeof(*uarg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	local = *uarg;
	total = trace_provider_count();
	max = local.max_providers;
	if (max > total) {
		max = total;
	}
	if (max > API_TRACE_MAX_PROVIDERS) {
		max = API_TRACE_MAX_PROVIDERS;
	}
	if (max > 0) {
		bytes = (size_t)max * sizeof(struct api_trace_provider);
		if (!api_trace_user_range(local.providers, bytes, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
	}
	for (i = 0; i < max; i++) {
		if (trace_provider_info(i, &info) != 0) {
			return (-API_ERR_BAD_VALUE);
		}
		api_trace_fill_provider(&local.providers[i], &info);
	}
	uarg->count = total;
	return (0);
}

static int
api_trace_info_probes(struct api_trace_probes *uarg)
{
	struct api_trace_probes	local;
	trace_probe_info_t	info;
	size_t			bytes;
	u32			i, max, total;

	if (!api_trace_user_range(uarg, sizeof(*uarg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	local = *uarg;
	total = trace_probe_count();
	max = local.max_probes;
	if (max > total) {
		max = total;
	}
	if (max > API_TRACE_MAX_PROBES) {
		max = API_TRACE_MAX_PROBES;
	}
	if (max > 0) {
		bytes = (size_t)max * sizeof(struct api_trace_probe);
		if (!api_trace_user_range(local.probes, bytes, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
	}
	for (i = 0; i < max; i++) {
		if (trace_probe_info(i, &info) != 0) {
			return (-API_ERR_BAD_VALUE);
		}
		api_trace_fill_probe(&local.probes[i], &info);
	}
	uarg->count = total;
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
	uarg->events_enabled =
	    (trace_probe_enabled(TRACE_PROBE_PROFILE_TICK) ||
	    trace_probe_enabled(TRACE_PROBE_PMU_COUNTERS)) ? 1 : 0;
	return (0);
}

static int
api_trace_info_aggs(struct api_trace_aggs *uarg)
{
	api_trace_handle_t	*handle;
	struct api_trace_aggs	local;
	size_t			bytes;
	int			ret;

	if (!api_trace_user_range(uarg, sizeof(*uarg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	local = *uarg;
	handle = api_trace_get_handle(local.trace);
	if (handle == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (local.max_aggs > API_TRACE_MAX_AGGREGATIONS) {
		return (-API_ERR_TOO_BIG);
	}
	if (local.max_aggs > 0) {
		bytes = (size_t)local.max_aggs * sizeof(struct api_trace_agg);
		if (!api_trace_user_range(local.aggs, bytes, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
	}
	ret = 0;
	if (local.max_aggs > 0) {
		ret = trace_session_read_aggs(handle->session_id,
		    (trace_aggregation_t *)local.aggs, local.max_aggs,
		    local.clear != 0);
		if (ret < 0) {
			return (-API_ERR_BAD_HANDLE);
		}
	}
	uarg->count = (u32)ret;
	return (ret);
}

int
api_trace_open(u32 flags)
{
	api_trace_handle_t	*handle;
	process_t		*proc;
	int			trace, session, privileged;

	if ((flags & ~API_TRACE_OPEN_KNOWN) != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!trace_is_initialized()) {
		return (-API_ERR_NODEV);
	}
	proc = process_current();
	privileged = proc_has_privilege(proc);
	if ((flags & API_TRACE_OPEN_KNOWN) != 0 && !privileged) {
		return (-API_ERR_PERM);
	}
	trace = api_trace_alloc_handle(proc);
	if (trace < 0) {
		return (trace);
	}
	handle = api_trace_get_handle(trace);
	if (handle == NULL) {
		return (-API_ERR_NO_MEMORY);
	}
	session = trace_session_open(flags, proc ? proc->pid : 0, privileged);
	if (session < 0) {
		entity_handle_free(proc, trace);
		return (-API_ERR_BUSY);
	}
	handle->session_id = session;
	handle->flags = flags;
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
	entity_handle_free(process_current(), trace);
	return (0);
}

int
api_trace_read(int trace, struct api_trace_read *uarg)
{
	api_trace_handle_t	*handle;
	struct api_trace_read	local;
	trace_session_stats_t	stats;
	size_t			bytes;
	int			ret;

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
	uarg->records_total = stats.records_read;
	uarg->records_lost = stats.records_lost;
	return (ret);
}

int
api_trace_ctl(int trace, u32 op, void *arg)
{
	api_trace_handle_t	*handle;
	struct api_trace_load	load;
	struct api_trace_program	user_program;
	trace_program_spec_t	spec;
	u32			clear_flags;
	size_t			bytes;
	u32			i;
	int			ret;

	handle = api_trace_get_handle(trace);
	if (handle == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	switch (op) {
	case API_TRACE_OP_START:
		ret = trace_session_start(handle->session_id);
		break;
	case API_TRACE_OP_STOP:
		ret = trace_session_stop(handle->session_id);
		break;
	case API_TRACE_OP_CLEAR:
		clear_flags = 0;
		if (arg != NULL) {
			if (!api_trace_user_range(arg, sizeof(clear_flags), 0)) {
				return (-API_ERR_BAD_ADDR);
			}
			clear_flags = *(u32 *)arg;
		}
		ret = trace_session_clear(handle->session_id, clear_flags);
		break;
	case API_TRACE_OP_LOAD:
		if (!api_trace_user_range(arg, sizeof(load), 0)) {
			return (-API_ERR_BAD_ADDR);
		}
		load = *(struct api_trace_load *)arg;
		if (load.program_count > API_TRACE_MAX_PROGRAMS) {
			return (-API_ERR_TOO_BIG);
		}
		if (load.program_count == 0) {
			return (0);
		}
		bytes = (size_t)load.program_count *
		    sizeof(struct api_trace_program);
		if (!api_trace_user_range(load.programs, bytes, 0)) {
			return (-API_ERR_BAD_ADDR);
		}
		for (i = 0; i < load.program_count; i++) {
			user_program = load.programs[i];
			api_trace_program_from_user(&spec, &user_program);
			ret = trace_session_load(handle->session_id, &spec);
			if (ret != 0) {
				return (-API_ERR_BAD_VALUE);
			}
		}
		ret = 0;
		break;
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
	case API_TRACE_INFO_PROVIDERS:
		return (api_trace_info_providers(
		    (struct api_trace_providers *)arg));
	case API_TRACE_INFO_PROBES:
		return (api_trace_info_probes((struct api_trace_probes *)arg));
	case API_TRACE_INFO_PMU:
		return (api_trace_info_pmu((struct api_trace_pmu *)arg));
	case API_TRACE_INFO_AGGS:
		return (api_trace_info_aggs((struct api_trace_aggs *)arg));
	default:
		return (-API_ERR_BAD_VALUE);
	}
}

int
api_trace_mark(u32 id, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4)
{
	u64	args[TRACE_MAX_ARGS];

	memset(args, 0, sizeof(args));
	args[0] = id;
	args[1] = a0;
	args[2] = a1;
	args[3] = a2;
	args[4] = a3;
	args[5] = a4;
	trace_probe_fire(TRACE_PROBE_USER_MARK, TRACE_REC_F_USER, NULL, args);
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

void
api_trace_entity_release(entity_id_t entity)
{
	api_trace_handle_t	*handle;

	handle = (api_trace_handle_t *)entity_io_ptr(entity,
	    ENTITY_IO_PTR_BACKING);
	if (!handle || !handle->used) {
		return;
	}
	trace_session_close(handle->session_id);
	memset(handle, 0, sizeof(*handle));
}
