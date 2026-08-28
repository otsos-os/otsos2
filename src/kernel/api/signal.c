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
 * SUBSTITUTE GOODS OR SERVICES; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type int as 32 bit signed
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type process_t as struct with process state and signals
$define %type registers_t as struct with CPU register state

$define %func signal_send as function with args u32, int
$define %func signal_deliver as procedure with args struct process *, registers_t *
$define %func signal_pending as function with args struct process *
$define %func signal_default as function with args int
$define %func signal_fatal_pending as function with args struct process *
$define %func signal_clear_pending as procedure with args struct process *, int

*/

/* !SPACE!

$space %export signal_send, signal_deliver, signal_pending, signal_default
$space %export signal_fatal_pending, signal_clear_pending

*/

#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/signal.h>
#include <kernel/api/posix/posix.h>
int
signal_default(int sig)
{
	switch (sig) {
	case SIGCHLD:
	case SIGCONT:
	case SIGURG:
	case SIGWINCH:
		return (SIG_DFL_IGNORE);
	case SIGSTOP:
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
		return (SIG_DFL_STOP);
	default:
		return (SIG_DFL_TERMINATE);
	}
}

int
signal_pending(struct process *proc)
{
	if (!proc) {
		return (0);
	}
	return ((int)(proc->sigpending & ~proc->sigmask));
}
int
signal_send(u32 pid, int sig)
{
	process_t	*proc;

	if (sig < 1 || sig > MAX_POSIX_SIGS) {
		return (-1);
	}

	proc = process_get(pid);
	if (!proc) {
		return (-1);
	}

	if (sig == SIGKILL || sig == SIGSTOP) {
		proc->sigpending |= (1ULL << (sig - 1));
		{
			thread_t	*td;
			for (td = proc->thread_list; td != NULL; td = td->next) {
				if (td->used && td->state == PROC_STATE_SLEEPING) {
					td->state = PROC_STATE_RUNNABLE;
					td->wait_channel = NULL;
				}
			}
		}
		return (0);
	}

	if (proc->sigmask & (1ULL << (sig - 1))) {
		return (0);
	}

	proc->sigpending |= (1ULL << (sig - 1));

	{
		thread_t	*td;
		for (td = proc->thread_list; td != NULL; td = td->next) {
			if (td->used && td->state == PROC_STATE_SLEEPING) {
				td->state = PROC_STATE_RUNNABLE;
				td->wait_channel = NULL;
			}
		}
	}

	return (0);
}

int
signal_fatal_pending(struct process *proc)
{
	u64	pending, mask;
	int	sig;

	if (!proc || proc->sigpending == 0) {
		return (0);
	}

	if (proc->sigpending & (1ULL << (SIGKILL - 1))) {
		return (SIGKILL);
	}

	pending = proc->sigpending & ~proc->sigmask;
	if (pending == 0) {
		return (0);
	}

	for (sig = 1; sig <= MAX_POSIX_SIGS; sig++) {
		mask = (1ULL << (sig - 1));
		if ((pending & mask) == 0) {
			continue;
		}
		if (proc->personality == PERSONALITY_POSIX &&
		    proc->sigaction[sig - 1].handler != 0) {
			continue;
		}
		if (signal_default(sig) == SIG_DFL_TERMINATE) {
			return (sig);
		}
	}
	return (0);
}

void
signal_clear_pending(struct process *proc, int sig)
{
	if (!proc || sig < 1 || sig > MAX_POSIX_SIGS) {
		return;
	}
	proc->sigpending &= ~(1ULL << (sig - 1));
}

void
signal_deliver(struct process *proc, registers_t *regs)
{
	thread_t	*td;
	u64		pending;
	int		sig;
	u64		mask;

	if (!proc || !regs) {
		return;
	}

	td = proc->cur_thread;
	if (!td) {
		td = proc->main_thread;
	}
	if (!td) {
		return;
	}

	pending = proc->sigpending & ~proc->sigmask;
	if (pending == 0) {
		return;
	}

	for (sig = 1; sig <= MAX_POSIX_SIGS; sig++) {
		mask = (1ULL << (sig - 1));
		if (pending & mask) {
			if (sig == SIGKILL) {
				proc->sigpending &= ~mask;
				process_exit(128 + sig);
				return;
			}

			if (sig == SIGSTOP) {
				proc->sigpending &= ~mask;
				td->state = PROC_STATE_SLEEPING;
				return;
			}

			if (proc->personality == PERSONALITY_POSIX) {
				if (proc->sigaction[sig - 1].handler != 0) {
					thread_save_context(td, regs);
					td->saved_context = td->context;
					td->saved_sigmask = proc->sigmask;
					proc->sigmask |= mask;
					proc->sigmask |= proc->sigaction[sig - 1].mask;
					proc->sigpending &= ~mask;
					regs->rip = proc->sigaction[sig - 1].handler;
					regs->rdi = (u64)sig;
					regs->rsi = 0;
					regs->rdx = 0;
					return;
				}
				goto handle_default;
			}

			handle_default:
			{
				int	dfl = signal_default(sig);

				proc->sigpending &= ~mask;

				if (dfl == SIG_DFL_TERMINATE) {
					process_exit(128 + sig);
					return;
				}
				if (dfl == SIG_DFL_STOP) {
					td->state = PROC_STATE_SLEEPING;
					return;
				}
			}
		}
	}
}
