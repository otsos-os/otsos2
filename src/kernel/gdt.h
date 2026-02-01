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

#ifndef GDT_H
#define GDT_H

#include <mlibc/mlibc.h>

/*
 * 0x00 - Null descriptor
 * 0x08 - Kernel Code (Ring 0, 64-bit)
 * 0x10 - Kernel Data (Ring 0)
 * 0x18 - User Code (Ring 3, 64-bit)
 * 0x20 - User Data (Ring 3)
 * 0x28 - TSS (16 bytes, spans 0x28-0x30)
 *
 *   Kernel CS = 0x08, Kernel DS = 0x10
 *   User CS = 0x1B (0x18 | 3), User DS = 0x23 (0x20 | 3)
 */

#define GDT_NULL_SEG 0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x18
#define GDT_USER_DATA 0x20
#define GDT_TSS 0x28

/* Selectors with RPL (Ring Privilege Level) */
#define KERNEL_CS (GDT_KERNEL_CODE)
#define KERNEL_DS (GDT_KERNEL_DATA)
#define USER_CS (GDT_USER_CODE | 3) /* 0x1B */
#define USER_DS (GDT_USER_DATA | 3) /* 0x23 */

/* Task State Segment (TSS) for x86_64 */
typedef struct {
  u32 reserved0;
  u64 rsp0; /* Stack pointer for Ring 0 */
  u64 rsp1; /* Stack pointer for Ring 1 (unused) */
  u64 rsp2; /* Stack pointer for Ring 2 (unused) */
  u64 reserved1;
  u64 ist1; /* Interrupt Stack Table 1 */
  u64 ist2;
  u64 ist3;
  u64 ist4;
  u64 ist5;
  u64 ist6;
  u64 ist7;
  u64 reserved2;
  u16 reserved3;
  u16 iomap_base; /* I/O Map Base Address */
} __attribute__((packed)) tss_t;

/* Initialize GDT with Ring 0/3 segments and TSS */
void gdt_init(void);

/* Set the kernel stack in TSS (called on context switch) */
void tss_set_rsp0(u64 stack);

/* Get current TSS RSP0 */
u64 tss_get_rsp0(void);

#endif
