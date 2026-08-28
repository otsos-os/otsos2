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

*/

/* !SPACE!

$space %export apc_init, apc_queue_kernel, apc_queue_user
$space %export apc_pending, apc_deliver, apc_return
$space %export apc_flush_thread, apc_enter_alertable, apc_leave_alertable
$space %export apc_stats
$space %export api_apc_alert, api_apc_queue

*/

#ifndef KERNEL_APC_H
#define KERNEL_APC_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

struct thread;

#define	APC_KERNEL		0
#define	APC_USER_NORMAL		1
#define	APC_USER_SPECIAL	2
#define	APC_AT_SYSCALL		0x1
#define	APC_AT_ALERTABLE	0x2
#define	APC_AT_USER_RETURN	0x4
#define	APC_MAX_TOTAL		512

typedef void (*apc_kernel_fn)(u64 arg1, u64 arg2, u64 arg3);

void	apc_init(void);
int	apc_queue_kernel(struct thread *td, apc_kernel_fn fn, u64 arg1,
	    u64 arg2, u64 arg3);
int	apc_queue_user(struct thread *td, u64 handler, int special, u64 arg1,
	    u64 arg2, u64 arg3);
int	apc_pending(struct thread *td, int context);
int	apc_deliver(struct thread *td, registers_t *regs, int context);
int	apc_return(struct thread *td, registers_t *regs);
void	apc_flush_thread(struct thread *td);
void	apc_enter_alertable(struct thread *td);
void	apc_leave_alertable(struct thread *td);
void	apc_stats(u64 *queued, u64 *delivered, u64 *dropped);
int	api_apc_alert(u64 spins);
int	api_apc_queue(u32 tid, u64 handler, u64 arg);

#endif
