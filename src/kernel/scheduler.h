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

$define %type registers_t as struct with CPU register snapshot
$define %type process_t as struct with process control block

$define %func scheduler_init as procedure with args void
$define %func scheduler_cm_update as function with args u32
$define %func scheduler_assign_process as procedure with args process_t *
$define %func scheduler_tick as procedure with args registers_t *
$define %func scheduler_yield as procedure with args registers_t *

*/

/* !SPACE!

$space %export scheduler_init, scheduler_assign_process, scheduler_tick
$space %export scheduler_cm_update, scheduler_yield

*/

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

struct process;
void	scheduler_init(void);
int	scheduler_cm_update(u32 flags);
void	scheduler_assign_process(struct process *proc);
void	scheduler_tick(registers_t *regs);
void	scheduler_yield(registers_t *regs);

#endif
