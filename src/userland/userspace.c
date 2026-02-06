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
#include <kernel/mmu.h>
#include <kernel/process.h>
#include <lib/com1.h>
#include <mlibc/memory.h>
#include <userland/elf.h>
#include <userland/userspace.h>

void userspace_init(void) {
  com1_printf("[USERSPACE] Initializing userspace subsystem...\n");

  /* Initialize GDT with Ring 3 support */
  gdt_init();

  /* Initialize process subsystem */
  process_init();

  com1_printf("[USERSPACE] Userspace ready\n");
}

/* Allocate and map user stack */
static u64 allocate_user_stack(void) {
  /* Allocate stack pages */
  u64 stack_pages = (USER_STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
  u64 stack_bottom = USER_STACK_TOP;

  com1_printf("[USERSPACE] Allocating user stack: %d pages at %p\n",
              (int)stack_pages, (void *)stack_bottom);

  for (u64 i = 0; i < stack_pages; i++) {
    void *page = kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (!page) {
      com1_printf("[USERSPACE] Error: Failed to allocate stack page\n");
      return 0;
    }
    memset(page, 0, PAGE_SIZE);

    u64 vaddr = stack_bottom + (i * PAGE_SIZE);
    mmu_map_page(vaddr, (u64)page, PTE_PRESENT | PTE_RW | PTE_USER | PTE_NX);
  }

  /* Return top of stack (stack grows downward) */
  return USER_STACK_BASE;
}

process_t *userspace_load_elf(const char *name, void *elf_data, u64 elf_size) {
  com1_printf("[USERSPACE] Loading ELF process '%s' (%d bytes)\n", name,
              (int)elf_size);

  /* Validate and load ELF */
  u64 entry = elf_load(elf_data, elf_size);
  if (entry == 0) {
    com1_printf("[USERSPACE] Error: Failed to load ELF\n");
    return NULL;
  }

  /* Allocate process structure */
  process_t *proc = process_get(0); /* Get first available slot */
  if (!proc) {
    /* Need to find free slot manually */
    extern process_t *alloc_process(void); /* Defined in process.c */
  }

  /* Allocate kernel stack */
  u8 *kstack = (u8 *)kmalloc_aligned(KERNEL_STACK_SIZE, 16);
  if (!kstack) {
    com1_printf("[USERSPACE] Error: Failed to allocate kernel stack\n");
    return NULL;
  }
  memset(kstack, 0, KERNEL_STACK_SIZE);

  /* Allocate user stack */
  u64 user_stack = allocate_user_stack();
  if (user_stack == 0) {
    kfree(kstack);
    return NULL;
  }

  /* Allocate a new process slot */
  process_t *new_proc = alloc_process();

  if (!new_proc) {
    com1_printf("[USERSPACE] Error: No free process slots\n");
    kfree(kstack);
    /* TODO: free user stack pages */
    return NULL;
  }

  /* Initialize process */
  new_proc->pid = next_pid++;
  new_proc->ppid = 0; /* Kernel is parent for init */
  new_proc->state = PROC_STATE_EMBRYO;

  /* Copy name */
  int i;
  for (i = 0; i < PROCESS_NAME_LEN - 1 && name[i]; i++) {
    new_proc->name[i] = name[i];
  }
  new_proc->name[i] = '\0';

  /* Memory */
  new_proc->cr3 = mmu_read_cr3();
  new_proc->entry_point = entry;

  /* Stacks */
  new_proc->kernel_stack = (u64)(kstack + KERNEL_STACK_SIZE);
  new_proc->user_stack = user_stack;

  /* Initialize context for userspace execution */
  memset(&new_proc->context, 0, sizeof(cpu_context_t));
  new_proc->context.rip = entry;
  new_proc->context.cs = USER_CS;
  new_proc->context.rflags = 0x202; /* IF=1 */
  new_proc->context.rsp = user_stack;
  new_proc->context.ss = USER_DS;

  new_proc->exit_code = 0;
  new_proc->next = NULL;

  new_proc->state = PROC_STATE_RUNNABLE;

  com1_printf("[USERSPACE] Created process '%s' (PID %d)\n", new_proc->name,
              new_proc->pid);
  com1_printf("[USERSPACE]   Entry: %p\n", (void *)entry);
  com1_printf("[USERSPACE]   User stack: %p\n", (void *)user_stack);
  com1_printf("[USERSPACE]   Kernel stack: %p\n",
              (void *)new_proc->kernel_stack);

  return new_proc;
}

void userspace_load_init(void *module_start, u64 module_size) {
  com1_printf("[USERSPACE] Loading init process from module...\n");
  com1_printf("[USERSPACE] Module at %p, size %d bytes\n", module_start,
              (int)module_size);

  process_t *init = userspace_load_elf("init", module_start, module_size);
  if (!init) {
    com1_printf("[USERSPACE] FATAL: Failed to load init!\n");
    while (1)
      __asm__ volatile("hlt");
  }

  /* Init must be PID 1 */
  if (init->pid != 1) {
    com1_printf("[USERSPACE] Warning: init is not PID 1 (got %d)\n", init->pid);
  }

  process_dump(init);

  /* Jump to userspace */
  userspace_jump(init);
}

void userspace_jump(process_t *proc) {
  if (!proc) {
    com1_printf("[USERSPACE] Error: Cannot jump to NULL process\n");
    return;
  }

  com1_printf("[USERSPACE] Jumping to userspace: %s (PID %d)\n", proc->name,
              proc->pid);
  com1_printf("[USERSPACE]   Entry: %p\n", (void *)proc->entry_point);
  com1_printf("[USERSPACE]   Stack: %p\n", (void *)proc->user_stack);
  com1_printf("[USERSPACE]   CS: 0x%x, SS: 0x%x\n", (u32)proc->context.cs,
              (u32)proc->context.ss);

  /* Set as current process */
  process_set_current(proc);

  /* Enter userspace via iretq */
  userspace_enter(proc->entry_point, proc->user_stack, proc->context.cs,
                  proc->context.ss);

  /* Should never return */
  com1_printf("[USERSPACE] FATAL: Returned from userspace!\n");
  while (1)
    __asm__ volatile("hlt");
}
