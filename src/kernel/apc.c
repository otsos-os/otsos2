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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type apc_kernel_fn as pointer to kernel APC routine
$define %type thread_t as struct with per-thread CPU context and APC queue
$define %type registers_t as struct with CPU register snapshot from interrupt

$define %func apc_init as procedure with args void
$define %func apc_alloc as function with args void
$define %func apc_free as procedure with args u32
$define %func apc_append as function with args thread_t *, u32
$define %func apc_deliverable as function with args int, int, int
$define %func apc_wake as procedure with args thread_t *
$define %func apc_queue_kernel as function with args thread_t *, apc_kernel_fn, u64, u64, u64
$define %func apc_queue_user as function with args thread_t *, u64, int, u64, u64, u64
$define %func apc_pending as function with args thread_t *, int
$define %func apc_deliver as function with args thread_t *, registers_t *, int
$define %func apc_return as function with args thread_t *, registers_t *
$define %func apc_flush_thread as procedure with args thread_t *
$define %func apc_enter_alertable as procedure with args thread_t *
$define %func apc_leave_alertable as procedure with args thread_t *
$define %func apc_stats as procedure with args u64 *, u64 *, u64 *
$define %func api_apc_alert as function with args u64
$define %func api_apc_queue as function with args u32, u64, u64
$define %const APC_ALERT_MAX_SPINS as ceiling on an alertable wait

*/

/* !SPACE!

$space %internal apc_alloc, apc_free, apc_append, apc_deliverable, apc_wake
$space %export apc_init, apc_queue_kernel, apc_queue_user
$space %export apc_pending, apc_deliver, apc_return
$space %export apc_flush_thread, apc_enter_alertable, apc_leave_alertable
$space %export apc_stats
$space %export api_apc_alert, api_apc_queue

*/

#include <kernel/apc.h>
#include <kernel/api/errno.h>
#include <kernel/api/signal.h>
#include <kernel/process.h>
#include <kernel/smp/smp.h>
#include <kernel/sync/sync.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	APC_SLOT_NONE		0xFFFFFFFFU
#define	APC_ALERT_MAX_SPINS	1000000ULL
#define	APC_RED_ZONE		128

static u32	apc_pool_used[APC_MAX_TOTAL];
static u32	apc_pool_type[APC_MAX_TOTAL];
static u64	apc_pool_handler[APC_MAX_TOTAL];
static u64	apc_pool_arg1[APC_MAX_TOTAL];
static u64	apc_pool_arg2[APC_MAX_TOTAL];
static u64	apc_pool_arg3[APC_MAX_TOTAL];
static u32	apc_pool_next[APC_MAX_TOTAL];
static u32	apc_free_head;
static u64	apc_queued_total;
static u64	apc_delivered_total;
static u64	apc_dropped_total;
static int	apc_initialized;
static spin_t	apc_spin = SPIN_INITIALIZER("apc", LO_APC);

void
apc_init(void)
{
	u32	i;

	if (apc_initialized) {
		return;
	}
	memset(apc_pool_used, 0, sizeof(apc_pool_used));
	memset(apc_pool_type, 0, sizeof(apc_pool_type));
	memset(apc_pool_handler, 0, sizeof(apc_pool_handler));
	memset(apc_pool_arg1, 0, sizeof(apc_pool_arg1));
	memset(apc_pool_arg2, 0, sizeof(apc_pool_arg2));
	memset(apc_pool_arg3, 0, sizeof(apc_pool_arg3));
	apc_free_head = 0;
	for (i = 0; i < APC_MAX_TOTAL; i++) {
		apc_pool_next[i] = i + 1;
	}
	apc_pool_next[APC_MAX_TOTAL - 1] = APC_SLOT_NONE;
	apc_queued_total = 0;
	apc_delivered_total = 0;
	apc_dropped_total = 0;
	apc_initialized = 1;
	printk("[APC] initialized: %d pool entries, %d per thread\n",
	    APC_MAX_TOTAL, THREAD_MAX_APCS);
}

static u32
apc_alloc(void)
{
	u32	slot;

	if (apc_free_head == APC_SLOT_NONE) {
		return (APC_SLOT_NONE);
	}
	slot = apc_free_head;
	apc_free_head = apc_pool_next[slot];
	apc_pool_next[slot] = APC_SLOT_NONE;
	apc_pool_used[slot] = 1;
	return (slot);
}

static void
apc_free(u32 slot)
{
	if (slot >= APC_MAX_TOTAL || !apc_pool_used[slot]) {
		return;
	}
	apc_pool_used[slot] = 0;
	apc_pool_type[slot] = 0;
	apc_pool_handler[slot] = 0;
	apc_pool_arg1[slot] = 0;
	apc_pool_arg2[slot] = 0;
	apc_pool_arg3[slot] = 0;
	apc_pool_next[slot] = apc_free_head;
	apc_free_head = slot;
}

static int
apc_append(thread_t *td, u32 slot)
{
	u32	cur;

	if (td->apc_count >= THREAD_MAX_APCS) {
		return (-API_ERR_RETRY);
	}
	apc_pool_next[slot] = APC_SLOT_NONE;
	if (td->apc_head == (int)APC_SLOT_NONE || td->apc_head < 0) {
		td->apc_head = (int)slot;
	} else {
		cur = (u32)td->apc_head;
		while (apc_pool_next[cur] != APC_SLOT_NONE) {
			cur = apc_pool_next[cur];
		}
		apc_pool_next[cur] = slot;
	}
	td->apc_count++;
	return (0);
}

static int
apc_deliverable(int type, int context, int in_user_apc)
{
	switch (type) {
	case APC_KERNEL:
		return (1);
	case APC_USER_NORMAL:
		if (in_user_apc) {
			return (0);
		}
		return ((context & APC_AT_ALERTABLE) != 0);
	case APC_USER_SPECIAL:
		if (in_user_apc) {
			return (0);
		}
		return ((context & (APC_AT_SYSCALL | APC_AT_ALERTABLE |
		    APC_AT_USER_RETURN)) != 0);
	default:
		return (0);
	}
}


static void
apc_wake(thread_t *td)
{
	if (!td->apc_alertable) {
		return;
	}
	thread_lock();
	if (thread_state_get(td) == PROC_STATE_SLEEPING) {
		thread_state_set(td, PROC_STATE_RUNNABLE);
		__atomic_store_n(&td->wait_channel, NULL, __ATOMIC_RELEASE);
	}
	thread_unlock();
}

int
apc_queue_kernel(thread_t *td, apc_kernel_fn fn, u64 arg1, u64 arg2,
    u64 arg3)
{
	u32	slot;
	int	err;

	if (!td || !td->used || fn == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&apc_spin);
	if (thread_state_get(td) == PROC_STATE_TERMINATED) {
		spin_unlock(&apc_spin);
		return (-API_ERR_NO_PROC);
	}
	slot = apc_alloc();
	if (slot == APC_SLOT_NONE) {
		apc_dropped_total++;
		spin_unlock(&apc_spin);
		printk("[APC] pool exhausted, kernel APC dropped\n");
		return (-API_ERR_RETRY);
	}
	apc_pool_type[slot] = APC_KERNEL;
	apc_pool_handler[slot] = (u64)fn;
	apc_pool_arg1[slot] = arg1;
	apc_pool_arg2[slot] = arg2;
	apc_pool_arg3[slot] = arg3;
	err = apc_append(td, slot);
	if (err != 0) {
		apc_free(slot);
		apc_dropped_total++;
		spin_unlock(&apc_spin);
		return (err);
	}
	apc_queued_total++;
	apc_wake(td);
	spin_unlock(&apc_spin);
	return (0);
}

int
apc_queue_user(thread_t *td, u64 handler, int special, u64 arg1, u64 arg2,
    u64 arg3)
{
	u32	slot;
	int	err;

	if (!td || !td->used || handler == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!is_user_address((const void *)handler, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	spin_lock(&apc_spin);
	if (thread_state_get(td) == PROC_STATE_TERMINATED) {
		spin_unlock(&apc_spin);
		return (-API_ERR_NO_PROC);
	}
	slot = apc_alloc();
	if (slot == APC_SLOT_NONE) {
		apc_dropped_total++;
		spin_unlock(&apc_spin);
		printk("[APC] pool exhausted, user APC dropped\n");
		return (-API_ERR_RETRY);
	}
	apc_pool_type[slot] = special ? APC_USER_SPECIAL : APC_USER_NORMAL;
	apc_pool_handler[slot] = handler;
	apc_pool_arg1[slot] = arg1;
	apc_pool_arg2[slot] = arg2;
	apc_pool_arg3[slot] = arg3;
	err = apc_append(td, slot);
	if (err != 0) {
		apc_free(slot);
		apc_dropped_total++;
		spin_unlock(&apc_spin);
		return (err);
	}
	apc_queued_total++;
	apc_wake(td);
	spin_unlock(&apc_spin);
	return (0);
}

int
apc_pending(thread_t *td, int context)
{
	u32	cur;
	int	found;

	if (!td || !td->used) {
		return (0);
	}
	if (td->apc_head < 0) {
		return (0);
	}
	found = 0;
	spin_lock(&apc_spin);
	cur = (td->apc_head < 0) ? APC_SLOT_NONE : (u32)td->apc_head;
	while (cur != APC_SLOT_NONE) {
		if (apc_deliverable((int)apc_pool_type[cur], context,
		    td->apc_in_user)) {
			found = 1;
			break;
		}
		cur = apc_pool_next[cur];
	}
	spin_unlock(&apc_spin);
	return (found);
}

void
apc_enter_alertable(thread_t *td)
{
	if (!td) {
		return;
	}
	spin_lock(&apc_spin);
	td->apc_alertable++;
	spin_unlock(&apc_spin);
}

void
apc_leave_alertable(thread_t *td)
{
	if (!td) {
		return;
	}
	spin_lock(&apc_spin);
	if (td->apc_alertable > 0) {
		td->apc_alertable--;
	}
	spin_unlock(&apc_spin);
}

void
apc_flush_thread(thread_t *td)
{
	u32	cur, next;

	if (!td) {
		return;
	}
	spin_lock(&apc_spin);
	cur = (td->apc_head < 0) ? APC_SLOT_NONE : (u32)td->apc_head;
	while (cur != APC_SLOT_NONE) {
		next = apc_pool_next[cur];
		apc_free(cur);
		cur = next;
	}
	td->apc_head = -1;
	td->apc_count = 0;
	td->apc_alertable = 0;
	td->apc_in_user = 0;
	spin_unlock(&apc_spin);
}

static u32
apc_pop(thread_t *td, int context)
{
	u32	cur, prev;

	prev = APC_SLOT_NONE;
	cur = (td->apc_head < 0) ? APC_SLOT_NONE : (u32)td->apc_head;
	while (cur != APC_SLOT_NONE) {
		if (apc_deliverable((int)apc_pool_type[cur], context,
		    td->apc_in_user)) {
			if (prev == APC_SLOT_NONE) {
				td->apc_head = (apc_pool_next[cur] ==
				    APC_SLOT_NONE) ? -1 :
				    (int)apc_pool_next[cur];
			} else {
				apc_pool_next[prev] = apc_pool_next[cur];
			}
			td->apc_count--;
			if (td->apc_count < 0) {
				td->apc_count = 0;
			}
			return (cur);
		}
		prev = cur;
		cur = apc_pool_next[cur];
	}
	return (APC_SLOT_NONE);
}

int
apc_deliver(thread_t *td, registers_t *regs, int context)
{
	apc_kernel_fn	fn;
	u64		handler, arg1, arg2, arg3, rsp;
	u32		slot;
	int		type, delivered, have_user;

	if (!td || !td->used || !regs) {
		return (0);
	}
	if (td->apc_head < 0) {
		return (0);
	}

	delivered = 0;
	have_user = 0;
	handler = 0;
	arg1 = 0;
	arg2 = 0;
	arg3 = 0;
	for (;;) {
		spin_lock(&apc_spin);
		slot = apc_pop(td, context);
		if (slot == APC_SLOT_NONE) {
			spin_unlock(&apc_spin);
			break;
		}
		type = (int)apc_pool_type[slot];
		handler = apc_pool_handler[slot];
		arg1 = apc_pool_arg1[slot];
		arg2 = apc_pool_arg2[slot];
		arg3 = apc_pool_arg3[slot];
		if (type != APC_KERNEL) {
			td->apc_in_user = 1;
			apc_free(slot);
			apc_delivered_total++;
			spin_unlock(&apc_spin);
			have_user = 1;
			break;
		}
		apc_free(slot);
		apc_delivered_total++;
		spin_unlock(&apc_spin);
		fn = (apc_kernel_fn)handler;
		fn(arg1, arg2, arg3);
		delivered++;
	}

	if (!have_user) {
		return (delivered);
	}

	if ((regs->cs & 3) != 3) {
		spin_lock(&apc_spin);
		td->apc_in_user = 0;
		apc_dropped_total++;
		spin_unlock(&apc_spin);
		printk("[APC] refused user APC over a kernel frame "
		    "(tid=%u context=0x%x)\n", td->tid, (unsigned)context);
		return (delivered);
	}

	rsp = regs->rsp;
	if (rsp <= APC_RED_ZONE) {
		spin_lock(&apc_spin);
		td->apc_in_user = 0;
		apc_dropped_total++;
		spin_unlock(&apc_spin);
		return (delivered);
	}
	rsp -= APC_RED_ZONE;
	rsp &= ~0xFULL;
	rsp -= 8;
	if (!is_user_address((const void *)rsp, 8)) {
		spin_lock(&apc_spin);
		td->apc_in_user = 0;
		apc_dropped_total++;
		spin_unlock(&apc_spin);
		printk("[APC] user stack unusable, APC dropped (tid=%u)\n",
		    td->tid);
		return (delivered);
	}

	thread_save_context(td, regs);
	td->apc_saved_context = td->context;
	memcpy(&td->apc_saved_fpu, &td->fpu_context,
	    sizeof(td->apc_saved_fpu));

	regs->rip = handler;
	regs->rsp = rsp;
	regs->rdi = arg1;
	regs->rsi = arg2;
	regs->rdx = arg3;
	delivered++;
	return (delivered);
}

int
apc_return(thread_t *td, registers_t *regs)
{
	if (!td || !regs) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!td->apc_in_user) {
		return (-API_ERR_BAD_VALUE);
	}

	if (!is_user_address((const void *)td->apc_saved_context.rip, 1) ||
	    !is_user_address((const void *)td->apc_saved_context.rsp, 1)) {
		printk("[APC] saved frame invalid on return (tid=%u), "
		    "killing process\n", td->tid);
		td->apc_in_user = 0;
		process_exit(-1);
		return (-API_ERR_BAD_ADDR);
	}

	regs->r15 = td->apc_saved_context.r15;
	regs->r14 = td->apc_saved_context.r14;
	regs->r13 = td->apc_saved_context.r13;
	regs->r12 = td->apc_saved_context.r12;
	regs->r11 = td->apc_saved_context.r11;
	regs->r10 = td->apc_saved_context.r10;
	regs->r9 = td->apc_saved_context.r9;
	regs->r8 = td->apc_saved_context.r8;
	regs->rbp = td->apc_saved_context.rbp;
	regs->rdi = td->apc_saved_context.rdi;
	regs->rsi = td->apc_saved_context.rsi;
	regs->rdx = td->apc_saved_context.rdx;
	regs->rcx = td->apc_saved_context.rcx;
	regs->rbx = td->apc_saved_context.rbx;
	regs->rax = td->apc_saved_context.rax;
	regs->rip = td->apc_saved_context.rip;
	regs->cs = td->apc_saved_context.cs;
	regs->rflags = td->apc_saved_context.rflags;
	regs->rsp = td->apc_saved_context.rsp;
	regs->ss = td->apc_saved_context.ss;
	memcpy(&td->fpu_context, &td->apc_saved_fpu,
	    sizeof(td->fpu_context));
	td->fpu_valid = 1;
	thread_load_fpu_context(td);

	spin_lock(&apc_spin);
	td->apc_in_user = 0;
	spin_unlock(&apc_spin);
	return (0);
}

void
apc_stats(u64 *queued, u64 *delivered, u64 *dropped)
{
	spin_lock(&apc_spin);
	if (queued) {
		*queued = apc_queued_total;
	}
	if (delivered) {
		*delivered = apc_delivered_total;
	}
	if (dropped) {
		*dropped = apc_dropped_total;
	}
	spin_unlock(&apc_spin);
}

int
api_apc_alert(u64 spins)
{
	process_t	*proc;
	thread_t	*td;
	u64		pass;

	td = thread_current();
	proc = process_current();
	if (!td || !proc) {
		return (-API_ERR_BAD_VALUE);
	}
	if (spins == 0 || spins > APC_ALERT_MAX_SPINS) {
		spins = APC_ALERT_MAX_SPINS;
	}

	apc_enter_alertable(td);
	for (pass = 0; pass < spins; pass++) {
		if (apc_pending(td, APC_AT_ALERTABLE)) {
			apc_leave_alertable(td);
			return (0);
		}
		if (signal_pending(proc)) {
			apc_leave_alertable(td);
			return (-API_ERR_INTR);
		}
		proc_sleep((void *)td);
	}
	apc_leave_alertable(td);
	return (-API_ERR_TIMED_OUT);
}

int
api_apc_queue(u32 tid, u64 handler, u64 arg)
{
	process_t	*proc;
	thread_t	*td;

	proc = process_current();
	if (!proc) {
		return (-API_ERR_BAD_VALUE);
	}
	if (handler == 0 || !is_user_address((const void *)handler, 1)) {
		return (-API_ERR_BAD_ADDR);
	}

	td = (tid == 0) ? thread_current() : thread_get(tid);
	if (!td || td->proc != proc) {
		return (-API_ERR_NO_PROC);
	}
	return (apc_queue_user(td, handler, 0, arg, 0, 0));
}
