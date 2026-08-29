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
$define %type acpi_madt_local_apic_t as packed struct with local APIC entry
$define %type acpi_madt_entry_header_t as packed struct with entry type and length
$define %type smp_cpu_t as per CPU descriptor
$define %type tss_t as struct with task state segment
$define %type thread_t as struct with per-thread CPU context and state

$define %func smp_init as procedure with args void
$define %func smp_init_single_cpu as procedure with args void
$define %func smp_init_bsp as procedure with args void
$define %func smp_start_ap as procedure with args u8, u8
$define %func smp_lock as procedure with args void
$define %func smp_unlock as procedure with args void
$define %func smp_lock_held as function with args void
$define %func smp_lock_release_all as function with args void
$define %func smp_lock_acquire_depth as procedure with args u32
$define %func smp_cpu_id as function with args void
$define %func smp_cpu_index as function with args void
$define %func smp_cpu_count_var as function with args void
$define %func smp_tss_current as function with args void
$define %func smp_tss_register as procedure with args u8, tss_t *
$define %func smp_current_thread as function with args void
$define %func smp_set_current_thread as procedure with args thread_t *
$define %func smp_ap_started as function with args u8
$define %func smp_ap_target_index as function with args void
$define %func smp_is_bsp as function with args void
$define %func smp_sched_cpu_count as function with args void
$define %func smp_ap_scheduling_enabled as function with args void
$define %func smp_cpu_index_from_lapic as function with args u8
$define %func smp_ap_count_cb as procedure with args acpi_madt_entry_header_t *, void *
$define %func smp_ap_list_cb as procedure with args acpi_madt_entry_header_t *, void *
$define %func ap_main as start with args u8
$define %func smp_send_init as procedure with args u8
$define %func smp_send_sipi as procedure with args u8, u8
$define %func smp_wait_us as procedure with args u32
$define %func smp_wait_ticks as procedure with args u32
$define %func smp_trampoline_setup as procedure with args u64, u64, u8
$define %func smp_ap_ready_clear as procedure with args void
$define %func smp_ap_park as start with args void

*/

/* !SPACE!

$space %internal smp_init_bsp, smp_start_ap, smp_cpu_index_from_lapic
$space %internal smp_ap_count_cb, smp_ap_list_cb
$space %internal smp_send_init, smp_send_sipi, smp_wait_us, smp_wait_ticks
$space %internal smp_trampoline_setup, smp_ap_ready_clear, smp_ap_park
$space %export smp_init, smp_init_single_cpu, smp_lock, smp_unlock, smp_lock_held
$space %export smp_lock_release_all, smp_lock_acquire_depth
$space %export smp_cpu_id, smp_cpu_index, smp_cpu_count_var
$space %export smp_tss_current, smp_tss_register
$space %export smp_current_thread, smp_set_current_thread
$space %export smp_ap_started, smp_ap_target_index
$space %export smp_is_bsp, smp_sched_cpu_count
$space %export smp_ap_scheduling_enabled
$space %export ap_main

*/

#include <kernel/cm/cm.h>
#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/timer.h>
#include <kernel/gdt.h>
#include <kernel/interrupts/apic/lapic.h>
#include <kernel/interrupts/idt.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/smp/pcpu.h>
#include <kernel/smp/smp.h>
#include <kernel/syscall.h>
#include <kernel/trace/trace.h>
#include <mlibc/stdio.h>
/* bkl - big kernel lock*/
static		spinlock_t	smp_bkl;
static		u64		smp_bkl_flags[256];
struct smp_cpu	smp_cpu_map[SMP_MAX_CPUS];
tss_t		*smp_tss_by_lapic[256];
u8		smp_bsp_lapic_id;
u8		smp_ap_cpu_index;
static volatile u8	smp_ap_ready[SMP_MAX_CPUS];
static int	smp_cpu_count_var;
static int	smp_initialized;
static int	smp_bsp_known;
static int	smp_ap_sched;		/* SYSTEM.Scheduler.ApScheduling */
extern char	ap_trampoline_start[];
extern char	ap_trampoline_end[];
extern void	pit_delay_us(u32 us);
extern int	pit_delay_us_checked(u32 us);
#define	TRAMPOLINE_BASE	0x8000
#define	TRAMPOLINE_VADDR	TRAMPOLINE_BASE
#define	OFF_CR3		0x100
#define	OFF_STACK	0x108
#define	OFF_CPU_INDEX	0x110
#define	OFF_ENTRY	0x118
#define	OFF_GDTR64	0x120
#define	OFF_GDTR32	0x130
#define	OFF_GDT32	0x140
#define	OFF_GDT64	0x180

static inline u8
lapic_id_from_entry(acpi_madt_local_apic_t *lapic)
{
	return (lapic->apic_id);
}

static void
smp_ap_ready_clear(void)
{
	int	i;

	for (i = 0; i < SMP_MAX_CPUS; i++) {
		smp_ap_ready[i] = 0;
	}
}

static int
smp_cpu_index_from_lapic(u8 lapic_id)
{
	int	i;
	for (i = 0; i < SMP_MAX_CPUS; i++) {
		if (smp_cpu_map[i].present &&
		    smp_cpu_map[i].lapic_id == lapic_id) {
			return (i);
		}
	}
	return (-1);
}

u8
smp_cpu_id(void)
{
	if (!lapic_is_enabled()) {
		return (smp_bsp_lapic_id);
	}
	return (lapic_get_id());
}

int
smp_is_bsp(void)
{
	if (!smp_bsp_known) {
		return (1);
	}
	return (smp_cpu_id() == smp_bsp_lapic_id);
}

int
smp_ap_scheduling_enabled(void)
{
	return (smp_ap_sched);
}

int
smp_sched_cpu_count(void)
{
	int	count, i;

	if (!smp_ap_sched) {
		return (1);
	}
	count = 0;
	for (i = 0; i < SMP_MAX_CPUS && i < smp_cpu_count_var; i++) {
		if (smp_cpu_map[i].present && smp_cpu_map[i].online) {
			count++;
		}
	}
	if (count < 1) {
		count = 1;
	}
	return (count);
}

int
smp_cpu_index(void)
{
	int	idx;

	if (pcpu_is_ready()) {
		return ((int)pcpu_id());
	}
	idx = smp_cpu_index_from_lapic(smp_cpu_id());
	if (idx < 0) {
		return (0);
	}
	return (idx);
}

int
smp_cpu_count(void)
{
	return (smp_cpu_count_var);
}

tss_t *
smp_tss_current(void)
{
	tss_t	*tss;
	int	idx;

	idx = smp_cpu_index();
	if (idx >= 0 && idx < SMP_MAX_CPUS && smp_cpu_map[idx].tss) {
		return (smp_cpu_map[idx].tss);
	}
	tss = smp_tss_by_lapic[smp_cpu_id()];
	if (tss == NULL) {
		panic("[SMP] no TSS for current CPU\n");
	}
	return (tss);
}

void
smp_tss_register(u8 lapic_id, tss_t *tss)
{
	smp_tss_by_lapic[lapic_id] = tss;
}

void
smp_init_single_cpu(void)
{
	if (smp_initialized) {
		return;
	}
	smp_initialized = 1;
	smp_bkl.locked = 0;
	smp_bkl.owner = 0;
	memset(smp_cpu_map, 0, sizeof(smp_cpu_map));
	memset(smp_tss_by_lapic, 0, sizeof(smp_tss_by_lapic));
	smp_ap_ready_clear();
	smp_bsp_lapic_id = lapic_is_enabled() ? lapic_get_id() : 0;
	smp_bsp_known = 1;
	smp_ap_cpu_index = 0;
	smp_cpu_count_var = 1;
	smp_cpu_map[0].lapic_id = smp_bsp_lapic_id;
	smp_cpu_map[0].present = 1;
	smp_cpu_map[0].online = 1;
	smp_cpu_map[0].cpu_index = 0;
	smp_cpu_map[0].current_thread = NULL;
	smp_cpu_map[0].stack_top = gdt_get_tss() ? gdt_get_tss()->rsp0 : 0;
	smp_cpu_map[0].tss = gdt_get_tss();
	smp_tss_register(smp_bsp_lapic_id, smp_cpu_map[0].tss);
	smp_ap_ready[0] = 1;
	pcpu_attach(0, smp_bsp_lapic_id);
	pcpu_set_syscall_stack(smp_cpu_map[0].stack_top);
	printk("[SMP] single CPU fallback initialized\n");
}

thread_t *
smp_current_thread(void)
{
	int	idx;
	idx = smp_cpu_index();
	return (smp_cpu_map[idx].current_thread);
}

void
smp_set_current_thread(thread_t *td)
{
	int	idx;
	idx = smp_cpu_index();
	smp_cpu_map[idx].current_thread = td;
}

int
smp_ap_started(u8 cpu_index)
{
	if (cpu_index >= SMP_MAX_CPUS) {
		return (0);
	}
	return (smp_ap_ready[cpu_index] != 0);
}

u8
smp_ap_target_index(void)
{
	return (smp_ap_cpu_index);
}

void
smp_lock(void)
{
	u8	cpu;
	u64	flags;
	cpu = smp_cpu_id();

	if (smp_bkl.locked && smp_bkl.owner == cpu) {
		smp_bkl.recursion++;
		return;
	}

	__asm__ volatile("pushfq; pop %0; cli" : "=r"(flags));
	smp_bkl_flags[cpu] = flags;

	while (__atomic_exchange_n(&smp_bkl.locked, 1,
	    __ATOMIC_ACQUIRE) != 0) {
		__asm__ volatile("pause");
	}
	smp_bkl.owner = cpu;
	smp_bkl.recursion = 0;
}

void
smp_unlock(void)
{
	u8	cpu;
	u64	flags;
	cpu = smp_cpu_id();

	if (!smp_bkl.locked || smp_bkl.owner != cpu) {
		panic("[SMP] BKL not held by CPU %d\n", (int)cpu);
	}

	if (smp_bkl.recursion > 0) {
		smp_bkl.recursion--;
		return;
	}

	flags = smp_bkl_flags[cpu];
	smp_bkl.owner = 0xFF;
	__atomic_store_n(&smp_bkl.locked, 0, __ATOMIC_RELEASE);
	__asm__ volatile("push %0; popfq" : : "r"(flags));
}

int
smp_lock_held(void)
{
	return (smp_bkl.locked && smp_bkl.owner == smp_cpu_id());
}

u32
smp_lock_release_all(void)
{
	u32	depth;

	if (!smp_lock_held()) {
		return (0);
	}
	depth = smp_bkl.recursion + 1;
	while (smp_lock_held()) {
		smp_unlock();
	}
	return (depth);
}

void
smp_lock_acquire_depth(u32 depth)
{
	u32	i;

	for (i = 0; i < depth; i++) {
		smp_lock();
	}
}

static void
smp_wait_ticks(u32 us)
{
	u64	start, deadline;
	u32	hz, ticks;

	hz = timer_get_frequency();
	if (!timer_is_initialized() || hz == 0) {
		for (ticks = 0; ticks < 100000; ticks++) {
			__asm__ volatile("pause");
		}
		return;
	}
	ticks = (u32)(((u64)us * hz + 999999ULL) / 1000000ULL);
	if (ticks == 0) {
		ticks = 1;
	}
	start = timer_get_ticks();
	deadline = start + ticks;
	while (timer_get_ticks() < deadline) {
		__asm__ volatile("pause");
	}
}

static void
smp_wait_us(u32 us)
{
	static int	pit_ch2_broken;

	if (!pit_ch2_broken) {
		if (pit_delay_us_checked(us) == 0) {
			return;
		}
		pit_ch2_broken = 1;
		printk("[SMP] PIT channel 2 unusable, using tick delays\n");
	}
	smp_wait_ticks(us);
}

static void
smp_send_init(u8 apic_id)
{
	lapic_icr_send(LAPIC_ICR_INIT | LAPIC_ICR_ASSERT, apic_id);
}

static void
smp_send_sipi(u8 vector, u8 apic_id)
{
	lapic_icr_send(LAPIC_ICR_STARTUP | LAPIC_ICR_ASSERT |
	    (u32)vector, apic_id);
}

static void
smp_trampoline_setup(u64 cr3, u64 stack_top, u8 cpu_index)
{
	u8	*page;
	u64	trampoline_size;
	u64	entry;
	u64	gdt_base;
	u16	gdt_limit;
	u8	*gdtr;
	u64	*gdt32;
	u64	*gdt64;

	page = (u8 *)TRAMPOLINE_VADDR;
	trampoline_size = (u64)ap_trampoline_end -
	    (u64)ap_trampoline_start;
	if (trampoline_size > 4096) {
		panic("[SMP] trampoline too large: %u bytes\n",
		    (u32)trampoline_size);
	}

	memcpy(page, ap_trampoline_start, trampoline_size);

	entry = (u64)ap_main;
	gdt_base = (u64)gdt_get_base();
	gdt_limit = gdt_get_limit();
	*(u64 *)(page + OFF_CR3) = cr3;
	*(u64 *)(page + OFF_STACK) = stack_top;
	*(u64 *)(page + OFF_CPU_INDEX) = cpu_index;
	*(u64 *)(page + OFF_ENTRY) = entry;
	gdtr = page + OFF_GDTR64;
	*(u16 *)gdtr = (5 * 8) - 1;
	*(u32 *)(gdtr + 2) = TRAMPOLINE_BASE + OFF_GDT64;
	gdt64 = (u64 *)(page + OFF_GDT64);
	gdt64[0] = 0;
	gdt64[1] = 0x00209A0000000000ULL;
	gdt64[2] = 0x0000920000000000ULL;
	gdt64[3] = 0x0000F20000000000ULL;
	gdt64[4] = 0x0020FA0000000000ULL;

	gdtr = page + OFF_GDTR32;
	*(u16 *)gdtr = (3 * 8) - 1;
	*(u32 *)(gdtr + 2) = TRAMPOLINE_BASE + OFF_GDT32;
	gdt32 = (u64 *)(page + OFF_GDT32);
	gdt32[0] = 0;
	gdt32[1] = 0x00CF9A000000FFFFULL;
	gdt32[2] = 0x00CF92000000FFFFULL;
}

static void
smp_start_ap(u8 apic_id, u8 cpu_index)
{
	u64	cr3;
	u64	*stack;
	u64	stack_top;
	u32	tries;
	u32	timeout;

	if (apic_id == smp_bsp_lapic_id) {
		return;
	}

	cr3 = pmap_kernel_cr3();
	stack = kmem_alloc_aligned(KERNEL_STACK_SIZE, 16);
	if (stack == NULL) {
		printk("[SMP] failed to allocate AP stack\n");
		return;
	}
	stack_top = (u64)stack + KERNEL_STACK_SIZE;
	smp_cpu_map[cpu_index].stack_top = stack_top;
	smp_ap_cpu_index = cpu_index;
	smp_ap_ready[cpu_index] = 0;
	smp_trampoline_setup(cr3, stack_top, cpu_index);
	printk("[SMP] starting AP %u (lapic %u) stack=%p\n",
	    (u32)cpu_index, (u32)apic_id, (void *)stack_top);

	smp_send_init(apic_id);
	smp_wait_us(10000);

	for (tries = 0; tries < 2; tries++) {
		smp_send_sipi(0x08, apic_id);
		smp_wait_us(200);

		timeout = 1000;
		while (timeout-- > 0) {
			if (smp_ap_ready[cpu_index]) {
				printk("[SMP] AP %u online\n",
				    (u32)cpu_index);
				return;
			}
			smp_wait_us(100);
		}
	}
	printk("[SMP] AP %u failed to start (stack %p leaked on purpose)\n",
	    (u32)cpu_index, (void *)stack_top);
}

static void
smp_ap_count_cb(acpi_madt_entry_header_t *entry, void *ctx)
{
	acpi_madt_local_apic_t	*lapic;
	int			*count;

	lapic = (acpi_madt_local_apic_t *)entry;
	count = (int *)ctx;
	if (!(lapic->flags & 1)) {
		return;
	}
	(*count)++;
}

static void
smp_ap_list_cb(acpi_madt_entry_header_t *entry, void *ctx)
{
	acpi_madt_local_apic_t	*lapic;
	u8			*next_index;
	u8			apic_id;
	int			idx;

	lapic = (acpi_madt_local_apic_t *)entry;
	next_index = (u8 *)ctx;
	if (!(lapic->flags & 1)) {
		return;
	}
	apic_id = lapic_id_from_entry(lapic);
	if (apic_id == smp_bsp_lapic_id) {
		idx = 0;
	} else {
		idx = (*next_index)++;
	}
	if (idx >= SMP_MAX_CPUS) {
		return;
	}
	smp_cpu_map[idx].lapic_id = apic_id;
	smp_cpu_map[idx].present = 1;
	smp_cpu_map[idx].online = (idx == 0) ? 1 : 0;
	smp_cpu_map[idx].cpu_index = idx;
	smp_cpu_map[idx].current_thread = NULL;
	smp_cpu_map[idx].stack_top = 0;
	smp_cpu_map[idx].tss = NULL;
}

static void
smp_init_bsp(void)
{
	u8	bsp_id;
	u8	next_index;
	int	ap_count;

	bsp_id = lapic_get_id();
	smp_bsp_lapic_id = bsp_id;
	smp_bsp_known = 1;
	memset(smp_cpu_map, 0, sizeof(smp_cpu_map));
	memset(smp_tss_by_lapic, 0, sizeof(smp_tss_by_lapic));
	smp_ap_ready_clear();
	smp_cpu_count_var = 1;
	smp_ap_cpu_index = 0;

	smp_cpu_map[0].lapic_id = bsp_id;
	smp_cpu_map[0].present = 1;
	smp_cpu_map[0].online = 1;
	smp_cpu_map[0].cpu_index = 0;
	smp_cpu_map[0].current_thread = NULL;
	smp_cpu_map[0].stack_top = gdt_get_tss() ? gdt_get_tss()->rsp0 : 0;
	smp_cpu_map[0].tss = gdt_get_tss();
	smp_tss_register(bsp_id, gdt_get_tss());
	pcpu_attach(0, bsp_id);
	pcpu_set_syscall_stack(smp_cpu_map[0].stack_top);

	ap_count = 0;
	acpi_madt_foreach(ACPI_MADT_LOCAL_APIC, smp_ap_count_cb,
	    &ap_count);
	if (ap_count <= 1) {
		printk("[SMP] only one CPU detected\n");
		return;
	}

	if (ap_count > SMP_MAX_CPUS) {
		printk("[SMP] %d CPUs reported, tracking only %d\n",
		    ap_count, SMP_MAX_CPUS);
		ap_count = SMP_MAX_CPUS;
	}

	next_index = 1;
	acpi_madt_foreach(ACPI_MADT_LOCAL_APIC, smp_ap_list_cb,
	    &next_index);

	smp_cpu_count_var = ap_count;
	printk("[SMP] BSP lapic %u, %d total cpus\n",
	    (u32)bsp_id, smp_cpu_count_var);
}

void
smp_init(void)
{
	u8	idx;

	if (smp_initialized) {
		return;
	}
	smp_initialized = 1;
	smp_bkl.locked = 0;
	smp_bkl.owner = 0;

	if (!acpi_is_initialized()) {
		printk("[SMP] ACPI not initialized, SMP disabled\n");
		smp_initialized = 0;
		smp_init_single_cpu();
		return;
	}
	if (!lapic_is_enabled()) {
		printk("[SMP] LAPIC not init SMP disabled\n");
		smp_initialized = 0;
		smp_init_single_cpu();
		return;
	}

	smp_ap_sched = cm_get_bool_default("SYSTEM", "Scheduler",
	    "ApScheduling", 0);

	smp_init_bsp();

	for (idx = 1; idx < smp_cpu_count_var && idx < SMP_MAX_CPUS; idx++) {
		if (!smp_cpu_map[idx].present) {
			continue;
		}
		smp_start_ap(smp_cpu_map[idx].lapic_id, idx);
	}
	printk("[SMP] %d cpu(s) present, %d scheduling (ap_sched=%d)\n",
	    smp_cpu_count_var, smp_sched_cpu_count(), smp_ap_sched);
}

static void
smp_ap_park(void)
{
	for (;;) {
		__asm__ volatile("cli; hlt");
	}
}

void
ap_main(u8 cpu_index)
{
	u64	stack_top;
	tss_t	*tss;
	gdt_entry_t	*gdt;
	gdt_ptr_t	gdt_ptr;
	u8	lapic_id;
	lapic_enable();
	lapic_id = lapic_get_id();
	if (cpu_index >= SMP_MAX_CPUS) {
		panic("[SMP]with ap cpu_index %u out of range\n",
		    (u32)cpu_index);
	}
	pcpu_attach((int)cpu_index, lapic_id);
	pmap_init();
	stack_top = smp_cpu_map[cpu_index].stack_top;
	if (smp_cpu_map[cpu_index].lapic_id != lapic_id) {
		smp_cpu_map[cpu_index].lapic_id = lapic_id;
		smp_cpu_map[cpu_index].present = 1;
		smp_cpu_map[cpu_index].cpu_index = cpu_index;
	}
	smp_cpu_map[cpu_index].current_thread = NULL;
	tss = kmem_alloc_aligned(sizeof(tss_t), 16);
	gdt = kmem_alloc_aligned(sizeof(gdt_entry_t) * 7, 16);
	if (tss == NULL || gdt == NULL) {
		panic("[SMP] with ap %u failed to allocate gdt/tss\n",
		    (u32)cpu_index);
	}
	memset(tss, 0, sizeof(tss_t));
	memset(gdt, 0, sizeof(gdt_entry_t) * 7);
	gdt_init_cpu(cpu_index, tss, gdt);
	if (stack_top != 0) {
		tss->rsp0 = stack_top;
	}
	pcpu_set_syscall_stack(stack_top);
	gdt_ptr.limit = sizeof(gdt_entry_t) * 7 - 1;
	gdt_ptr.base = (u64)gdt;
	gdt_flush((u64)&gdt_ptr);
	pcpu_reload_gsbase();
	tss_load(GDT_TSS);
	load_idt(&idt_ptr);
	syscall_init();
	smp_cpu_map[cpu_index].tss = tss;
	smp_tss_register(lapic_id, tss);
	trace_cpu_online();

	if (!smp_ap_scheduling_enabled()) {
		smp_cpu_map[cpu_index].online = 0;
		__atomic_store_n(&smp_ap_ready[cpu_index], 1,
		    __ATOMIC_RELEASE);
		printk("[SMP] ap %u lapic %u parked (ap scheduling off)\n",
		    (u32)cpu_index, (u32)lapic_id);
		smp_ap_park();
	}

	lapic_timer_init_ap();
	smp_cpu_map[cpu_index].online = 1;
	__atomic_store_n(&smp_ap_ready[cpu_index], 1, __ATOMIC_RELEASE);
	printk("[SMP] with ap  %u %u init succesful ,tss at %p\n",
	    (u32)cpu_index, (u32)lapic_id, (void *)tss);

	__asm__ volatile("sti");
	while (1) {
		__asm__ volatile("hlt");
	}
}
