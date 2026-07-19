/* !DEFINES!

$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type trace_cpu_ring_t as struct with per CPU trace ring state
$define %type trace_session_t as struct with trace read cursors
$define %type trace_event_desc_t as struct with trace event metadata
$define %type trace_record_t as struct with one binary trace record
$define %type trace_session_filter_t as struct with session predicates
$define %type registers_t as struct with CPU register snapshot
$define %type thread_t as struct with per-thread CPU context and state
$define %type process_t as struct with process control block

$define %func trace_mask_set as procedure with args u64 *, u16, int
$define %func trace_mask_get as function with args const u64 *, u16
$define %func trace_round_ring_records as function with args int
$define %func trace_source_name as function with args u16
$define %func trace_event_desc as function with args u16
$define %func trace_source_enabled as function with args u16
$define %func trace_default_filter as procedure with args filter
$define %func trace_record_match as function with args session, record
$define %func trace_capture_stack as procedure with args record, regs
$define %func trace_register_builtin_events as procedure with args void
$define %func trace_apply_config as procedure with args void
$define %func trace_cpu_ring_alloc as function with args int
$define %func trace_init as procedure with args void
$define %func trace_cpu_online as procedure with args void
$define %func trace_is_initialized as function with args void
$define %func trace_is_enabled as function with args void
$define %func trace_clock_read as function with args void
$define %func trace_register_event as function with args event desc
$define %func trace_event_enabled as function with args u16
$define %func trace_enable_source as function with args u16, int
$define %func trace_emit as procedure with args event, flags, regs, args
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
$define %func trace_session_open as function with args u32
$define %func trace_session_close as function with args int
$define %func trace_session_start as function with args int
$define %func trace_session_stop as function with args int
$define %func trace_session_flush as function with args int
$define %func trace_session_filter as function with args session, filter
$define %func trace_session_enable_event as function with args int, u16, int
$define %func trace_session_enable_source as function with args int, u16, int
$define %func trace_session_read as function with args session, records, count
$define %func trace_session_stats as function with args int, stats
$define %func trace_get_stats as procedure with args trace_stats_t *

*/

/* !SPACE!

$space %internal trace_mask_set, trace_mask_get
$space %internal trace_round_ring_records
$space %internal trace_default_filter, trace_record_match
$space %internal trace_capture_stack, trace_register_builtin_events
$space %internal trace_apply_config, trace_cpu_ring_alloc
$space %export trace_init, trace_cpu_online
$space %export trace_is_initialized, trace_is_enabled, trace_clock_read
$space %export trace_register_event, trace_event_desc
$space %export trace_event_enabled, trace_enable_source
$space %export trace_source_name, trace_source_enabled, trace_emit
$space %export trace_sample_tick, trace_irq_enter, trace_irq_exit
$space %export trace_exception, trace_syscall_enter, trace_syscall_exit
$space %export trace_sched_tick, trace_sched_switch
$space %export trace_thread_runtime_cycles
$space %export trace_kqueue_create, trace_kqueue_destroy
$space %export trace_knote_ready, trace_kevent_wait, trace_kevent_return
$space %export trace_event_timer_tick
$space %export trace_session_open, trace_session_close
$space %export trace_session_start, trace_session_stop, trace_session_flush
$space %export trace_session_filter
$space %export trace_session_enable_event, trace_session_enable_source
$space %export trace_session_read, trace_session_stats, trace_get_stats

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
#include <kernel/drivers/timer.h>
#include <kernel/cm/cm.h>
#include <kernel/process.h>
#include <kernel/smp/smp.h>
#include <kernel/thread.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	TRACE_IRQ_VECTORS	256
#define	TRACE_STACK_SCAN_LIMIT	0x100000ULL

typedef struct trace_cpu_ring {
	trace_record_t	*records;
	u64		head;
	u64		records_written;
	u64		records_lost;
	u64		irq_start[TRACE_IRQ_VECTORS];
	u64		last_sample_tsc;
	u32		record_count;
	u32		record_mask;
	u32		sample_ticks;
	int		online;
} trace_cpu_ring_t;

typedef struct trace_session {
	int			used;
	int			active;
	u32			flags;
	trace_session_filter_t	filter;
	u64			cursor[TRACE_MAX_CPUS];
	u64			limit[TRACE_MAX_CPUS];
	u64			read_records;
	u64			lost_records;
} trace_session_t;

static trace_cpu_ring_t	g_trace_cpu[TRACE_MAX_CPUS];
static trace_session_t	g_trace_sessions[TRACE_MAX_SESSIONS];
static trace_event_desc_t g_trace_events[TRACE_MAX_EVENTS];
static u64		g_trace_enabled_events[TRACE_EVENT_WORDS];
static u64		g_trace_source_mask;
static u64		g_trace_event_counts[TRACE_MAX_EVENTS];
static u64		g_trace_source_counts[TRACE_SOURCE_COUNT];
static u64		g_trace_records_written;
static u64		g_trace_records_lost;
static u32		g_trace_ring_records;
static u32		g_trace_sample_period;
static u32		g_trace_stack_depth;
static u32		g_trace_session_count;
static int		g_trace_initialized;
static int		g_trace_enabled;

static const char *g_trace_source_names[TRACE_SOURCE_COUNT] = {
	"main",
	"pmu",
	"syscall",
	"handler",
	"scheduler",
	"event",
	"user"
};

static const char *g_trace_source_registry_names[TRACE_SOURCE_COUNT] = {
	"Main",
	"Pmu",
	"Syscall",
	"Handler",
	"Scheduler",
	"Event",
	"User"
};

static const trace_field_desc_t trace_fields_pmu[] = {
	{ "cycles", 0, 0 },
	{ "instructions", 1, 0 },
	{ "cache_references", 2, 0 },
	{ "cache_misses", 3, 0 },
	{ "branch_instructions", 4, 0 },
	{ "branch_misses", 5, 0 }
};

static const trace_field_desc_t trace_fields_syscall_enter[] = {
	{ "number", 0, 0 },
	{ "arg0", 1, 0 },
	{ "arg1", 2, 0 },
	{ "arg2", 3, 0 },
	{ "arg3", 4, 0 },
	{ "arg4", 5, 0 }
};

static const trace_field_desc_t trace_fields_syscall_exit[] = {
	{ "number", 0, 0 },
	{ "return", 1, 0 },
	{ "cycles", 2, 0 }
};

static const trace_field_desc_t trace_fields_irq[] = {
	{ "vector", 0, 0 },
	{ "err_code", 1, 0 },
	{ "cs", 2, 0 },
	{ "cycles", 3, 0 }
};

static const trace_field_desc_t trace_fields_sched_switch[] = {
	{ "prev_pid", 0, 0 },
	{ "prev_tid", 1, 0 },
	{ "next_pid", 2, 0 },
	{ "next_tid", 3, 0 },
	{ "reason", 4, 0 },
	{ "prev_cycles", 5, 0 }
};

static const trace_field_desc_t trace_fields_kqueue[] = {
	{ "kqueue", 0, 0 },
	{ "a", 1, 0 },
	{ "b", 2, 0 },
	{ "c", 3, 0 }
};

static const trace_field_desc_t trace_fields_user_mark[] = {
	{ "id", 0, 0 },
	{ "a0", 1, 0 },
	{ "a1", 2, 0 },
	{ "a2", 3, 0 },
	{ "a3", 4, 0 },
	{ "a4", 5, 0 }
};

static const trace_event_desc_t trace_builtin_events[] = {
	{
		TRACE_EV_CORE_BOOT, TRACE_SOURCE_MAIN, 0,
		"core", "boot", NULL, 0
	},
	{
		TRACE_EV_PROFILE_SAMPLE, TRACE_SOURCE_MAIN, 0,
		"profile", "sample", trace_fields_pmu, 6
	},
	{
		TRACE_EV_PMU_COUNTERS, TRACE_SOURCE_PMU, 0,
		"pmu", "counters", trace_fields_pmu, 6
	},
	{
		TRACE_EV_SYSCALL_ENTER, TRACE_SOURCE_SYSCALL, 0,
		"syscall", "enter", trace_fields_syscall_enter, 6
	},
	{
		TRACE_EV_SYSCALL_EXIT, TRACE_SOURCE_SYSCALL, 0,
		"syscall", "exit", trace_fields_syscall_exit, 3
	},
	{
		TRACE_EV_IRQ_ENTER, TRACE_SOURCE_HANDLER, 0,
		"handler", "irq_enter", trace_fields_irq, 4
	},
	{
		TRACE_EV_IRQ_EXIT, TRACE_SOURCE_HANDLER, 0,
		"handler", "irq_exit", trace_fields_irq, 4
	},
	{
		TRACE_EV_EXCEPTION, TRACE_SOURCE_HANDLER, 0,
		"handler", "exception", trace_fields_irq, 4
	},
	{
		TRACE_EV_SCHED_TICK, TRACE_SOURCE_SCHEDULER, 0,
		"scheduler", "tick", NULL, 0
	},
	{
		TRACE_EV_SCHED_SWITCH, TRACE_SOURCE_SCHEDULER, 0,
		"scheduler", "switch", trace_fields_sched_switch, 6
	},
	{
		TRACE_EV_EVENT_KQUEUE_CREATE, TRACE_SOURCE_EVENT, 0,
		"event", "kqueue_create", trace_fields_kqueue, 4
	},
	{
		TRACE_EV_EVENT_KQUEUE_DESTROY, TRACE_SOURCE_EVENT, 0,
		"event", "kqueue_destroy", trace_fields_kqueue, 4
	},
	{
		TRACE_EV_EVENT_KNOTE_READY, TRACE_SOURCE_EVENT, 0,
		"event", "knote_ready", trace_fields_kqueue, 4
	},
	{
		TRACE_EV_EVENT_KEVENT_WAIT, TRACE_SOURCE_EVENT, 0,
		"event", "kevent_wait", trace_fields_kqueue, 4
	},
	{
		TRACE_EV_EVENT_KEVENT_RETURN, TRACE_SOURCE_EVENT, 0,
		"event", "kevent_return", trace_fields_kqueue, 4
	},
	{
		TRACE_EV_EVENT_TIMER_TICK, TRACE_SOURCE_EVENT, 0,
		"event", "timer_tick", NULL, 0
	},
	{
		TRACE_EV_USER_MARK, TRACE_SOURCE_USER, 0,
		"user", "mark", trace_fields_user_mark, 6
	}
};

static void
trace_mask_set(u64 *mask, u16 event, int enabled)
{
	u64	bit;
	u32	word;

	if (event >= TRACE_MAX_EVENTS) {
		return;
	}
	word = event / 64;
	bit = 1ULL << (event % 64);
	if (enabled) {
		mask[word] |= bit;
	} else {
		mask[word] &= ~bit;
	}
}

static int
trace_mask_get(const u64 *mask, u16 event)
{
	u64	bit;
	u32	word;

	if (event >= TRACE_MAX_EVENTS) {
		return (0);
	}
	word = event / 64;
	bit = 1ULL << (event % 64);
	return ((mask[word] & bit) != 0);
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

const char *
trace_source_name(u16 source)
{
	if (source >= TRACE_SOURCE_COUNT) {
		return ("unknown");
	}
	return (g_trace_source_names[source]);
}

static void
trace_default_filter(trace_session_filter_t *filter)
{
	int	i;

	memset(filter, 0, sizeof(*filter));
	filter->source_mask = TRACE_SOURCE_MASK_ALL;
	filter->pid = -1;
	filter->tid = -1;
	filter->cpu = -1;
	filter->flags = 0;
	for (i = 0; i < TRACE_EVENT_WORDS; i++) {
		filter->event_mask[i] = ~0ULL;
	}
}

static int
trace_record_match(trace_session_t *session, const trace_record_t *rec)
{
	trace_session_filter_t	*filter;

	filter = &session->filter;
	if ((filter->source_mask & (1ULL << rec->source)) == 0) {
		return (0);
	}
	if (!trace_mask_get(filter->event_mask, (u16)rec->event)) {
		return (0);
	}
	if ((filter->flags & TRACE_FILTER_HAS_PID) &&
	    filter->pid != (int)rec->pid) {
		return (0);
	}
	if ((filter->flags & TRACE_FILTER_HAS_TID) &&
	    filter->tid != (int)rec->tid) {
		return (0);
	}
	if ((filter->flags & TRACE_FILTER_HAS_CPU) &&
	    filter->cpu != (int)rec->cpu) {
		return (0);
	}
	return (1);
}

static void
trace_capture_stack(trace_record_t *rec, const registers_t *regs)
{
	u64	*frame;
	u64	bp, next, ret;
	u32	limit;
	u16	count;

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

static void
trace_register_builtin_events(void)
{
	const trace_event_desc_t	*desc;
	u32			count, i;

	count = sizeof(trace_builtin_events) /
	    sizeof(trace_builtin_events[0]);
	for (i = 0; i < count; i++) {
		desc = &trace_builtin_events[i];
		trace_register_event(desc);
	}
}

static void
trace_apply_config(void)
{
	const char	*name;
	int		enabled;
	u16		source;

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

	memset(g_trace_enabled_events, 0, sizeof(g_trace_enabled_events));
	g_trace_source_mask = 0;
	for (source = 0; source < TRACE_SOURCE_COUNT; source++) {
		name = g_trace_source_registry_names[source];
		enabled = cm_get_bool_default("SYSTEM", "Trace.Sources",
		    name, 1);
		trace_enable_source(source, enabled);
	}
}

static int
trace_cpu_ring_alloc(int cpu)
{
	trace_cpu_ring_t	*ring;
	size_t			bytes;

	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return (-1);
	}

	ring = &g_trace_cpu[cpu];
	if (ring->records != NULL) {
		ring->online = 1;
		return (0);
	}

	bytes = sizeof(trace_record_t) * g_trace_ring_records;
	ring->records = kmem_calloc(g_trace_ring_records,
	    sizeof(trace_record_t));
	if (ring->records == NULL) {
		ring->records_lost++;
		g_trace_records_lost++;
		return (-1);
	}

	ring->head = 0;
	ring->records_written = 0;
	ring->records_lost = 0;
	ring->record_count = g_trace_ring_records;
	ring->record_mask = g_trace_ring_records - 1;
	ring->sample_ticks = 0;
	ring->last_sample_tsc = trace_clock_read();
	ring->online = 1;
	memset(ring->irq_start, 0, sizeof(ring->irq_start));

	printk("[TRACE] CPU %d ring: %u records, %u KB\n", cpu,
	    g_trace_ring_records, (u32)(bytes / 1024));
	return (0);
}

void
trace_init(void)
{
	memset(g_trace_cpu, 0, sizeof(g_trace_cpu));
	memset(g_trace_sessions, 0, sizeof(g_trace_sessions));
	memset(g_trace_events, 0, sizeof(g_trace_events));
	memset(g_trace_event_counts, 0, sizeof(g_trace_event_counts));
	memset(g_trace_source_counts, 0, sizeof(g_trace_source_counts));
	g_trace_records_written = 0;
	g_trace_records_lost = 0;
	g_trace_session_count = 0;

	trace_register_builtin_events();
	trace_apply_config();
	g_trace_initialized = 1;

	if (!g_trace_enabled) {
		printk("[TRACE] disabled by registry\n");
		return;
	}

	trace_pmu_init();
	trace_cpu_online();
	trace_emit(TRACE_EV_CORE_BOOT, 0, NULL, g_trace_ring_records,
	    g_trace_sample_period, g_trace_stack_depth, 0, 0, 0);
	printk("[TRACE] enabled sources=0x%llx ring=%u sample_ticks=%u\n",
	    g_trace_source_mask, g_trace_ring_records,
	    g_trace_sample_period);
}

void
trace_cpu_online(void)
{
	int	cpu;

	if (!g_trace_initialized || !g_trace_enabled) {
		return;
	}

	cpu = smp_cpu_index();
	if (trace_cpu_ring_alloc(cpu) == 0) {
		trace_pmu_cpu_online(cpu);
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

	__asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return (((u64)hi << 32) | lo);
}

int
trace_register_event(const trace_event_desc_t *desc)
{
	if (desc == NULL || desc->id >= TRACE_MAX_EVENTS ||
	    desc->source >= TRACE_SOURCE_COUNT) {
		return (-1);
	}
	g_trace_events[desc->id] = *desc;
	return (0);
}

const trace_event_desc_t *
trace_event_desc(u16 event)
{
	if (event >= TRACE_MAX_EVENTS) {
		return (NULL);
	}
	if (g_trace_events[event].name == NULL) {
		return (NULL);
	}
	return (&g_trace_events[event]);
}

int
trace_event_enabled(u16 event)
{
	if (!trace_is_enabled() || event >= TRACE_MAX_EVENTS) {
		return (0);
	}
	if (g_trace_events[event].name == NULL) {
		return (0);
	}
	return (trace_mask_get(g_trace_enabled_events, event));
}

int
trace_enable_source(u16 source, int enabled)
{
	u16	event;

	if (source >= TRACE_SOURCE_COUNT) {
		return (-1);
	}
	if (enabled) {
		g_trace_source_mask |= 1ULL << source;
	} else {
		g_trace_source_mask &= ~(1ULL << source);
	}

	for (event = 0; event < TRACE_MAX_EVENTS; event++) {
		if (g_trace_events[event].name == NULL) {
			continue;
		}
		if (g_trace_events[event].source == source) {
			trace_mask_set(g_trace_enabled_events, event,
			    enabled);
		}
	}
	return (0);
}

int
trace_source_enabled(u16 source)
{
	if (!trace_is_enabled() || source >= TRACE_SOURCE_COUNT) {
		return (0);
	}
	return ((g_trace_source_mask & (1ULL << source)) != 0);
}

void
trace_emit(u16 event, u32 flags, const registers_t *regs,
    u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	trace_cpu_ring_t	*ring;
	trace_record_t		rec;
	trace_event_desc_t	*desc;
	thread_t		*td;
	process_t		*proc;
	u64			seq;
	u32			index;
	int			cpu;

	if (!trace_event_enabled(event)) {
		return;
	}

	cpu = smp_cpu_index();
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		g_trace_records_lost++;
		return;
	}

	ring = &g_trace_cpu[cpu];
	if (ring->records == NULL || ring->record_count == 0) {
		ring->records_lost++;
		g_trace_records_lost++;
		return;
	}

	desc = &g_trace_events[event];
	td = thread_current();
	proc = NULL;
	if (td != NULL) {
		proc = td->proc;
	}

	memset(&rec, 0, sizeof(rec));
	seq = ring->head;
	index = (u32)(seq & ring->record_mask);

	rec.seq = seq;
	rec.tsc = trace_clock_read();
	if (timer_is_initialized()) {
		rec.ticks = timer_get_ticks();
	}
	rec.cpu = (u32)cpu;
	rec.event = event;
	rec.source = desc->source;
	rec.flags = flags;
	if (regs != NULL && (regs->cs & 3) != 0) {
		rec.flags |= TRACE_REC_F_USER;
	}
	if (proc != NULL) {
		rec.pid = proc->pid;
	}
	if (td != NULL) {
		rec.tid = td->tid;
	}
	if (regs != NULL) {
		rec.ip = regs->rip;
		rec.sp = regs->rsp;
		rec.bp = regs->rbp;
	}
	rec.args[0] = arg0;
	rec.args[1] = arg1;
	rec.args[2] = arg2;
	rec.args[3] = arg3;
	rec.args[4] = arg4;
	rec.args[5] = arg5;
	trace_capture_stack(&rec, regs);

	ring->records[index] = rec;
	ring->head = seq + 1;
	ring->records_written++;
	__atomic_fetch_add(&g_trace_records_written, 1,
	    __ATOMIC_RELAXED);
	__atomic_fetch_add(&g_trace_event_counts[event], 1,
	    __ATOMIC_RELAXED);
	__atomic_fetch_add(&g_trace_source_counts[desc->source], 1,
	    __ATOMIC_RELAXED);
}

void
trace_sample_tick(registers_t *regs)
{
	trace_cpu_ring_t	*ring;
	u64			values[TRACE_PMU_COUNTER_COUNT];
	u64			now, delta;
	int			cpu, sample_enabled, counters_enabled;

	sample_enabled = trace_event_enabled(TRACE_EV_PROFILE_SAMPLE);
	counters_enabled = trace_event_enabled(TRACE_EV_PMU_COUNTERS);
	if (!sample_enabled && !counters_enabled) {
		return;
	}

	cpu = smp_cpu_index();
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return;
	}
	ring = &g_trace_cpu[cpu];
	if (ring->records == NULL) {
		return;
	}

	ring->sample_ticks++;
	if (ring->sample_ticks < g_trace_sample_period) {
		return;
	}
	ring->sample_ticks = 0;

	memset(values, 0, sizeof(values));
	trace_pmu_sample(cpu, values, TRACE_PMU_COUNTER_COUNT);
	if (values[TRACE_PMU_CYCLES] == 0) {
		now = trace_clock_read();
		delta = now - ring->last_sample_tsc;
		ring->last_sample_tsc = now;
		values[TRACE_PMU_CYCLES] = delta;
	}

	if (sample_enabled) {
		trace_emit(TRACE_EV_PROFILE_SAMPLE, TRACE_REC_F_PMU_VALID,
		    regs, values[0], values[1], values[2], values[3],
		    values[4], values[5]);
	}
	if (counters_enabled) {
		trace_emit(TRACE_EV_PMU_COUNTERS, TRACE_REC_F_PMU_VALID,
		    regs, values[0], values[1], values[2], values[3],
		    values[4], values[5]);
	}
}

void
trace_irq_enter(registers_t *regs)
{
	trace_cpu_ring_t	*ring;
	u64			now;
	int			cpu;

	if (regs == NULL ||
	    !trace_event_enabled(TRACE_EV_IRQ_ENTER)) {
		return;
	}
	cpu = smp_cpu_index();
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return;
	}
	ring = &g_trace_cpu[cpu];
	now = trace_clock_read();
	if (regs->int_no < TRACE_IRQ_VECTORS) {
		ring->irq_start[regs->int_no] = now;
	}
	trace_emit(TRACE_EV_IRQ_ENTER, 0, regs, regs->int_no,
	    regs->err_code, regs->cs, 0, 0, 0);
}

void
trace_irq_exit(registers_t *regs)
{
	trace_cpu_ring_t	*ring;
	u64			now, start, cycles;
	int			cpu;

	if (regs == NULL ||
	    !trace_event_enabled(TRACE_EV_IRQ_EXIT)) {
		return;
	}
	cpu = smp_cpu_index();
	if (cpu < 0 || cpu >= TRACE_MAX_CPUS) {
		return;
	}
	ring = &g_trace_cpu[cpu];
	now = trace_clock_read();
	start = 0;
	if (regs->int_no < TRACE_IRQ_VECTORS) {
		start = ring->irq_start[regs->int_no];
	}
	cycles = 0;
	if (start != 0) {
		cycles = now - start;
	}
	trace_emit(TRACE_EV_IRQ_EXIT, 0, regs, regs->int_no,
	    regs->err_code, regs->cs, cycles, 0, 0);
}

void
trace_exception(registers_t *regs)
{
	u64	cr2;

	if (regs == NULL) {
		return;
	}
	cr2 = 0;
	if (regs->int_no == 14) {
		__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
	}
	trace_emit(TRACE_EV_EXCEPTION, 0, regs, regs->int_no,
	    regs->err_code, regs->cs, cr2, 0, 0);
}

u64
trace_syscall_enter(registers_t *regs)
{
	u64	start;

	start = trace_clock_read();
	if (regs == NULL) {
		return (start);
	}
	trace_emit(TRACE_EV_SYSCALL_ENTER, 0, regs, regs->rax,
	    regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);
	return (start);
}

void
trace_syscall_exit(registers_t *regs, u64 number, u64 ret,
    u64 start_tsc)
{
	u64	cycles;

	cycles = 0;
	if (start_tsc != 0) {
		cycles = trace_clock_read() - start_tsc;
	}
	trace_emit(TRACE_EV_SYSCALL_EXIT, 0, regs, number, ret,
	    cycles, 0, 0, 0);
}

void
trace_sched_tick(registers_t *regs)
{
	trace_emit(TRACE_EV_SCHED_TICK, 0, regs, 0, 0, 0, 0, 0, 0);
}

void
trace_sched_switch(struct thread *prev, struct thread *next,
    u32 reason, registers_t *regs)
{
	thread_t	*prev_td;
	thread_t	*next_td;
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

	trace_emit(TRACE_EV_SCHED_SWITCH, 0, regs, prev_pid, prev_tid,
	    next_pid, next_tid, reason, prev_delta);
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
	trace_emit(TRACE_EV_EVENT_KQUEUE_CREATE, 0, NULL, (u64)kq_idx,
	    pid, 0, 0, 0, 0);
}

void
trace_kqueue_destroy(int kq_idx)
{
	trace_emit(TRACE_EV_EVENT_KQUEUE_DESTROY, 0, NULL, (u64)kq_idx,
	    0, 0, 0, 0, 0);
}

void
trace_knote_ready(s16 filter, u64 ident, s64 data)
{
	trace_emit(TRACE_EV_EVENT_KNOTE_READY, 0, NULL, (u64)filter,
	    ident, (u64)data, 0, 0, 0);
}

void
trace_kevent_wait(int kq_idx, int nchanges, int nevents,
    s64 timeout_ms)
{
	trace_emit(TRACE_EV_EVENT_KEVENT_WAIT, 0, NULL, (u64)kq_idx,
	    (u64)nchanges, (u64)nevents, (u64)timeout_ms, 0, 0);
}

void
trace_kevent_return(int kq_idx, int count, s64 timeout_ms)
{
	trace_emit(TRACE_EV_EVENT_KEVENT_RETURN, 0, NULL, (u64)kq_idx,
	    (u64)count, (u64)timeout_ms, 0, 0, 0);
}

void
trace_event_timer_tick(void)
{
	trace_emit(TRACE_EV_EVENT_TIMER_TICK, 0, NULL, 0, 0, 0, 0, 0, 0);
}

int
trace_session_open(u32 flags)
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
		session->active = 1;
		session->flags = flags;
		trace_default_filter(&session->filter);
		for (cpu = 0; cpu < TRACE_MAX_CPUS; cpu++) {
			session->cursor[cpu] = g_trace_cpu[cpu].head;
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

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	memset(session, 0, sizeof(*session));
	if (g_trace_session_count > 0) {
		g_trace_session_count--;
	}
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
			session->cursor[cpu] = g_trace_cpu[cpu].head;
			session->limit[cpu] = 0;
		}
	}
	session->active = 1;
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
			session->limit[cpu] = g_trace_cpu[cpu].head;
		}
	}
	session->active = 0;
	return (0);
}

int
trace_session_flush(int session_id)
{
	trace_session_t	*session;
	u64		head;
	int		cpu;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	for (cpu = 0; cpu < TRACE_MAX_CPUS; cpu++) {
		if (session->active) {
			head = g_trace_cpu[cpu].head;
		} else {
			head = session->limit[cpu];
		}
		session->cursor[cpu] = head;
	}
	session->read_records = 0;
	session->lost_records = 0;
	return (0);
}

int
trace_session_filter(int session_id,
    const trace_session_filter_t *filter)
{
	trace_session_t	*session;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS ||
	    filter == NULL) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	session->filter = *filter;
	return (0);
}

int
trace_session_enable_event(int session_id, u16 event, int enabled)
{
	trace_session_t	*session;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS ||
	    event >= TRACE_MAX_EVENTS) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	trace_mask_set(session->filter.event_mask, event, enabled);
	return (0);
}

int
trace_session_enable_source(int session_id, u16 source, int enabled)
{
	trace_session_t	*session;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS ||
	    source >= TRACE_SOURCE_COUNT) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}
	if (enabled) {
		session->filter.source_mask |= 1ULL << source;
	} else {
		session->filter.source_mask &= ~(1ULL << source);
	}
	return (0);
}

int
trace_session_read(int session_id, trace_record_t *out, u32 max_records)
{
	trace_cpu_ring_t	*ring;
	trace_session_t		*session;
	trace_record_t		*src;
	u64			cursor, head, live_head, live_tail, lost;
	u32			count;
	int			cpu;

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
		ring = &g_trace_cpu[cpu];
		if (ring->records == NULL || ring->record_count == 0) {
			continue;
		}
		live_head = ring->head;
		if (session->active) {
			head = live_head;
		} else {
			head = session->limit[cpu];
		}
		cursor = session->cursor[cpu];
		if (live_head > ring->record_count) {
			live_tail = live_head - ring->record_count;
		} else {
			live_tail = 0;
		}
		if (cursor < live_tail) {
			lost = live_tail - cursor;
			cursor = live_tail;
			session->lost_records += lost;
			ring->records_lost += lost;
			__atomic_fetch_add(&g_trace_records_lost,
			    lost, __ATOMIC_RELAXED);
		}
		while (cursor < head && count < max_records) {
			src = &ring->records[cursor & ring->record_mask];
			if (src->seq == cursor &&
			    trace_record_match(session, src)) {
				out[count] = *src;
				count++;
				session->read_records++;
			}
			cursor++;
		}
		session->cursor[cpu] = cursor;
	}

	return ((int)count);
}

int
trace_session_stats(int session_id, trace_session_stats_t *stats)
{
	trace_session_t	*session;

	if (session_id < 0 || session_id >= TRACE_MAX_SESSIONS ||
	    stats == NULL) {
		return (-1);
	}
	session = &g_trace_sessions[session_id];
	if (!session->used) {
		return (-1);
	}

	memset(stats, 0, sizeof(*stats));
	stats->read_records = session->read_records;
	stats->lost_records = session->lost_records;
	stats->active = (u32)session->active;
	stats->flags = session->flags;
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
	stats->ring_records = g_trace_ring_records;
	stats->session_count = g_trace_session_count;
	stats->enabled = (u32)g_trace_enabled;
	stats->initialized = (u32)g_trace_initialized;
	memcpy(stats->event_count, g_trace_event_counts,
	    sizeof(g_trace_event_counts));
	memcpy(stats->source_count, g_trace_source_counts,
	    sizeof(g_trace_source_counts));
}
