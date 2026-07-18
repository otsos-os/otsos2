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
#include <kernel/signal.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define POSIX_REG_R8		0
#define POSIX_REG_R9		1
#define POSIX_REG_R10		2
#define POSIX_REG_R11		3
#define POSIX_REG_R12		4
#define POSIX_REG_R13		5
#define POSIX_REG_R14		6
#define POSIX_REG_R15		7
#define POSIX_REG_RDI		8
#define POSIX_REG_RSI		9
#define POSIX_REG_RBP		10
#define POSIX_REG_RBX		11
#define POSIX_REG_RDX		12
#define POSIX_REG_RAX		13
#define POSIX_REG_RCX		14
#define POSIX_REG_RSP		15
#define POSIX_REG_RIP		16
#define POSIX_REG_EFL		17

typedef struct {
	u64	sa_handler;
	u64	sa_flags;
	u64	sa_restorer;
	u64	sa_mask;
} posix_sigaction_kernel_t;

static void
posix_signal_mask_fixup(struct process *proc)
{
	if (!proc) {
		return;
	}
	proc->sigmask &= ~(1ULL << (SIGKILL - 1));
	proc->sigmask &= ~(1ULL << (SIGSTOP - 1));
}

static void
posix_sigreturn_badframe(struct process *proc)
{
	if (proc) {
		posix_cleanup_process(proc);
	}
	process_exit(128 + SIGSEGV);
}

s64
posix_rt_sigaction(u64 signum_u, u64 act_u, u64 oldact_u,
    u64 sigsetsize, u64 a5, u64 a6, registers_t *regs)
{
	posix_sigaction_kernel_t	act;
	posix_sigaction_kernel_t	old;
	struct process	*proc;
	int		signum;

	(void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}
	if (sigsetsize != 8) {
		return (-POSIX_EINVAL);
	}

	signum = (int)signum_u;

	if (signum < 1 || signum > MAX_POSIX_SIGS) {
		return (-POSIX_EINVAL);
	}

	if (oldact_u) {
		if (!is_user_address((void *)oldact_u, sizeof(old)) ||
		    !user_range_fault_in((void *)oldact_u, sizeof(old), 1)) {
			return (-POSIX_EFAULT);
		}
		memset(&old, 0, sizeof(old));
		old.sa_handler = proc->sigaction[signum - 1].handler;
		old.sa_flags = proc->sigaction[signum - 1].flags;
		old.sa_restorer = proc->sigaction[signum - 1].restorer;
		old.sa_mask = proc->sigaction[signum - 1].mask;
		memcpy((void *)oldact_u, &old, sizeof(old));
	}

	if (act_u && (signum == SIGKILL || signum == SIGSTOP)) {
		return (-POSIX_EINVAL);
	}

	if (act_u) {
		if (!is_user_address((void *)act_u, sizeof(act)) ||
		    !user_range_fault_in((void *)act_u, sizeof(act), 0)) {
			return (-POSIX_EFAULT);
		}
		memcpy(&act, (void *)act_u, sizeof(act));
		proc->sigaction[signum - 1].handler =
		    (u64)act.sa_handler;
		proc->sigaction[signum - 1].flags = act.sa_flags;
		proc->sigaction[signum - 1].restorer =
		    (u64)act.sa_restorer;
		proc->sigaction[signum - 1].mask = act.sa_mask;
	}

	return (0);
}

s64
posix_rt_sigprocmask(u64 how_u, u64 set_u, u64 oldset_u,
    u64 sigsetsize, u64 a5, u64 a6, registers_t *regs)
{
	struct process	*proc;
	u64		set_value;
	u64		old_value;
	int		how;

	(void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}
	if (sigsetsize != 8) {
		return (-POSIX_EINVAL);
	}

	how = (int)how_u;

	if (oldset_u) {
		if (!is_user_address((void *)oldset_u, sizeof(old_value)) ||
		    !user_range_fault_in((void *)oldset_u, sizeof(old_value),
		    1)) {
			return (-POSIX_EFAULT);
		}
		old_value = proc->sigmask;
		memcpy((void *)oldset_u, &old_value, sizeof(old_value));
	}

	if (set_u) {
		if (!is_user_address((void *)set_u, sizeof(set_value)) ||
		    !user_range_fault_in((void *)set_u, sizeof(set_value),
		    0)) {
			return (-POSIX_EFAULT);
		}
		memcpy(&set_value, (void *)set_u, sizeof(set_value));
		switch (how) {
		case POSIX_SIG_BLOCK:
			proc->sigmask |= set_value;
			break;
		case POSIX_SIG_UNBLOCK:
			proc->sigmask &= ~set_value;
			break;
		case POSIX_SIG_SETMASK:
			proc->sigmask = set_value;
			break;
		default:
			return (-POSIX_EINVAL);
		}

		posix_signal_mask_fixup(proc);
	}

	return (0);
}

s64
posix_rt_sigreturn(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	posix_rt_sigframe_t	frame;
	struct process	*proc;
	posix_mcontext_t	*mc;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;

	proc = process_current();
	if (!proc || !regs) {
		return (-POSIX_EFAULT);
	}

	if (!is_user_address((void *)regs->rsp, sizeof(frame)) ||
	    !user_range_fault_in((void *)regs->rsp, sizeof(frame), 0)) {
		posix_sigreturn_badframe(proc);
		return (-POSIX_EFAULT);
	}
	memcpy(&frame, (void *)regs->rsp, sizeof(frame));
	mc = &frame.uc.mcontext;

	if (!is_user_address((void *)mc->gregs[POSIX_REG_RIP], 1) ||
	    !is_user_address((void *)mc->gregs[POSIX_REG_RSP], 1)) {
		posix_sigreturn_badframe(proc);
		return (-POSIX_EFAULT);
	}

	regs->r8 = mc->gregs[POSIX_REG_R8];
	regs->r9 = mc->gregs[POSIX_REG_R9];
	regs->r10 = mc->gregs[POSIX_REG_R10];
	regs->r11 = mc->gregs[POSIX_REG_R11];
	regs->r12 = mc->gregs[POSIX_REG_R12];
	regs->r13 = mc->gregs[POSIX_REG_R13];
	regs->r14 = mc->gregs[POSIX_REG_R14];
	regs->r15 = mc->gregs[POSIX_REG_R15];
	regs->rdi = mc->gregs[POSIX_REG_RDI];
	regs->rsi = mc->gregs[POSIX_REG_RSI];
	regs->rbp = mc->gregs[POSIX_REG_RBP];
	regs->rbx = mc->gregs[POSIX_REG_RBX];
	regs->rdx = mc->gregs[POSIX_REG_RDX];
	regs->rax = mc->gregs[POSIX_REG_RAX];
	regs->rcx = mc->gregs[POSIX_REG_RCX];
	regs->rsp = mc->gregs[POSIX_REG_RSP];
	regs->rip = mc->gregs[POSIX_REG_RIP];
	regs->rflags = mc->gregs[POSIX_REG_EFL];

	proc->sigmask = frame.uc.sigmask[0];
	posix_signal_mask_fixup(proc);

	return ((s64)regs->rax);
}
