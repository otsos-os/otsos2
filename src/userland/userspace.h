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

#ifndef USERSPACE_H
#define USERSPACE_H

#include <kernel/process.h>
#include <mlibc/mlibc.h>

/* Default user stack virtual address */
#define USER_STACK_BASE 0x00007FFFFFFFFFF0
#define USER_STACK_TOP (USER_STACK_BASE - USER_STACK_SIZE + 16)

/* Initialize userspace subsystem */
void userspace_init(void);

/* Load init process from multiboot module */
void userspace_load_init(void *module_start, u64 module_size);

/* Load ELF and create userspace process */
process_t *userspace_load_elf(const char *name, void *elf_data, u64 elf_size);

/* Jump to userspace (starts executing the process) */
void userspace_jump(process_t *proc);

/* Assembly function to switch to Ring 3 */
extern void userspace_enter(u64 entry, u64 user_stack, u64 user_cs,
                            u64 user_ds);

#endif
