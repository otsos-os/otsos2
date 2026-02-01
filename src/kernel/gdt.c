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

#include <kernel/gdt.h>
#include <lib/com1.h>

/*
 * GDT Entry structure for x86_64
 * Each entry is 8 bytes, except TSS which is 16 bytes
 */
typedef struct {
  u16 limit_low;
  u16 base_low;
  u8 base_mid;
  u8 access;
  u8 granularity;
  u8 base_high;
} __attribute__((packed)) gdt_entry_t;

/* TSS descriptor is 16 bytes in long mode */
typedef struct {
  u16 limit_low;
  u16 base_low;
  u8 base_mid;
  u8 access;
  u8 granularity;
  u8 base_mid_high;
  u32 base_high;
  u32 reserved;
} __attribute__((packed)) tss_descriptor_t;

typedef struct {
  u16 limit;
  u64 base;
} __attribute__((packed)) gdt_ptr_t;

/*
 * GDT: 5 normal entries + 1 TSS entry (16 bytes = 2 slots)
 * Total: 7 slots worth of space
 */
static gdt_entry_t gdt[7] __attribute__((aligned(16)));
static tss_t tss __attribute__((aligned(16)));
static gdt_ptr_t gdt_ptr;

/* Kernel stack for TSS (16KB aligned) */
static u8 kernel_stack[16384] __attribute__((aligned(16)));

/*
 * Set a GDT entry
 * access byte format:
 *   bit 7: Present
 *   bits 5-6: DPL (privilege level)
 *   bit 4: Descriptor type (1 = code/data, 0 = system)
 *   bit 3: Executable
 *   bit 2: Direction/Conforming
 *   bit 1: Readable/Writable
 *   bit 0: Accessed
 *
 * granularity byte format:
 *   bit 7: 4KB granularity
 *   bit 6: 32-bit (0 for long mode data, 0 for code uses L bit)
 *   bit 5: Long mode (1 for 64-bit code segment)
 *   bits 0-3: limit high
 */
static void gdt_set_entry(int idx, u32 base, u32 limit, u8 access,
                          u8 granularity) {
  gdt[idx].limit_low = limit & 0xFFFF;
  gdt[idx].base_low = base & 0xFFFF;
  gdt[idx].base_mid = (base >> 16) & 0xFF;
  gdt[idx].access = access;
  gdt[idx].granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
  gdt[idx].base_high = (base >> 24) & 0xFF;
}

/* Set TSS descriptor (16 bytes in long mode) */
static void gdt_set_tss(int idx, u64 base, u32 limit) {
  tss_descriptor_t *tss_desc = (tss_descriptor_t *)&gdt[idx];

  tss_desc->limit_low = limit & 0xFFFF;
  tss_desc->base_low = base & 0xFFFF;
  tss_desc->base_mid = (base >> 16) & 0xFF;
  tss_desc->access = 0x89; /* Present, DPL=0, TSS available */
  tss_desc->granularity = ((limit >> 16) & 0x0F);
  tss_desc->base_mid_high = (base >> 24) & 0xFF;
  tss_desc->base_high = (base >> 32) & 0xFFFFFFFF;
  tss_desc->reserved = 0;
}

/* Assembly function to load GDT and reload segments */
extern void gdt_flush(u64 gdt_ptr_addr);
extern void tss_load(u16 selector);

void gdt_init(void) {
  com1_printf("[GDT] Initializing GDT with Ring 3 support...\n");

  /* Initialize TSS */
  memset(&tss, 0, sizeof(tss_t));
  tss.rsp0 = (u64)&kernel_stack[sizeof(kernel_stack)]; /* Top of kernel stack */
  tss.iomap_base = sizeof(tss_t); /* No I/O permission bitmap */

  /* Null descriptor */
  gdt_set_entry(0, 0, 0, 0, 0);

  /* Kernel Code: base=0, limit=0xFFFFF, DPL=0, executable, readable, long mode
   */
  /* Access: 1001 1010 = 0x9A (Present, Ring 0, Code, Executable, Readable) */
  /* Granularity: 0010 0000 = 0x20 (Long mode bit set) */
  gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0x20);

  /* Kernel Data: base=0, limit=0xFFFFF, DPL=0, writable */
  /* Access: 1001 0010 = 0x92 (Present, Ring 0, Data, Writable) */
  /* Granularity: 0000 0000 = 0x00 (no special flags for data in long mode) */
  gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0x00);

  /* User Data: base=0, limit=0xFFFFF, DPL=3, writable */
  /* Access: 1111 0010 = 0xF2 (Present, Ring 3, Data, Writable) */
  /* Granularity: 0000 0000 = 0x00 */
  gdt_set_entry(3, 0, 0xFFFFF, 0xF2, 0x00);

  /* User Code: base=0, limit=0xFFFFF, DPL=3, executable, readable, long mode */
  /* Access: 1111 1010 = 0xFA (Present, Ring 3, Code, Executable, Readable) */
  /* Granularity: 0010 0000 = 0x20 (Long mode bit set) */
  gdt_set_entry(4, 0, 0xFFFFF, 0xFA, 0x20);

  /* TSS descriptor (occupies slots 5 and 6) */
  gdt_set_tss(5, (u64)&tss, sizeof(tss_t) - 1);

  /* Set up GDT pointer */
  gdt_ptr.limit = sizeof(gdt) - 1;
  gdt_ptr.base = (u64)&gdt;

  /* Load GDT */
  gdt_flush((u64)&gdt_ptr);

  /* Load TSS */
  tss_load(GDT_TSS);

  com1_printf("[GDT] GDT loaded at %p, TSS at %p\n", &gdt, &tss);
  com1_printf("[GDT] Kernel stack RSP0: %p\n", (void *)tss.rsp0);
}

void tss_set_rsp0(u64 stack) { tss.rsp0 = stack; }

u64 tss_get_rsp0(void) { return tss.rsp0; }
