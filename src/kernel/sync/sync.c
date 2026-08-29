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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

$define %func sync_init as procedure with args void
$define %func sync_configure as procedure with args void

*/

/* !SPACE!

$space %export sync_init, sync_configure
$space %export spin_deadlock_spins

*/

#include <kernel/cm/cm.h>
#include <kernel/smp/pcpu.h>
#include <kernel/sync/sync.h>
#include <mlibc/stdio.h>

extern u64	spin_deadlock_spins;

void
sync_init(void)
{
	pcpu_init();
	witness_init();
	pcpu_attach(0, 0);
}

void
sync_configure(void)
{
	u32	spins;
	int	on;

	on = cm_get_bool_default("SYSTEM", "Sync", "Witness", 0);
	witness_enable(on);
	spins = cm_get_u32_default("SYSTEM", "Sync", "SpinTimeoutKcycles", 0);
	if (spins != 0) {
		spin_deadlock_spins = (u64)spins * 1000ULL;
	}
	printk("[SYNC] witness=%d spin_timeout=%llu\n", on,
	    (unsigned long long)spin_deadlock_spins);
}
