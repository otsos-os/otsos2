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
 * and/or other materials provided with the distribution.
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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type spinlock_t as big kernel lock type
$define %type smp_cpu_t as per CPU descriptor
$define %type thread_t as struct with per-thread CPU context and state
$define %type tss_t as struct with task state segment

$define %func smp_init as procedure with args void
$define %func smp_lock as procedure with args void
$define %func smp_unlock as procedure with args void
$define %func smp_lock_held as function with args void
$define %func smp_cpu_id as function with args void
$define %func smp_cpu_index as function with args void
$define %func smp_cpu_count as function with args void
$define %func smp_tss_current as function with args void
$define %func smp_tss_register as procedure with args u8, tss_t *
$define %func smp_current_thread as function with args void
$define %func smp_set_current_thread as procedure with args thread_t *
$define %func smp_ap_started as function with args u8
$define %func smp_ap_target_index as function with args void
$define %func ap_main as start with args u8

$define %const SMP_MAX_CPUS as 32

*/

/* !SPACE!

$space %internal smp_init_bsp, smp_start_ap
$space %export smp_init, smp_lock, smp_unlock, smp_lock_held
$space %export smp_cpu_id, smp_cpu_index, smp_cpu_count
$space %export smp_tss_current, smp_tss_register
$space %export smp_current_thread, smp_set_current_thread
$space %export smp_ap_started, smp_ap_target_index
$space %export ap_main
$space %export smp_tss_by_lapic, smp_cpu_map
$space %export smp_bsp_lapic_id, smp_ap_cpu_index

*/

#ifndef KERNEL_SMP_SMP_H
#define KERNEL_SMP_SMP_H
#include <kernel/gdt.h>
#include <kernel/thread.h>
#include <mlibc/mlibc.h>
#define SMP_MAX_CPUS 32

struct spinlock {
	u32	locked;
	u32	owner;
};

typedef struct spinlock	spinlock_t;

struct smp_cpu {
	u8	lapic_id;
	u8	present;
	u8	cpu_index;
	thread_t	*current_thread;
	tss_t	*tss;
};

void	smp_init(void);
void	smp_lock(void);
void	smp_unlock(void);
int	smp_lock_held(void);
u8	smp_cpu_id(void);
int	smp_cpu_index(void);
int	smp_cpu_count(void);
tss_t	*smp_tss_current(void);
void	smp_tss_register(u8 lapic_id, tss_t *tss);
thread_t *smp_current_thread(void);
void	smp_set_current_thread(thread_t *td);
int	smp_ap_started(u8 cpu_index);
u8	smp_ap_target_index(void);
void	ap_main(u8 cpu_index);
extern tss_t	*smp_tss_by_lapic[256];
extern struct smp_cpu	smp_cpu_map[SMP_MAX_CPUS];
extern u8	smp_bsp_lapic_id;
extern u8	smp_ap_cpu_index;

#endif
