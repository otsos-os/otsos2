/* !DEFINES!

$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type registers_t as struct with CPU register snapshot
$define %type trace_field_desc_t as struct with trace field metadata
$define %type trace_event_desc_t as struct with trace event metadata
$define %type trace_record_t as struct with one binary trace record
$define %type trace_session_filter_t as struct with session predicates
$define %type trace_session_stats_t as struct with session counters
$define %type trace_stats_t as struct with global trace counters

$define %func trace_init as procedure with args void
$define %func trace_cpu_online as procedure with args void
$define %func trace_is_initialized as function with args void
$define %func trace_is_enabled as function with args void
$define %func trace_clock_read as function with args void
$define %func trace_register_event as function with args event desc
$define %func trace_event_desc as function with args u16
$define %func trace_event_enabled as function with args u16
$define %func trace_enable_source as function with args u16, int
$define %func trace_source_name as function with args u16
$define %func trace_source_enabled as function with args u16
$define %func trace_emit as procedure with args event, flags, regs, args
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
#define	TRACE_MAX_EVENTS		128
#define	TRACE_EVENT_WORDS		2
#define	TRACE_MAX_SESSIONS		16
#define	TRACE_RECORD_ARGS		6
#define	TRACE_RECORD_STACK		8
#define	TRACE_NAME_LEN			32
#define	TRACE_PROVIDER_LEN		16
#define	TRACE_MIN_RING_RECORDS		128
#define	TRACE_DEFAULT_RING_RECORDS	1024
#define	TRACE_MAX_RING_RECORDS		16384

#define	TRACE_SOURCE_MASK_ALL		((1ULL << TRACE_SOURCE_COUNT) - 1)

#define	TRACE_FILTER_HAS_PID		0x00000001
#define	TRACE_FILTER_HAS_TID		0x00000002
#define	TRACE_FILTER_HAS_CPU		0x00000004

#define	TRACE_REC_F_USER		0x00000001
#define	TRACE_REC_F_LOST_BEFORE		0x00000002
#define	TRACE_REC_F_PMU_VALID		0x00000004

struct thread;

enum trace_source_id {
	TRACE_SOURCE_MAIN = 0,
	TRACE_SOURCE_PMU,
	TRACE_SOURCE_SYSCALL,
	TRACE_SOURCE_HANDLER,
	TRACE_SOURCE_SCHEDULER,
	TRACE_SOURCE_EVENT,
	TRACE_SOURCE_USER,
	TRACE_SOURCE_COUNT
};

enum trace_event_id {
	TRACE_EV_CORE_BOOT = 0,
	TRACE_EV_PROFILE_SAMPLE,
	TRACE_EV_PMU_COUNTERS,
	TRACE_EV_SYSCALL_ENTER,
	TRACE_EV_SYSCALL_EXIT,
	TRACE_EV_IRQ_ENTER,
	TRACE_EV_IRQ_EXIT,
	TRACE_EV_EXCEPTION,
	TRACE_EV_SCHED_TICK,
	TRACE_EV_SCHED_SWITCH,
	TRACE_EV_EVENT_KQUEUE_CREATE,
	TRACE_EV_EVENT_KQUEUE_DESTROY,
	TRACE_EV_EVENT_KNOTE_READY,
	TRACE_EV_EVENT_KEVENT_WAIT,
	TRACE_EV_EVENT_KEVENT_RETURN,
	TRACE_EV_EVENT_TIMER_TICK,
	TRACE_EV_USER_MARK,
	TRACE_EV_COUNT
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

typedef struct trace_field_desc {
	const char	*name;
	u16		index;
	u16		flags;
} trace_field_desc_t;

typedef struct trace_event_desc {
	u16			 id;
	u16			 source;
	u32			 flags;
	const char		*provider;
	const char		*name;
	const trace_field_desc_t *fields;
	u16			 field_count;
} trace_event_desc_t;

typedef struct trace_record {
	u64	seq;
	u64	tsc;
	u64	ticks;
	u64	pid;
	u64	tid;
	u64	ip;
	u64	sp;
	u64	bp;
	u64	args[TRACE_RECORD_ARGS];
	u64	stack[TRACE_RECORD_STACK];
	u32	cpu;
	u32	event;
	u32	source;
	u32	flags;
	u16	stack_count;
	u16	reserved;
} trace_record_t;

typedef struct trace_session_filter {
	u64	source_mask;
	u64	event_mask[TRACE_EVENT_WORDS];
	int	pid;
	int	tid;
	int	cpu;
	u32	flags;
} trace_session_filter_t;

typedef struct trace_session_stats {
	u64	read_records;
	u64	lost_records;
	u32	active;
	u32	flags;
} trace_session_stats_t;

typedef struct trace_stats {
	u64	records_written;
	u64	records_lost;
	u64	event_count[TRACE_MAX_EVENTS];
	u64	source_count[TRACE_SOURCE_COUNT];
	u32	ring_records;
	u32	session_count;
	u32	enabled;
	u32	initialized;
} trace_stats_t;

void	trace_init(void);
void	trace_cpu_online(void);
int	trace_is_initialized(void);
int	trace_is_enabled(void);
u64	trace_clock_read(void);
int	trace_register_event(const trace_event_desc_t *desc);
const trace_event_desc_t *trace_event_desc(u16 event);
int	trace_event_enabled(u16 event);
int	trace_enable_source(u16 source, int enabled);
const char *trace_source_name(u16 source);
int	trace_source_enabled(u16 source);
void	trace_emit(u16 event, u32 flags, const registers_t *regs,
	    u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4,
	    u64 arg5);
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
int	trace_session_open(u32 flags);
int	trace_session_close(int session_id);
int	trace_session_start(int session_id);
int	trace_session_stop(int session_id);
int	trace_session_flush(int session_id);
int	trace_session_filter(int session_id,
	    const trace_session_filter_t *filter);
int	trace_session_enable_event(int session_id, u16 event, int enabled);
int	trace_session_enable_source(int session_id, u16 source, int enabled);
int	trace_session_read(int session_id, trace_record_t *out,
	    u32 max_records);
int	trace_session_stats(int session_id, trace_session_stats_t *stats);
void	trace_get_stats(trace_stats_t *stats);

void	trace_pmu_init(void);
void	trace_pmu_cpu_online(int cpu);
void	trace_pmu_sample(int cpu, u64 *values, u32 max_values);
u32	trace_pmu_counter_count(void);
const char *trace_pmu_counter_name(u32 counter);
int	trace_pmu_counter_active(u32 counter);

#endif
