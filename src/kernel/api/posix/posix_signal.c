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

#include <kernel/api/posix/posix.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

typedef struct {
	u64	sa_handler;
	u64	sa_flags;
	u64	sa_restorer;
	u64	sa_mask;
} posix_sigaction_kernel_t;

s64
posix_rt_sigaction(u64 signum_u, u64 act_u, u64 oldact_u,
    u64 sigsetsize, u64 a5, u64 a6, registers_t *regs)
{
	struct process	*proc;
	int		signum;

	(void)sigsetsize; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	signum = (int)signum_u;

	if (signum < 1 || signum > MAX_POSIX_SIGS) {
		return (-POSIX_EINVAL);
	}

	if (signum == 9 || signum == 15) {
		if (oldact_u && is_user_address((void *)oldact_u, 32)) {
			posix_sigaction_kernel_t	*old;
			old = (posix_sigaction_kernel_t *)oldact_u;
			old->sa_handler = proc->
			    sigaction[signum - 1].handler;
			old->sa_flags = proc->sigaction[signum - 1].flags;
			old->sa_restorer = proc->
			    sigaction[signum - 1].restorer;
			old->sa_mask = proc->sigaction[signum - 1].mask;
		}
		return (0);
	}

	if (oldact_u && is_user_address((void *)oldact_u, 32)) {
		posix_sigaction_kernel_t	*old;
		old = (posix_sigaction_kernel_t *)oldact_u;
		old->sa_handler = proc->
		    sigaction[signum - 1].handler;
		old->sa_flags = proc->sigaction[signum - 1].flags;
		old->sa_restorer = proc->
		    sigaction[signum - 1].restorer;
		old->sa_mask = proc->sigaction[signum - 1].mask;
	}

	if (act_u && is_user_address((void *)act_u, 32)) {
		posix_sigaction_kernel_t	*act;
		act = (posix_sigaction_kernel_t *)act_u;
		proc->sigaction[signum - 1].handler =
		    (u64)act->sa_handler;
		proc->sigaction[signum - 1].flags = act->sa_flags;
		proc->sigaction[signum - 1].restorer =
		    (u64)act->sa_restorer;
		proc->sigaction[signum - 1].mask = act->sa_mask;
	}

	return (0);
}

s64
posix_rt_sigprocmask(u64 how_u, u64 set_u, u64 oldset_u,
    u64 sigsetsize, u64 a5, u64 a6, registers_t *regs)
{
	struct process	*proc;
	int		how;
	u64		*set;
	u64		*oldset;

	(void)sigsetsize; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	how = (int)how_u;
	set = (u64 *)set_u;
	oldset = (u64 *)oldset_u;

	if (oldset && is_user_address(oldset, sizeof(u64))) {
		*oldset = proc->sigmask;
	}

	if (set && is_user_address(set, sizeof(u64))) {
		switch (how) {
		case POSIX_SIG_BLOCK:
			proc->sigmask |= *set;
			break;
		case POSIX_SIG_UNBLOCK:
			proc->sigmask &= ~(*set);
			break;
		case POSIX_SIG_SETMASK:
			proc->sigmask = *set;
			break;
		default:
			return (-POSIX_EINVAL);
		}

		if (proc->sigmask & (1ULL << (9 - 1))) {
			proc->sigmask &= ~(1ULL << (9 - 1));
		}
		if (proc->sigmask & (1ULL << (15 - 1))) {
			proc->sigmask &= ~(1ULL << (15 - 1));
		}
	}

	return (0);
}

s64
posix_rt_sigreturn(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	struct thread	*td;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;

	proc = process_current();
	if (!proc || !regs) {
		return (-POSIX_EFAULT);
	}

	td = proc->cur_thread;
	if (!td) {
		td = proc->main_thread;
	}
	if (!td) {
		return (-POSIX_EFAULT);
	}

	regs->r15 = td->saved_context.r15;
	regs->r14 = td->saved_context.r14;
	regs->r13 = td->saved_context.r13;
	regs->r12 = td->saved_context.r12;
	regs->r11 = td->saved_context.r11;
	regs->r10 = td->saved_context.r10;
	regs->r9 = td->saved_context.r9;
	regs->r8 = td->saved_context.r8;
	regs->rbp = td->saved_context.rbp;
	regs->rdi = td->saved_context.rdi;
	regs->rsi = td->saved_context.rsi;
	regs->rdx = td->saved_context.rdx;
	regs->rcx = td->saved_context.rcx;
	regs->rbx = td->saved_context.rbx;
	regs->rax = td->saved_context.rax;
	regs->rip = td->saved_context.rip;
	regs->cs = td->saved_context.cs;
	regs->rflags = td->saved_context.rflags;
	regs->rsp = td->saved_context.rsp;
	regs->ss = td->saved_context.ss;

	proc->sigmask = td->saved_sigmask;

	return ((s64)regs->rax);
}
