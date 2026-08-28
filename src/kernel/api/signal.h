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
$define %type process_t as struct with process state
$define %type registers_t as struct with CPU register state

$define %func signal_send as function with args u32, int
$define %func signal_deliver as procedure with args struct process *, registers_t *
$define %func signal_pending as function with args struct process *
$define %func signal_default as function with args int
$define %func signal_fatal_pending as function with args struct process *
$define %func signal_clear_pending as procedure with args struct process *, int

$define %constant SIG_DFL_IGNORE as default signal action: ignore
$define %constant SIG_DFL_TERMINATE as default signal action: terminate
$define %constant SIG_DFL_STOP as default signal action: stop

*/

/* !SPACE!

$space %export signal_send, signal_deliver, signal_pending, signal_default
$space %export signal_fatal_pending, signal_clear_pending
$space %export SIG_DFL_IGNORE, SIG_DFL_TERMINATE, SIG_DFL_STOP

*/

#ifndef KERNEL_API_SIGNAL_H
#define KERNEL_API_SIGNAL_H
#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

struct process;

#define SIG_DFL_IGNORE		0
#define SIG_DFL_TERMINATE	1
#define SIG_DFL_STOP		2
int		signal_send(u32 pid, int sig);
void		signal_deliver(struct process *proc, registers_t *regs);
int		signal_pending(struct process *proc);
int		signal_default(int sig);
int		signal_fatal_pending(struct process *proc);
void		signal_clear_pending(struct process *proc, int sig);
#endif
