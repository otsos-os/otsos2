/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type trace_ring_t as per CPU per session trace buffer
$define %type trace_program_t as loaded probe program
$define %type trace_session_t as consumer-owned trace state
$define %type trace_context_t as one fired probe execution context
$define %type trace_probe_desc_t as static provider probe descriptor
$define %type trace_record_t as one trace record
$define %type registers_t as struct with CPU register snapshot
$define %type thread_t as struct with per-thread CPU context and state
$define %type process_t as struct with process control block

$define %func trace_copy_name as procedure with args char *, size, string
$define %func trace_round_ring_records as function with args int
$define %func trace_apply_config as procedure with args void
$define %func trace_ring_alloc as function with args ring
$define %func trace_ring_free as procedure with args ring
$define %func trace_ring_reset as procedure with args ring
$define %func trace_session_alloc_cpu as function with args session, cpu
$define %func trace_capture_stack as procedure with args record, regs
$define %func trace_probe_provider_enabled as function with args probe
$define %func trace_rebuild_enable_counts as procedure with args void
$define %func trace_context_fill as procedure with args context
$define %func trace_field_valid as function with args probe, field, allow none
$define %func trace_field_value as function with args context, field
$define %func trace_predicate_match as function with args context, predicate
$define %func trace_program_match as function with args context, program
$define %func trace_aggregation_find as function with args session, action, key
$define %func trace_aggregation_update as procedure with args session, context
$define %func trace_record_write as procedure with args session, context, action
$define %func trace_action_exec as procedure with args session, context, action
$define %func trace_init as procedure with args void
$define %func trace_cpu_online as procedure with args void
$define %func trace_is_initialized as function with args void
$define %func trace_is_enabled as function with args void
$define %func trace_clock_read as function with args void
$define %func trace_probe_enabled as function with args u32
$define %func trace_probe_fire as procedure with args probe, flags, regs, args
$define %func trace_sample_tick as procedure with args registers_t *
$define %func trace_irq_enter as procedure with args registers_t *
$define %func trace_irq_exit as procedure with args registers_t *
$define %func trace_exception as procedure with args registers_t *
$define %func trace_syscall_enter as function with args registers_t *
$define %func trace_syscall_exit as procedure with args regs, nr, ret, tsc
$define %func trace_sched_tick as procedure with args registers_t *
$define %func trace_sched_switch as procedure with args prev, next, reason, regs
$define %func trace_thread_runtime_cycles as function with args thread_t *
$define %func trace_kqueue_create as procedure with args int, u32
$define %func trace_kqueue_destroy as procedure with args int
$define %func trace_knote_ready as procedure with args s16, u64, s64
$define %func trace_kevent_wait as procedure with args int, int, int, s64
$define %func trace_kevent_return as procedure with args int, int, s64
$define %func trace_event_timer_tick as procedure with args void
$define %func trace_session_open as function with args flags, owner, priv
$define %func trace_session_close as function with args int
$define %func trace_session_start as function with args int
$define %func trace_session_stop as function with args int
$define %func trace_session_clear as function with args int, u32
$define %func trace_session_load as function with args int, program
$define %func trace_session_read as function with args session, records, count
$define %func trace_session_read_aggs as function with args session, aggs, count
$define %func trace_session_stats as function with args int, stats
$define %func trace_provider_count as function with args void
$define %func trace_probe_count as function with args void
$define %func trace_provider_info as function with args index, info
$define %func trace_probe_info as function with args index, info
$define %func trace_get_stats as procedure with args trace_stats_t *

*/

/* !SPACE!

$space %internal trace_copy_name, trace_round_ring_records
$space %internal trace_apply_config, trace_ring_alloc, trace_ring_free
$space %internal trace_ring_reset, trace_session_alloc_cpu
$space %internal trace_capture_stack, trace_probe_provider_enabled
$space %internal trace_rebuild_enable_counts, trace_context_fill
$space %internal trace_field_valid, trace_field_value, trace_predicate_match
$space %internal trace_program_match, trace_aggregation_find
$space %internal trace_aggregation_update, trace_record_write
$space %internal trace_action_exec
$space %export trace_init, trace_cpu_online
$space %export trace_is_initialized, trace_is_enabled, trace_clock_read
$space %export trace_probe_enabled, trace_probe_fire
$space %export trace_sample_tick, trace_irq_enter, trace_irq_exit
$space %export trace_exception, trace_syscall_enter, trace_syscall_exit
$space %export trace_sched_tick, trace_sched_switch
$space %export trace_thread_runtime_cycles
$space %export trace_kqueue_create, trace_kqueue_destroy
$space %export trace_knote_ready, trace_kevent_wait, trace_kevent_return
$space %export trace_event_timer_tick
$space %export trace_session_open, trace_session_close
$space %export trace_session_start, trace_session_stop, trace_session_clear
$space %export trace_session_load, trace_session_read
$space %export trace_session_read_aggs, trace_session_stats
$space %export trace_provider_count, trace_probe_count
$space %export trace_provider_info, trace_probe_info, trace_get_stats

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

#include <kernel/trace/trace.h>
#include <kernel/cm/cm.h>
#include <kernel/drivers/timer.h>
#include <kernel/process.h>
#include <kernel/smp/smp.h>
#include <kernel/thread.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	TRACE_IRQ_VECTORS	256
#define	TRACE_STACK_SCAN_LIMIT	0x100000ULL

typedef struct trace_ring {
	trace_record_t	*records;
	u64		head;
	u64		limit;
	u64		records_written;
	u64		records_lost;
	u32		record_count;
	u32		record_mask;
} trace_ring_t;

typedef struct trace_program {
	int			used;
	u32			id;
	trace_program_spec_t	spec;
} trace_program_t;

typedef struct trace_session {
	int			used;
	int			active;
	int			privileged;
	u32			flags;
	u32			owner_pid;
	u32			program_count;
	u64			records_written;
	u64			records_read;
	u64			records_lost;
	u64			cursor[TRACE_MAX_CPUS];
	trace_ring_t		rings[TRACE_MAX_CPUS];
	trace_program_t		programs[TRACE_MAX_PROGRAMS];
	trace_aggregation_t	aggregations[TRACE_MAX_AGGREGATIONS];
} trace_session_t;

typedef struct trace_context {
	const trace_probe_desc_t	*probe;
	const registers_t	*regs;
	u64			args[TRACE_MAX_ARGS];
	u64			pid;
	u64			tid;
	u64			ip;
	u64			sp;
	u64			bp;
	u64			tsc;
	u64			ticks;
	u32			cpu;
	u32			flags;
	u32			argc;
} trace_context_t;

static trace_session_t	g_trace_sessions[TRACE_MAX_SESSIONS];
static int		g_trace_cpu_online[TRACE_MAX_CPUS];
static u64		g_trace_irq_start[TRACE_MAX_CPUS][TRACE_IRQ_VECTORS];
static u64		g_trace_last_sample_tsc[TRACE_MAX_CPUS];
static u32		g_trace_sample_ticks[TRACE_MAX_CPUS];
static u32		g_trace_probe_enable_count[TRACE_MAX_PROBES];
static u64		g_trace_probe_hits[TRACE_MAX_PROBES];
static u64		g_trace_records_written;
static u64		g_trace_records_lost;
static u64		g_trace_action_hits;
static u64		g_trace_aggregation_updates;
static u32		g_trace_session_count;
static u32		g_trace_ring_records;
static u32		g_trace_sample_period;
static u32		g_trace_stack_depth;
static int		g_trace_initialized;
static int		g_trace_enabled;

static int	g_trace_provider_enabled[TRACE_PROVIDER_COUNT];

static const char *g_trace_provider_names[TRACE_PROVIDER_COUNT] = {
	"core",
	"profile",
	"pmu",
	"syscall",
	"irq",
	"sched",
	"event",
	"user"
};

static const char *g_trace_provider_registry_names[TRACE_PROVIDER_COUNT] = {
	"Core",
	"Profile",
	"Pmu",
	"Syscall",
	"Irq",
	"Sched",
	"Event",
	"User"
};

static const trace_arg_desc_t trace_args_boot[] = {
	{ "ring_records", TRACE_ARG_U64, 0 },
	{ "sample_ticks", TRACE_ARG_U64, 0 },
	{ "stack_depth", TRACE_ARG_U64, 0 }
};

static const trace_arg_desc_t trace_args_pmu[] = {
	{ "cycles", TRACE_ARG_CYCLES, 0 },
	{ "instructions", TRACE_ARG_U64, 0 },
	{ "cache_references", TRACE_ARG_U64, 0 },
	{ "cache_misses", TRACE_ARG_U64, 0 },
	{ "branch_instructions", TRACE_ARG_U64, 0 },
	{ "branch_misses", TRACE_ARG_U64, 0 }
};

static const trace_arg_desc_t trace_args_syscall_entry[] = {
	{ "number", TRACE_ARG_ID, 0 },
	{ "arg0", TRACE_ARG_U64, 0 },
	{ "arg1", TRACE_ARG_U64, 0 },
	{ "arg2", TRACE_ARG_U64, 0 },
	{ "arg3", TRACE_ARG_U64, 0 },
	{ "arg4", TRACE_ARG_U64, 0 },
	{ "arg5", TRACE_ARG_U64, 0 }
};

static const trace_arg_desc_t trace_args_syscall_return[] = {
	{ "number", TRACE_ARG_ID, 0 },
	{ "return", TRACE_ARG_S64, 0 },
	{ "cycles", TRACE_ARG_CYCLES, 0 }
};

static const trace_arg_desc_t trace_args_irq[] = {
	{ "vector", TRACE_ARG_ID, 0 },
	{ "err_code", TRACE_ARG_U64, 0 },
	{ "cs", TRACE_ARG_U64, 0 },
	{ "cycles", TRACE_ARG_CYCLES, 0 }
};

static const trace_arg_desc_t trace_args_exception[] = {
	{ "vector", TRACE_ARG_ID, 0 },
	{ "err_code", TRACE_ARG_U64, 0 },
	{ "cs", TRACE_ARG_U64, 0 },
	{ "fault_addr", TRACE_ARG_PTR, 0 }
};

static const trace_arg_desc_t trace_args_sched_switch[] = {
	{ "prev_pid", TRACE_ARG_PID, 0 },
	{ "prev_tid", TRACE_ARG_TID, 0 },
	{ "next_pid", TRACE_ARG_PID, 0 },
	{ "next_tid", TRACE_ARG_TID, 0 },
	{ "reason", TRACE_ARG_ID, 0 },
	{ "prev_cycles", TRACE_ARG_CYCLES, 0 }
};

static const trace_arg_desc_t trace_args_event[] = {
	{ "id", TRACE_ARG_ID, 0 },
	{ "a", TRACE_ARG_U64, 0 },
	{ "b", TRACE_ARG_U64, 0 },
	{ "c", TRACE_ARG_U64, 0 }
};

static const trace_arg_desc_t trace_args_user_mark[] = {
	{ "id", TRACE_ARG_ID, 0 },
	{ "a0", TRACE_ARG_U64, 0 },
	{ "a1", TRACE_ARG_U64, 0 },
	{ "a2", TRACE_ARG_U64, 0 },
	{ "a3", TRACE_ARG_U64, 0 },
	{ "a4", TRACE_ARG_U64, 0 }
};

static const trace_probe_desc_t g_trace_probes[TRACE_PROBE_COUNT] = {
	{
		TRACE_PROBE_CORE_BOOT, TRACE_PROVIDER_CORE, 0,
		"kernel", "", "boot", trace_args_boot, 3
	},
	{
		TRACE_PROBE_PROFILE_TICK, TRACE_PROVIDER_PROFILE, 0,
		"kernel", "", "tick", trace_args_pmu, 6
	},
	{
		TRACE_PROBE_PMU_COUNTERS, TRACE_PROVIDER_PMU, 0,
		"kernel", "", "counters", trace_args_pmu, 6
	},
	{
		TRACE_PROBE_SYSCALL_ENTRY, TRACE_PROVIDER_SYSCALL, 0,
		"kernel", "", "entry", trace_args_syscall_entry, 7
	},
	{
		TRACE_PROBE_SYSCALL_RETURN, TRACE_PROVIDER_SYSCALL, 0,
		"kernel", "", "return", trace_args_syscall_return, 3
	},
	{
		TRACE_PROBE_IRQ_ENTRY, TRACE_PROVIDER_IRQ, 0,
		"kernel", "", "entry", trace_args_irq, 4
	},
	{
		TRACE_PROBE_IRQ_RETURN, TRACE_PROVIDER_IRQ, 0,
		"kernel", "", "return", trace_args_irq, 4
	},
	{
		TRACE_PROBE_IRQ_EXCEPTION, TRACE_PROVIDER_IRQ, 0,
		"kernel", "", "exception", trace_args_exception, 4
	},
	{
		TRACE_PROBE_SCHED_TICK, TRACE_PROVIDER_SCHED, 0,
		"kernel", "", "tick", NULL, 0
	},
	{
		TRACE_PROBE_SCHED_SWITCH, TRACE_PROVIDER_SCHED, 0,
		"kernel", "", "switch", trace_args_sched_switch, 6
	},
	{
		TRACE_PROBE_EVENT_KQUEUE_CREATE, TRACE_PROVIDER_EVENT, 0,
		"kernel", "kqueue", "create", trace_args_event, 4
	},
	{
		TRACE_PROBE_EVENT_KQUEUE_DESTROY, TRACE_PROVIDER_EVENT, 0,
		"kernel", "kqueue", "destroy", trace_args_event, 4
	},
	{
		TRACE_PROBE_EVENT_KNOTE_READY, TRACE_PROVIDER_EVENT, 0,
		"kernel", "knote", "ready", trace_args_event, 4
	},
	{
		TRACE_PROBE_EVENT_KEVENT_WAIT, TRACE_PROVIDER_EVENT, 0,
		"kernel", "kevent", "wait", trace_args_event, 4
	},
	{
		TRACE_PROBE_EVENT_KEVENT_RETURN, TRACE_PROVIDER_EVENT, 0,
		"kernel", "kevent", "return", trace_args_event, 4
	},
	{
		TRACE_PROBE_EVENT_TIMER_TICK, TRACE_PROVIDER_EVENT, 0,
		"kernel", "timer", "tick", NULL, 0
	},
	{
		TRACE_PROBE_USER_MARK, TRACE_PROVIDER_USER, 0,
		"user", "", "mark", trace_args_user_mark, 6
	}
};

static void
trace_copy_name(char *dst, u32 size, const char *src)
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

static u32
trace_round_ring_records(int value)
{
	u32	records;

	if (value < TRACE_MIN_RING_RECORDS) {
		value = TRACE_MIN_RING_RECORDS;
	}
	if (value > TRACE_MAX_RING_RECORDS) {
		value = TRACE_MAX_RING_RECORDS;
	}
	records = 1;
	while (records < (u32)value) {
		records <<= 1;
	}
	if (records > TRACE_MAX_RING_RECORDS) {
		records = TRACE_MAX_RING_RECORDS;
	}
	return (records);
}

static void
trace_apply_config(void)
{
	const char	*name;
	u32		provider;
	int		enabled;

	g_trace_enabled = cm_get_bool_default("SYSTEM", "Trace",
	    "Enabled", 1);
	g_trace_ring_records = trace_round_ring_records(
	    (int)cm_get_u32_default("SYSTEM", "Trace", "RingRecords",
	    TRACE_DEFAULT_RING_RECORDS));
	g_trace_sample_period = cm_get_u32_default("SYSTEM", "Trace",
	    "SampleEveryTicks", 10);
	if (g_trace_sample_period == 0) {
		g_trace_sample_period = 1;
	}
	g_trace_stack_depth = cm_get_u32_default("SYSTEM", "Trace",
	    "StackDepth", 4);
	if (g_trace_stack_depth > TRACE_RECORD_STACK) {
		g_trace_stack_depth = TRACE_RECORD_STACK;
	}
	for (provider = 0; provider < TRACE_PROVIDER_COUNT; provider++) {
		name = g_trace_provider_registry_names[provider];
		enabled = cm_get_bool_default("SYSTEM", "Trace.Providers",
		    name, 1);
		g_trace_provider_enabled[provider] = enabled ? 1 : 0;
	}
}

static int
trace_ring_alloc(trace_ring_t *ring)
{
	if (ring == NULL) {
		return (-1);
	}
	if (ring->records != NULL) {
		return (0);
	}
	ring->records = kmem_calloc(g_trace_ring_records,
	    sizeof(trace_record_t));
	if (ring->records == NULL) {
		return (-1);
	}
	ring->head = 0;
	ring->limit = 0;
	ring->records_written = 0;
	ring->records_lost = 0;
	ring->record_count = g_trace_ring_records;
	ring->record_mask = g_trace_ring_records - 1;
	return (0);
}

static void
trace_ring_free(trace_ring_t *ring)
{
	if (ring == NULL) {
		return;
	}
	if (ring->records != NULL) {
		kmem_free(ring->records);
	}
	memset(ring, 0, sizeof(*ring));
}

static void
trace_ring_reset(trace_ring_t *ring)
{
	if (ring == NULL || ring->records == NULL) {
		return;
	}
	memset(ring->records, 0, ring->record_count * sizeof(trace_record_t));
	ring->head = 0;
	ring->limit = 0;
	ring->records_written = 0;
	ring->records_lost = 0;
}

static int
trace_session_alloc_cpu(trace_session_t *session, int cpu)
{
	trace_ring_t	*ring;

	if (session == NULL || cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return (-1);
	}
	ring = &session->rings[cpu];
	if (trace_ring_alloc(ring) != 0) {
		session->records_lost++;
		__atomic_fetch_add(&g_trace_records_lost, 1,
		    __ATOMIC_RELAXED);
		return (-1);
	}
	return (0);
}

static void
trace_capture_stack(trace_record_t *rec, const registers_t *regs)
{
	u64	*frame;
	u64	bp, next, ret;
	u32	limit;
	u32	count;

	memset(rec->stack, 0, sizeof(rec->stack));
	rec->stack_count = 0;
	if (regs == NULL || g_trace_stack_depth == 0) {
		return;
	}
	limit = g_trace_stack_depth;
	if (limit > TRACE_RECORD_STACK) {
		limit = TRACE_RECORD_STACK;
	}
	count = 0;
	rec->stack[count++] = regs->rip;
	if ((regs->cs & 3) != 0 || count >= limit) {
		rec->stack_count = count;
		return;
	}
	bp = regs->rbp;
	while (count < limit) {
		if (bp < KERNEL_VMA) {
			break;
		}
		if (bp < regs->rsp || bp - regs->rsp > KERNEL_STACK_SIZE) {
			break;
		}
		frame = (u64 *)bp;
		next = frame[0];
		ret = frame[1];
		if (ret == 0) {
			break;
		}
		rec->stack[count++] = ret;
		if (next <= bp || next - bp > TRACE_STACK_SCAN_LIMIT) {
			break;
		}
		bp = next;
	}
	rec->stack_count = count;
}

static int
trace_probe_provider_enabled(u32 probe_id)
{
	const trace_probe_desc_t	*probe;

	if (probe_id >= TRACE_PROBE_COUNT) {
		return (0);
	}
	probe = &g_trace_probes[probe_id];
	if (probe->provider >= TRACE_PROVIDER_COUNT) {
		return (0);
	}
	return (g_trace_provider_enabled[probe->provider] != 0);
}

static void
trace_rebuild_enable_counts(void)
{
	trace_session_t		*session;
	trace_program_t		*program;
	u32			i, p;

	memset(g_trace_probe_enable_count, 0,
	    sizeof(g_trace_probe_enable_count));
	for (i = 0; i < TRACE_MAX_SESSIONS; i++) {
		session = &g_trace_sessions[i];
		if (!session->used || !session->active) {
			continue;
		}
		for (p = 0; p < TRACE_MAX_PROGRAMS; p++) {
			program = &session->programs[p];
			if (!program->used) {
				continue;
			}
			if (program->spec.probe_id >= TRACE_PROBE_COUNT) {
				continue;
			}
			g_trace_probe_enable_count[program->spec.probe_id]++;
		}
	}
}

static void
trace_context_fill(trace_context_t *ctx, u32 probe_id, u32 flags,
    const registers_t *regs, const u64 *args)
{
	thread_t	*td;
	process_t	*proc;
	u32		i;

	memset(ctx, 0, sizeof(*ctx));
	ctx->probe = &g_trace_probes[probe_id];
	ctx->regs = regs;
	ctx->cpu = (u32)smp_cpu_index();
	ctx->flags = flags;
	ctx->tsc = trace_clock_read();
	if (timer_is_initialized()) {
		ctx->ticks = timer_get_ticks();
	}
	td = thread_current();
	proc = NULL;
	if (td != NULL) {
		proc = td->proc;
		ctx->tid = td->tid;
	}
	if (proc != NULL) {
		ctx->pid = proc->pid;
	}
	if (regs != NULL) {
		ctx->ip = regs->rip;
		ctx->sp = regs->rsp;
		ctx->bp = regs->rbp;
		if ((regs->cs & 3) != 0) {
			ctx->flags |= TRACE_REC_F_USER;
		}
	}
	ctx->argc = ctx->probe->argc;
	if (ctx->argc > TRACE_MAX_ARGS) {
		ctx->argc = TRACE_MAX_ARGS;
	}
	for (i = 0; i < ctx->argc; i++) {
		if (args != NULL) {
			ctx->args[i] = args[i];
		}
	}
}

static u64
trace_field_value(const trace_context_t *ctx, u32 field)
{
	u32	index;

	switch (field) {
	case TRACE_FIELD_PID:
		return (ctx->pid);
	case TRACE_FIELD_TID:
		return (ctx->tid);
	case TRACE_FIELD_CPU:
		return (ctx->cpu);
	case TRACE_FIELD_PROBE:
		return (ctx->probe->id);
	default:
		break;
	}
	if (field >= TRACE_FIELD_ARG0 &&
	    field <= TRACE_FIELD_ARG7) {
		index = field - TRACE_FIELD_ARG0;
		if (index < ctx->argc && index < TRACE_MAX_ARGS) {
			return (ctx->args[index]);
		}
	}
	return (0);
}

static int
trace_field_valid(const trace_probe_desc_t *probe, u32 field, int allow_none)
{
	u32	index;

	if (field == TRACE_FIELD_NONE) {
		return (allow_none != 0);
	}
	switch (field) {
	case TRACE_FIELD_PID:
	case TRACE_FIELD_TID:
	case TRACE_FIELD_CPU:
	case TRACE_FIELD_PROBE:
		return (1);
	default:
		break;
	}
	if (field >= TRACE_FIELD_ARG0 && field <= TRACE_FIELD_ARG7) {
		index = field - TRACE_FIELD_ARG0;
		if (index < TRACE_MAX_ARGS && index < probe->argc) {
			return (1);
		}
	}
	return (0);
}

static int
trace_predicate_match(const trace_context_t *ctx,
    const trace_predicate_t *pred)
{
	u64	left, right;

	left = trace_field_value(ctx, pred->field);
	right = pred->value;
	switch (pred->op) {
	case TRACE_PRED_EQ:
		return (left == right);
	case TRACE_PRED_NE:
		return (left != right);
	case TRACE_PRED_LT:
		return (left < right);
	case TRACE_PRED_LE:
		return (left <= right);
	case TRACE_PRED_GT:
		return (left > right);
	case TRACE_PRED_GE:
		return (left >= right);
	case TRACE_PRED_MASK:
		return ((left & right) != 0);
	default:
		return (0);
	}
}

static int
trace_program_match(const trace_context_t *ctx,
    const trace_program_t *program)
{
	const trace_predicate_t	*pred;
	u32			i;

	if (!program->used || program->spec.probe_id != ctx->probe->id) {
		return (0);
	}
	for (i = 0; i < program->spec.predicate_count; i++) {
		pred = &program->spec.predicates[i];
		if (!trace_predicate_match(ctx, pred)) {
			return (0);
		}
	}
	return (1);
}

static trace_aggregation_t *
trace_aggregation_find(trace_session_t *session,
    const trace_context_t *ctx, const trace_action_t *action, const u64 *key)
{
	trace_aggregation_t	*agg;
	u32			empty, i, k, match;

	empty = TRACE_MAX_AGGREGATIONS;
	for (i = 0; i < TRACE_MAX_AGGREGATIONS; i++) {
		agg = &session->aggregations[i];
		if (agg->kind == 0) {
			if (empty == TRACE_MAX_AGGREGATIONS) {
				empty = i;
			}
			continue;
		}
		if (agg->id == action->id && agg->kind == action->kind &&
		    agg->probe_id == ctx->probe->id && agg->arg == action->arg &&
		    agg->key[0] == key[0]) {
			match = 1;
			for (k = 1; k < 4; k++) {
				if (agg->key[k] != key[k]) {
					match = 0;
					break;
				}
			}
			if (match) {
				return (agg);
			}
		}
	}
	if (empty == TRACE_MAX_AGGREGATIONS) {
		session->records_lost++;
		__atomic_fetch_add(&g_trace_records_lost, 1,
		    __ATOMIC_RELAXED);
		return (NULL);
	}
	agg = &session->aggregations[empty];
	memset(agg, 0, sizeof(*agg));
	agg->id = action->id;
	agg->kind = action->kind;
	agg->probe_id = ctx->probe->id;
	agg->arg = action->arg;
	for (k = 0; k < 4; k++) {
		agg->key[k] = key[k];
	}
	return (agg);
}

static void
trace_aggregation_update(trace_session_t *session,
    const trace_context_t *ctx, const trace_action_t *action)
{
	trace_aggregation_t	*agg;
	u64			key[4];
	u64			value, group, bucket, width, tmp;

	value = 0;
	group = 0;
	bucket = 0;
	if (action->arg < TRACE_MAX_ARGS) {
		value = ctx->args[action->arg];
	}
	if (action->key != TRACE_FIELD_NONE) {
		group = trace_field_value(ctx, action->key);
	}
	memset(key, 0, sizeof(key));
	key[0] = group;
	if (action->kind == TRACE_ACT_QUANTIZE) {
		tmp = value;
		while (tmp > 1) {
			tmp >>= 1;
			bucket++;
		}
		key[1] = bucket;
	} else if (action->kind == TRACE_ACT_LQUANTIZE) {
		width = action->value;
		if (width == 0) {
			width = 1;
		}
		key[1] = value / width;
	}
	agg = trace_aggregation_find(session, ctx, action, key);
	if (agg == NULL) {
		return;
	}
	switch (action->kind) {
	case TRACE_ACT_COUNT:
		agg->value++;
		break;
	case TRACE_ACT_SUM:
		agg->value += value;
		break;
	case TRACE_ACT_MIN:
		if (agg->count == 0 || value < agg->value) {
			agg->value = value;
		}
		break;
	case TRACE_ACT_MAX:
		if (agg->count == 0 || value > agg->value) {
			agg->value = value;
		}
		break;
	case TRACE_ACT_QUANTIZE:
	case TRACE_ACT_LQUANTIZE:
		agg->value++;
		break;
	default:
		return;
	}
	agg->count++;
	__atomic_fetch_add(&g_trace_aggregation_updates, 1,
	    __ATOMIC_RELAXED);
}

static void
trace_record_write(trace_session_t *session, const trace_context_t *ctx,
    const trace_action_t *action)
{
	trace_record_t	rec;
	trace_ring_t	*ring;
	u64		seq;
	u32		index, i;
	int		cpu;

	cpu = (int)ctx->cpu;
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		session->records_lost++;
		__atomic_fetch_add(&g_trace_records_lost, 1,
		    __ATOMIC_RELAXED);
		return;
	}
	ring = &session->rings[cpu];
	if (ring->records == NULL || ring->record_count == 0) {
		session->records_lost++;
		__atomic_fetch_add(&g_trace_records_lost, 1,
		    __ATOMIC_RELAXED);
		return;
	}
	memset(&rec, 0, sizeof(rec));
	seq = ring->head;
	index = (u32)(seq & ring->record_mask);
	rec.seq = seq;
	rec.tsc = ctx->tsc;
	rec.ticks = ctx->ticks;
	rec.pid = ctx->pid;
	rec.tid = ctx->tid;
	rec.ip = ctx->ip;
	rec.sp = ctx->sp;
	rec.bp = ctx->bp;
	rec.probe_id = ctx->probe->id;
	rec.action_id = action->id;
	rec.cpu = ctx->cpu;
	rec.flags = ctx->flags;
	rec.argc = ctx->argc;
	for (i = 0; i < ctx->argc && i < TRACE_MAX_ARGS; i++) {
		rec.args[i] = ctx->args[i];
	}
	if (action->kind == TRACE_ACT_STACK) {
		trace_capture_stack(&rec, ctx->regs);
		if (rec.stack_count > 0) {
			rec.flags |= TRACE_REC_F_KERNEL_STACK;
		}
	}
	if ((session->flags & TRACE_OPEN_KERNEL_STACK) == 0) {
		rec.stack_count = 0;
		memset(rec.stack, 0, sizeof(rec.stack));
		if ((rec.flags & TRACE_REC_F_USER) == 0) {
			rec.ip = 0;
			rec.sp = 0;
			rec.bp = 0;
		}
	}
	ring->records[index] = rec;
	ring->head = seq + 1;
	ring->records_written++;
	session->records_written++;
	__atomic_fetch_add(&g_trace_records_written, 1, __ATOMIC_RELAXED);
}

static void
trace_action_exec(trace_session_t *session, const trace_context_t *ctx,
    const trace_action_t *action)
{
	switch (action->kind) {
	case TRACE_ACT_RECORD:
	case TRACE_ACT_STACK:
		trace_record_write(session, ctx, action);
		break;
	case TRACE_ACT_COUNT:
	case TRACE_ACT_SUM:
	case TRACE_ACT_MIN:
	case TRACE_ACT_MAX:
	case TRACE_ACT_QUANTIZE:
	case TRACE_ACT_LQUANTIZE:
		trace_aggregation_update(session, ctx, action);
		break;
	default:
		break;
	}
	__atomic_fetch_add(&g_trace_action_hits, 1, __ATOMIC_RELAXED);
}

void
trace_init(void)
{
	u64	args[TRACE_MAX_ARGS];

	memset(g_trace_sessions, 0, sizeof(g_trace_sessions));
	memset(g_trace_cpu_online, 0, sizeof(g_trace_cpu_online));
	memset(g_trace_irq_start, 0, sizeof(g_trace_irq_start));
	memset(g_trace_last_sample_tsc, 0, sizeof(g_trace_last_sample_tsc));
	memset(g_trace_sample_ticks, 0, sizeof(g_trace_sample_ticks));
	memset(g_trace_probe_enable_count, 0, sizeof(g_trace_probe_enable_count));
	memset(g_trace_probe_hits, 0, sizeof(g_trace_probe_hits));
	g_trace_records_written = 0;
	g_trace_records_lost = 0;
	g_trace_action_hits = 0;
	g_trace_aggregation_updates = 0;
	g_trace_session_count = 0;

	trace_apply_config();
	g_trace_initialized = 1;
	if (!g_trace_enabled) {
		printk("[TRACE] disabled by registry\n");
		return;
	}
	trace_pmu_init();
	trace_cpu_online();
	memset(args, 0, sizeof(args));
	args[0] = g_trace_ring_records;
	args[1] = g_trace_sample_period;
	args[2] = g_trace_stack_depth;
	trace_probe_fire(TRACE_PROBE_CORE_BOOT, 0, NULL, args);
	printk("[TRACE] dtrace core enabled probes=%u ring=%u sample_ticks=%u\n",
	    TRACE_PROBE_COUNT, g_trace_ring_records, g_trace_sample_period);
}

void
trace_cpu_online(void)
{
	trace_session_t	*session;
	int		cpu, i;

	if (!g_trace_initialized || !g_trace_enabled) {
		return;
	}
	cpu = smp_cpu_index();
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return;
	}
	g_trace_cpu_online[cpu] = 1;
	g_trace_last_sample_tsc[cpu] = trace_clock_read();
	trace_pmu_cpu_online(cpu);
	for (i = 0; i < TRACE_MAX_SESSIONS; i++) {
		session = &g_trace_sessions[i];
		if (session->used) {
			trace_session_alloc_cpu(session, cpu);
		}
	}
}

int
trace_is_initialized(void)
{
	return (g_trace_initialized);
}

int
trace_is_enabled(void)
{
	return (g_trace_initialized && g_trace_enabled);
}

u64
trace_clock_read(void)
{
	u32	lo, hi;

	__asm__ volatile("lfence; rdtsc" : "=a"(lo), "=d"(hi) : : "memory");
	return (((u64)hi << 32) | lo);
}

int
trace_probe_enabled(u32 probe_id)
{
	if (!trace_is_enabled() || probe_id >= TRACE_PROBE_COUNT) {
		return (0);
	}
	if (!trace_probe_provider_enabled(probe_id)) {
		return (0);
	}
	return (g_trace_probe_enable_count[probe_id] != 0);
}

void
trace_probe_fire(u32 probe_id, u32 flags, const registers_t *regs,
    const u64 *args)
{
	trace_context_t	ctx;
	trace_session_t	*session;
	trace_program_t	*program;
	trace_action_t	*action;
	u32		s, p, a;

	if (!trace_probe_enabled(probe_id)) {
		return;
	}
	trace_context_fill(&ctx, probe_id, flags, regs, args);
	__atomic_fetch_add(&g_trace_probe_hits[probe_id], 1,
	    __ATOMIC_RELAXED);
	for (s = 0; s < TRACE_MAX_SESSIONS; s++) {
		session = &g_trace_sessions[s];
		if (!session->used || !session->active) {
			continue;
		}
		if (!session->privileged &&
		    ctx.pid != (u64)session->owner_pid) {
			continue;
		}
		for (p = 0; p < TRACE_MAX_PROGRAMS; p++) {
			program = &session->programs[p];
			if (!trace_program_match(&ctx, program)) {
				continue;
			}
			for (a = 0; a < program->spec.action_count; a++) {
				action = &program->spec.actions[a];
				trace_action_exec(session, &ctx, action);
			}
		}
	}
}

void
trace_sample_tick(registers_t *regs)
{
	u64		values[TRACE_PMU_COUNTER_COUNT];
	u64		now, delta;
	int		cpu;

	if (!trace_probe_enabled(TRACE_PROBE_PROFILE_TICK) &&
	    !trace_probe_enabled(TRACE_PROBE_PMU_COUNTERS)) {
		return;
	}
	cpu = smp_cpu_index();
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return;
	}
	g_trace_sample_ticks[cpu]++;
	if (g_trace_sample_ticks[cpu] < g_trace_sample_period) {
		return;
	}
	g_trace_sample_ticks[cpu] = 0;
	memset(values, 0, sizeof(values));
	trace_pmu_sample(cpu, values, TRACE_PMU_COUNTER_COUNT);
	if (values[TRACE_PMU_CYCLES] == 0) {
		now = trace_clock_read();
		delta = now - g_trace_last_sample_tsc[cpu];
		g_trace_last_sample_tsc[cpu] = now;
		values[TRACE_PMU_CYCLES] = delta;
	}
	trace_probe_fire(TRACE_PROBE_PROFILE_TICK, TRACE_REC_F_PMU_VALID,
	    regs, values);
	trace_probe_fire(TRACE_PROBE_PMU_COUNTERS, TRACE_REC_F_PMU_VALID,
	    regs, values);
}

void
trace_irq_enter(registers_t *regs)
{
	u64	args[TRACE_MAX_ARGS];
	u64	now;
	int	cpu;

	if (regs == NULL) {
		return;
	}
	cpu = smp_cpu_index();
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return;
	}
	now = trace_clock_read();
	if (regs->int_no < TRACE_IRQ_VECTORS) {
		g_trace_irq_start[cpu][regs->int_no] = now;
	}
	memset(args, 0, sizeof(args));
	args[0] = regs->int_no;
	args[1] = regs->err_code;
	args[2] = regs->cs;
	trace_probe_fire(TRACE_PROBE_IRQ_ENTRY, 0, regs, args);
}

void
trace_irq_exit(registers_t *regs)
{
	u64	args[TRACE_MAX_ARGS];
	u64	now, start, cycles;
	int	cpu;

	if (regs == NULL) {
		return;
	}
	cpu = smp_cpu_index();
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return;
	}
	now = trace_clock_read();
	start = 0;
	if (regs->int_no < TRACE_IRQ_VECTORS) {
		start = g_trace_irq_start[cpu][regs->int_no];
	}
	cycles = 0;
	if (start != 0) {
		cycles = now - start;
	}
	memset(args, 0, sizeof(args));
	args[0] = regs->int_no;
	args[1] = regs->err_code;
	args[2] = regs->cs;
	args[3] = cycles;
	trace_probe_fire(TRACE_PROBE_IRQ_RETURN, 0, regs, args);
}

void
trace_exception(registers_t *regs)
{
	u64	args[TRACE_MAX_ARGS];
	u64	cr2;

	if (regs == NULL) {
		return;
	}
	cr2 = 0;
	if (regs->int_no == 14) {
		__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
	}
	memset(args, 0, sizeof(args));
	args[0] = regs->int_no;
	args[1] = regs->err_code;
	args[2] = regs->cs;
	args[3] = cr2;
	trace_probe_fire(TRACE_PROBE_IRQ_EXCEPTION, 0, regs, args);
}

u64
trace_syscall_enter(registers_t *regs)
{
	u64	args[TRACE_MAX_ARGS];
	u64	start;

	start = trace_clock_read();
	if (regs == NULL) {
		return (start);
	}
	memset(args, 0, sizeof(args));
	args[0] = regs->rax;
	args[1] = regs->rdi;
	args[2] = regs->rsi;
	args[3] = regs->rdx;
	args[4] = regs->r10;
	args[5] = regs->r8;
	args[6] = regs->r9;
	trace_probe_fire(TRACE_PROBE_SYSCALL_ENTRY, 0, regs, args);
	return (start);
}

void
trace_syscall_exit(registers_t *regs, u64 number, u64 ret, u64 start_tsc)
{
	u64	args[TRACE_MAX_ARGS];
	u64	cycles;

	cycles = 0;
	if (start_tsc != 0) {
		cycles = trace_clock_read() - start_tsc;
	}
	memset(args, 0, sizeof(args));
	args[0] = number;
	args[1] = ret;
	args[2] = cycles;
	trace_probe_fire(TRACE_PROBE_SYSCALL_RETURN, 0, regs, args);
}

void
trace_sched_tick(registers_t *regs)
{
	trace_probe_fire(TRACE_PROBE_SCHED_TICK, 0, regs, NULL);
}

void
trace_sched_switch(struct thread *prev, struct thread *next, u32 reason,
    registers_t *regs)
{
	thread_t	*prev_td;
	thread_t	*next_td;
	u64		args[TRACE_MAX_ARGS];
	u64		now, prev_delta;
	u64		prev_pid, prev_tid, next_pid, next_tid;

	prev_td = prev;
	next_td = next;
	now = trace_clock_read();
	prev_delta = 0;
	prev_pid = 0;
	prev_tid = 0;
	next_pid = 0;
	next_tid = 0;
	if (prev_td != NULL) {
		prev_tid = prev_td->tid;
		if (prev_td->proc != NULL) {
			prev_pid = prev_td->proc->pid;
		}
		if (prev_td->trace_last_tsc != 0) {
			prev_delta = now - prev_td->trace_last_tsc;
			prev_td->trace_runtime_cycles += prev_delta;
		}
	}
	if (next_td != NULL) {
		next_tid = next_td->tid;
		if (next_td->proc != NULL) {
			next_pid = next_td->proc->pid;
		}
		next_td->trace_last_tsc = now;
		next_td->trace_switches++;
	}
	memset(args, 0, sizeof(args));
	args[0] = prev_pid;
	args[1] = prev_tid;
	args[2] = next_pid;
	args[3] = next_tid;
	args[4] = reason;
	args[5] = prev_delta;
	trace_probe_fire(TRACE_PROBE_SCHED_SWITCH, 0, regs, args);
}

u64
trace_thread_runtime_cycles(struct thread *td)
{
	u64	now, total;

	if (td == NULL) {
		return (0);
	}
	total = td->trace_runtime_cycles;
	if (td->state == PROC_STATE_RUNNING && td->trace_last_tsc != 0) {
		now = trace_clock_read();
		total += now - td->trace_last_tsc;
	}
	return (total);
}

void
trace_kqueue_create(int kq_idx, u32 pid)
{
	u64	args[TRACE_MAX_ARGS];

	memset(args, 0, sizeof(args));
	args[0] = (u64)kq_idx;
	args[1] = pid;
	trace_probe_fire(TRACE_PROBE_EVENT_KQUEUE_CREATE, 0, NULL, args);
}

void
trace_kqueue_destroy(int kq_idx)
{
	u64	args[TRACE_MAX_ARGS];

	memset(args, 0, sizeof(args));
	args[0] = (u64)kq_idx;
	trace_probe_fire(TRACE_PROBE_EVENT_KQUEUE_DESTROY, 0, NULL, args);
}

void
trace_knote_ready(s16 filter, u64 ident, s64 data)
{
	u64	args[TRACE_MAX_ARGS];

	memset(args, 0, sizeof(args));
	args[0] = (u64)filter;
	args[1] = ident;
	args[2] = (u64)data;
	trace_probe_fire(TRACE_PROBE_EVENT_KNOTE_READY, 0, NULL, args);
}

void
trace_kevent_wait(int kq_idx, int nchanges, int nevents, s64 timeout_ms)
{
	u64	args[TRACE_MAX_ARGS];

	memset(args, 0, sizeof(args));
	args[0] = (u64)kq_idx;
	args[1] = (u64)nchanges;
	args[2] = (u64)nevents;
	args[3] = (u64)timeout_ms;
	trace_probe_fire(TRACE_PROBE_EVENT_KEVENT_WAIT, 0, NULL, args);
}

void
trace_kevent_return(int kq_idx, int count, s64 timeout_ms)
{
	u64	args[TRACE_MAX_ARGS];

	memset(args, 0, sizeof(args));
	args[0] = (u64)kq_idx;
	args[1] = (u64)count;
	args[2] = (u64)timeout_ms;
	trace_probe_fire(TRACE_PROBE_EVENT_KEVENT_RETURN, 0, NULL, args);
}

void
trace_event_timer_tick(void)
{
	trace_probe_fire(TRACE_PROBE_EVENT_TIMER_TICK, 0, NULL, NULL);
}

int
trace_session_open(u32 flags, u32 owner_pid, int privileged)
{
	trace_session_t	*session;
	int		i, cpu;

	if (!g_trace_initialized) {
		return (-1);
	}
	for (i = 0; i < TRACE_MAX_SESSIONS; i++) {
		session = &g_trace_sessions[i];
		if (session->used) {
			continue;
		}
		memset(session, 0, sizeof(*session));
		session->used = 1;
		session->active = 0;
		session->flags = flags;
		session->owner_pid = owner_pid;
		session->privileged = privileged ? 1 : 0;
		for (cpu = 0; cpu < TRACE_MAX_CPUS; cpu++) {
			if (g_trace_cpu_online[cpu]) {
				trace_session_alloc_cpu(session, cpu);
			}
			session->cursor[cpu] = 0;
		}
		g_trace_session_count++;
		return (i);
	}
	return (-1);
}

int
trace_session_close(int session_id)
{
	trace_session_t	*session;
	int		cpu;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	for (cpu = 0; cpu < TRACE_MAX_CPUS; cpu++) {
		trace_ring_free(&session->rings[cpu]);
	}
	memset(session, 0, sizeof(*session));
	if (g_trace_session_count > 0) {
		g_trace_session_count--;
	}
	trace_rebuild_enable_counts();
	return (0);
}

int
trace_session_start(int session_id)
{
	trace_session_t	*session;
	int		cpu;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	if (!session->active) {
		for (cpu = 0; cpu < TRACE_MAX_CPUS; cpu++) {
			session->cursor[cpu] = session->rings[cpu].head;
			session->rings[cpu].limit = 0;
		}
	}
	session->active = 1;
	trace_rebuild_enable_counts();
	return (0);
}

int
trace_session_stop(int session_id)
{
	trace_session_t	*session;
	int		cpu;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	if (session->active) {
		for (cpu = 0; cpu < TRACE_MAX_CPUS; cpu++) {
			session->rings[cpu].limit = session->rings[cpu].head;
		}
	}
	session->active = 0;
	trace_rebuild_enable_counts();
	return (0);
}

int
trace_session_clear(int session_id, u32 flags)
{
	trace_session_t	*session;
	int		cpu;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	if (flags == 0) {
		flags = TRACE_CLEAR_ALL;
	}
	if ((flags & TRACE_CLEAR_RECORDS) != 0) {
		for (cpu = 0; cpu < TRACE_MAX_CPUS; cpu++) {
			trace_ring_reset(&session->rings[cpu]);
			session->cursor[cpu] = 0;
		}
		session->records_written = 0;
		session->records_read = 0;
		session->records_lost = 0;
	}
	if ((flags & TRACE_CLEAR_PROGRAMS) != 0) {
		memset(session->programs, 0, sizeof(session->programs));
		session->program_count = 0;
	}
	if ((flags & TRACE_CLEAR_AGGS) != 0) {
		memset(session->aggregations, 0, sizeof(session->aggregations));
	}
	trace_rebuild_enable_counts();
	return (0);
}

int
trace_session_load(int session_id, const trace_program_spec_t *program)
{
	trace_session_t		*session;
	trace_program_t		*slot;
	const trace_probe_desc_t	*probe;
	const trace_action_t	*action;
	const trace_predicate_t	*pred;
	u32			i;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS ||
	    program == NULL) {
		return (-1);
	}
	if (program->probe_id >= TRACE_PROBE_COUNT ||
	    program->predicate_count > TRACE_MAX_PREDICATES ||
	    program->action_count == 0 ||
	    program->action_count > TRACE_MAX_ACTIONS) {
		return (-1);
	}
	probe = &g_trace_probes[program->probe_id];
	for (i = 0; i < program->predicate_count; i++) {
		pred = &program->predicates[i];
		if (pred->op < TRACE_PRED_EQ || pred->op > TRACE_PRED_MASK) {
			return (-1);
		}
		if (!trace_field_valid(probe, pred->field, 0)) {
			return (-1);
		}
	}
	for (i = 0; i < program->action_count; i++) {
		action = &program->actions[i];
		if (action->kind < TRACE_ACT_RECORD ||
		    action->kind > TRACE_ACT_LQUANTIZE) {
			return (-1);
		}
		if (action->kind != TRACE_ACT_COUNT &&
		    action->kind != TRACE_ACT_RECORD &&
		    action->kind != TRACE_ACT_STACK &&
		    (action->arg >= TRACE_MAX_ARGS ||
		    action->arg >= probe->argc)) {
			return (-1);
		}
		if (!trace_field_valid(probe, action->key, 1)) {
			return (-1);
		}
	}
	session = &g_trace_sessions[session_id];
	if (!session->used || session->program_count >= TRACE_MAX_PROGRAMS) {
		return (-1);
	}
	for (i = 0; i < TRACE_MAX_PROGRAMS; i++) {
		slot = &session->programs[i];
		if (!slot->used) {
			memset(slot, 0, sizeof(*slot));
			slot->used = 1;
			slot->id = i + 1;
			slot->spec = *program;
			session->program_count++;
			trace_rebuild_enable_counts();
			return (0);
		}
	}
	return (-1);
}

int
trace_session_read(int session_id, trace_record_t *out, u32 max_records)
{
	trace_session_t	*session;
	trace_ring_t	*ring;
	trace_record_t	*src;
	u64		cursor, head, live_tail, lost;
	u32		count;
	int		cpu;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS ||
	    out == NULL || max_records == 0) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	count = 0;
	for (cpu = 0; cpu < TRACE_MAX_CPUS && count < max_records; cpu++) {
		ring = &session->rings[cpu];
		if (ring->records == NULL || ring->record_count == 0) {
			continue;
		}
		head = session->active ? ring->head : ring->limit;
		cursor = session->cursor[cpu];
		if (head > ring->record_count) {
			live_tail = head - ring->record_count;
		} else {
			live_tail = 0;
		}
		if (cursor < live_tail) {
			lost = live_tail - cursor;
			cursor = live_tail;
			ring->records_lost += lost;
			session->records_lost += lost;
			__atomic_fetch_add(&g_trace_records_lost, lost,
			    __ATOMIC_RELAXED);
		}
		while (cursor < head && count < max_records) {
			src = &ring->records[cursor & ring->record_mask];
			if (src->seq == cursor) {
				out[count] = *src;
				count++;
				session->records_read++;
			}
			cursor++;
		}
		session->cursor[cpu] = cursor;
	}
	return ((int)count);
}

int
trace_session_read_aggs(int session_id, trace_aggregation_t *out,
    u32 max_aggs, int clear)
{
	trace_session_t		*session;
	trace_aggregation_t	*agg;
	u32			count, i;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS ||
	    out == NULL || max_aggs == 0) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	count = 0;
	for (i = 0; i < TRACE_MAX_AGGREGATIONS && count < max_aggs; i++) {
		agg = &session->aggregations[i];
		if (agg->kind == 0) {
			continue;
		}
		out[count++] = *agg;
	}
	if (clear) {
		memset(session->aggregations, 0, sizeof(session->aggregations));
	}
	return ((int)count);
}

int
trace_session_stats(int session_id, trace_session_stats_t *stats)
{
	trace_session_t		*session;
	trace_aggregation_t	*agg;
	u32			i, aggs;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS ||
	    stats == NULL) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	aggs = 0;
	for (i = 0; i < TRACE_MAX_AGGREGATIONS; i++) {
		agg = &session->aggregations[i];
		if (agg->kind != 0) {
			aggs++;
		}
	}
	memset(stats, 0, sizeof(*stats));
	stats->records_written = session->records_written;
	stats->records_read = session->records_read;
	stats->records_lost = session->records_lost;
	stats->aggregation_count = aggs;
	stats->active = (u32)session->active;
	stats->program_count = session->program_count;
	stats->flags = session->flags;
	return (0);
}

u32
trace_provider_count(void)
{
	return (TRACE_PROVIDER_COUNT);
}

u32
trace_probe_count(void)
{
	return (TRACE_PROBE_COUNT);
}

int
trace_provider_info(u32 index, trace_provider_info_t *info)
{
	u32	i, count;

	if (index >= TRACE_PROVIDER_COUNT || info == NULL) {
		return (-1);
	}
	count = 0;
	for (i = 0; i < TRACE_PROBE_COUNT; i++) {
		if (g_trace_probes[i].provider == index) {
			count++;
		}
	}
	memset(info, 0, sizeof(*info));
	info->id = index;
	info->enabled = g_trace_provider_enabled[index] ? 1 : 0;
	info->probe_count = count;
	trace_copy_name(info->name, TRACE_NAME_LEN,
	    g_trace_provider_names[index]);
	return (0);
}

int
trace_probe_info(u32 index, trace_probe_info_t *info)
{
	const trace_probe_desc_t	*probe;
	u32			i;

	if (index >= TRACE_PROBE_COUNT || info == NULL) {
		return (-1);
	}
	probe = &g_trace_probes[index];
	memset(info, 0, sizeof(*info));
	info->id = probe->id;
	info->provider = probe->provider;
	info->enabled = trace_probe_enabled(probe->id) ? 1 : 0;
	info->argc = probe->argc;
	info->flags = probe->flags;
	trace_copy_name(info->provider_name, TRACE_NAME_LEN,
	    g_trace_provider_names[probe->provider]);
	trace_copy_name(info->module, TRACE_NAME_LEN, probe->module);
	trace_copy_name(info->function, TRACE_NAME_LEN, probe->function);
	trace_copy_name(info->name, TRACE_NAME_LEN, probe->name);
	for (i = 0; i < probe->argc && i < TRACE_MAX_ARGS; i++) {
		info->args[i] = probe->args[i];
	}
	return (0);
}

void
trace_get_stats(trace_stats_t *stats)
{
	if (stats == NULL) {
		return;
	}
	memset(stats, 0, sizeof(*stats));
	stats->records_written = g_trace_records_written;
	stats->records_lost = g_trace_records_lost;
	stats->action_hits = g_trace_action_hits;
	stats->aggregation_updates = g_trace_aggregation_updates;
	stats->provider_count = TRACE_PROVIDER_COUNT;
	stats->probe_count = TRACE_PROBE_COUNT;
	stats->session_count = g_trace_session_count;
	stats->ring_records = g_trace_ring_records;
	stats->enabled = (u32)g_trace_enabled;
	stats->initialized = (u32)g_trace_initialized;
	memcpy(stats->probe_hits, g_trace_probe_hits,
	    sizeof(g_trace_probe_hits));
}
