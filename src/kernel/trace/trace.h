/* !DEFINES!

$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type registers_t as struct with CPU register snapshot
$define %type trace_arg_desc_t as struct with probe argument metadata
$define %type trace_probe_desc_t as struct with provider probe metadata
$define %type trace_predicate_t as struct with one safe predicate clause
$define %type trace_action_t as struct with one safe probe action
$define %type trace_program_spec_t as struct with loadable probe program
$define %type trace_record_t as struct with one trace buffer record
$define %type trace_aggregation_t as struct with one aggregation bucket
$define %type trace_provider_info_t as struct with provider snapshot
$define %type trace_probe_info_t as struct with probe snapshot
$define %type trace_session_stats_t as struct with session counters
$define %type trace_stats_t as struct with global trace counters

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
$define %func trace_thread_runtime_cycles as function with args struct thread *
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
$define %func trace_pmu_init as procedure with args void
$define %func trace_pmu_cpu_online as procedure with args int
$define %func trace_pmu_sample as procedure with args int, u64 *, u32
$define %func trace_pmu_counter_count as function with args void
$define %func trace_pmu_counter_name as function with args u32
$define %func trace_pmu_counter_active as function with args u32

*/

/* !SPACE!

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
$space %export trace_pmu_init, trace_pmu_cpu_online
$space %export trace_pmu_sample, trace_pmu_counter_count
$space %export trace_pmu_counter_name, trace_pmu_counter_active

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

#ifndef KERNEL_TRACE_TRACE_H
#define KERNEL_TRACE_TRACE_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

#define	TRACE_MAX_CPUS			32
#define	TRACE_MAX_PROVIDERS		16
#define	TRACE_MAX_PROBES		256
#define	TRACE_MAX_ARGS			8
#define	TRACE_MAX_PREDICATES		8
#define	TRACE_MAX_ACTIONS		16
#define	TRACE_MAX_PROGRAMS		128
#define	TRACE_MAX_SESSIONS		16
#define	TRACE_MAX_AGGREGATIONS		256
#define	TRACE_RECORD_STACK		8
#define	TRACE_NAME_LEN			32
#define	TRACE_MIN_RING_RECORDS		128
#define	TRACE_DEFAULT_RING_RECORDS	1024
#define	TRACE_MAX_RING_RECORDS		16384

#define	TRACE_OPEN_PRIVILEGED		0x00000001
#define	TRACE_OPEN_KERNEL_STACK		0x00000002

#define	TRACE_CLEAR_RECORDS		0x00000001
#define	TRACE_CLEAR_PROGRAMS		0x00000002
#define	TRACE_CLEAR_AGGS		0x00000004
#define	TRACE_CLEAR_ALL \
	(TRACE_CLEAR_RECORDS | TRACE_CLEAR_PROGRAMS | TRACE_CLEAR_AGGS)

#define	TRACE_REC_F_USER		0x00000001
#define	TRACE_REC_F_KERNEL_STACK		0x00000002
#define	TRACE_REC_F_PMU_VALID		0x00000004
#define	TRACE_REC_F_DROPPED_BEFORE	0x00000008

struct thread;

enum trace_arg_type {
	TRACE_ARG_U64 = 1,
	TRACE_ARG_S64,
	TRACE_ARG_PID,
	TRACE_ARG_TID,
	TRACE_ARG_CPU,
	TRACE_ARG_ID,
	TRACE_ARG_PTR,
	TRACE_ARG_CYCLES,
	TRACE_ARG_ERRNO,
	TRACE_ARG_FLAGS,
	TRACE_ARG_BYTES
};

enum trace_field_id {
	TRACE_FIELD_NONE = 0,
	TRACE_FIELD_PID,
	TRACE_FIELD_TID,
	TRACE_FIELD_CPU,
	TRACE_FIELD_PROBE,
	TRACE_FIELD_ARG0 = 16,
	TRACE_FIELD_ARG1,
	TRACE_FIELD_ARG2,
	TRACE_FIELD_ARG3,
	TRACE_FIELD_ARG4,
	TRACE_FIELD_ARG5,
	TRACE_FIELD_ARG6,
	TRACE_FIELD_ARG7
};

enum trace_predicate_op {
	TRACE_PRED_EQ = 1,
	TRACE_PRED_NE,
	TRACE_PRED_LT,
	TRACE_PRED_LE,
	TRACE_PRED_GT,
	TRACE_PRED_GE,
	TRACE_PRED_MASK
};

enum trace_action_kind {
	TRACE_ACT_RECORD = 1,
	TRACE_ACT_STACK,
	TRACE_ACT_COUNT,
	TRACE_ACT_SUM,
	TRACE_ACT_MIN,
	TRACE_ACT_MAX,
	TRACE_ACT_QUANTIZE,
	TRACE_ACT_LQUANTIZE
};

enum trace_provider_id {
	TRACE_PROVIDER_CORE = 0,
	TRACE_PROVIDER_PROFILE,
	TRACE_PROVIDER_PMU,
	TRACE_PROVIDER_SYSCALL,
	TRACE_PROVIDER_IRQ,
	TRACE_PROVIDER_SCHED,
	TRACE_PROVIDER_EVENT,
	TRACE_PROVIDER_USER,
	TRACE_PROVIDER_COUNT
};

enum trace_probe_id {
	TRACE_PROBE_CORE_BOOT = 0,
	TRACE_PROBE_PROFILE_TICK,
	TRACE_PROBE_PMU_COUNTERS,
	TRACE_PROBE_SYSCALL_ENTRY,
	TRACE_PROBE_SYSCALL_RETURN,
	TRACE_PROBE_IRQ_ENTRY,
	TRACE_PROBE_IRQ_RETURN,
	TRACE_PROBE_IRQ_EXCEPTION,
	TRACE_PROBE_SCHED_TICK,
	TRACE_PROBE_SCHED_SWITCH,
	TRACE_PROBE_EVENT_KQUEUE_CREATE,
	TRACE_PROBE_EVENT_KQUEUE_DESTROY,
	TRACE_PROBE_EVENT_KNOTE_READY,
	TRACE_PROBE_EVENT_KEVENT_WAIT,
	TRACE_PROBE_EVENT_KEVENT_RETURN,
	TRACE_PROBE_EVENT_TIMER_TICK,
	TRACE_PROBE_USER_MARK,
	TRACE_PROBE_COUNT
};

enum trace_sched_reason {
	TRACE_SCHED_BOOT = 1,
	TRACE_SCHED_PREEMPT,
	TRACE_SCHED_SLEEP,
	TRACE_SCHED_EXIT,
	TRACE_SCHED_WAKE
};

enum trace_pmu_counter_id {
	TRACE_PMU_CYCLES = 0,
	TRACE_PMU_INSTRUCTIONS,
	TRACE_PMU_CACHE_REFERENCES,
	TRACE_PMU_CACHE_MISSES,
	TRACE_PMU_BRANCH_INSTRUCTIONS,
	TRACE_PMU_BRANCH_MISSES,
	TRACE_PMU_COUNTER_COUNT
};

typedef struct trace_arg_desc {
	const char	*name;
	u32		type;
	u32		flags;
} trace_arg_desc_t;

typedef struct trace_probe_desc {
	u32			 id;
	u32			 provider;
	u32			 flags;
	const char		*module;
	const char		*function;
	const char		*name;
	const trace_arg_desc_t	*args;
	u32			 argc;
} trace_probe_desc_t;

typedef struct trace_predicate {
	u32	field;
	u32	op;
	u64	value;
} trace_predicate_t;

typedef struct trace_action {
	u32	kind;
	u32	arg;
	u32	key;
	u32	id;
	u64	value;
} trace_action_t;

typedef struct trace_program_spec {
	u32			probe_id;
	u32			flags;
	u32			predicate_count;
	u32			action_count;
	trace_predicate_t	predicates[TRACE_MAX_PREDICATES];
	trace_action_t		actions[TRACE_MAX_ACTIONS];
} trace_program_spec_t;

typedef struct trace_record {
	u64	seq;
	u64	tsc;
	u64	ticks;
	u64	pid;
	u64	tid;
	u64	ip;
	u64	sp;
	u64	bp;
	u64	probe_id;
	u64	action_id;
	u64	args[TRACE_MAX_ARGS];
	u64	stack[TRACE_RECORD_STACK];
	u32	cpu;
	u32	flags;
	u32	argc;
	u32	stack_count;
} trace_record_t;

typedef struct trace_aggregation {
	u32	id;
	u32	kind;
	u32	probe_id;
	u32	arg;
	u64	key[4];
	u64	value;
	u64	count;
} trace_aggregation_t;

typedef struct trace_provider_info {
	u32	id;
	u32	enabled;
	u32	probe_count;
	u32	reserved;
	char	name[TRACE_NAME_LEN];
} trace_provider_info_t;

typedef struct trace_probe_info {
	u32	id;
	u32	provider;
	u32	enabled;
	u32	argc;
	u32	flags;
	u32	reserved;
	char	provider_name[TRACE_NAME_LEN];
	char	module[TRACE_NAME_LEN];
	char	function[TRACE_NAME_LEN];
	char	name[TRACE_NAME_LEN];
	trace_arg_desc_t args[TRACE_MAX_ARGS];
} trace_probe_info_t;

typedef struct trace_session_stats {
	u64	records_written;
	u64	records_read;
	u64	records_lost;
	u64	aggregation_count;
	u32	active;
	u32	program_count;
	u32	flags;
	u32	reserved;
} trace_session_stats_t;

typedef struct trace_stats {
	u64	records_written;
	u64	records_lost;
	u64	probe_hits[TRACE_MAX_PROBES];
	u64	action_hits;
	u64	aggregation_updates;
	u32	provider_count;
	u32	probe_count;
	u32	session_count;
	u32	ring_records;
	u32	enabled;
	u32	initialized;
} trace_stats_t;

void	trace_init(void);
void	trace_cpu_online(void);
int	trace_is_initialized(void);
int	trace_is_enabled(void);
u64	trace_clock_read(void);
int	trace_probe_enabled(u32 probe_id);
void	trace_probe_fire(u32 probe_id, u32 flags, const registers_t *regs,
	    const u64 *args);

void	trace_sample_tick(registers_t *regs);
void	trace_irq_enter(registers_t *regs);
void	trace_irq_exit(registers_t *regs);
void	trace_exception(registers_t *regs);
u64	trace_syscall_enter(registers_t *regs);
void	trace_syscall_exit(registers_t *regs, u64 number, u64 ret,
	    u64 start_tsc);
void	trace_sched_tick(registers_t *regs);
void	trace_sched_switch(struct thread *prev, struct thread *next,
	    u32 reason, registers_t *regs);
u64	trace_thread_runtime_cycles(struct thread *td);
void	trace_kqueue_create(int kq_idx, u32 pid);
void	trace_kqueue_destroy(int kq_idx);
void	trace_knote_ready(s16 filter, u64 ident, s64 data);
void	trace_kevent_wait(int kq_idx, int nchanges, int nevents,
	    s64 timeout_ms);
void	trace_kevent_return(int kq_idx, int count, s64 timeout_ms);
void	trace_event_timer_tick(void);

int	trace_session_open(u32 flags, u32 owner_pid, int privileged);
int	trace_session_close(int session_id);
int	trace_session_start(int session_id);
int	trace_session_stop(int session_id);
int	trace_session_clear(int session_id, u32 flags);
int	trace_session_load(int session_id, const trace_program_spec_t *program);
int	trace_session_read(int session_id, trace_record_t *out,
	    u32 max_records);
int	trace_session_read_aggs(int session_id, trace_aggregation_t *out,
	    u32 max_aggs, int clear);
int	trace_session_stats(int session_id, trace_session_stats_t *stats);

u32	trace_provider_count(void);
u32	trace_probe_count(void);
int	trace_provider_info(u32 index, trace_provider_info_t *info);
int	trace_probe_info(u32 index, trace_probe_info_t *info);
void	trace_get_stats(trace_stats_t *stats);

void	trace_pmu_init(void);
void	trace_pmu_cpu_online(int cpu);
void	trace_pmu_sample(int cpu, u64 *values, u32 max_values);
u32	trace_pmu_counter_count(void);
const char *trace_pmu_counter_name(u32 counter);
int	trace_pmu_counter_active(u32 counter);

#endif
