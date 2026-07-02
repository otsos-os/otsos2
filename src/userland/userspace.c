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

#include <kernel/console.h>
#include <kernel/gdt.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_map.h>
#include <mm/vm/vm_object.h>
#include <mm/vm/vm_page.h>
#include <kernel/api/posix/posix.h>
#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>
#include <userland/elf.h>
#include <userland/userspace.h>

static void status_line(const char *label, int ok) {
  const int pad_col = 32;
  int len = strlen(label);
  printf("%s", label);
  for (int i = len; i < pad_col; i++) {
    console_putchar(' ');
  }
  if (ok) {
    printf("\033[32m[OK]\033[0m\n");
  } else {
    printf("\033[31m[FAILED]\033[0m\n");
  }
}

void userspace_init(void) {
  com1_printf("[USERSPACE] Initializing userspace subsystem...\n");

  /* Initialize GDT with Ring 3 support */
  gdt_init();
  status_line("gdt", gdt_is_initialized());

  /* Initialize process subsystem */
  process_init();
  status_line("process", process_is_initialized());

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
    u64 page = vm_page_alloc_phys(0);
    if (!page) {
      com1_printf("[USERSPACE] Error: Failed to allocate stack page\n");
      for (u64 j = 0; j < i; j++) {
        u64 vaddr = stack_bottom + (j * PAGE_SIZE);
        u64 paddr = pmap_extract(vaddr);
        pmap_remove(vaddr);
        if (paddr) {
          vm_page_free_phys(paddr);
        }
      }
      return 0;
    }
    memset((void *)page, 0, PAGE_SIZE);

    u64 vaddr = stack_bottom + (i * PAGE_SIZE);
    pmap_enter(vaddr, page, PTE_PRESENT | PTE_RW | PTE_USER | PTE_NX);
  }

  /* Return top of stack (stack grows downward) */
  return USER_STACK_BASE;
}

void
register_data_bss(process_t *proc, u64 data_start, u64 data_end)
{
  vm_object_t *obj;
  u64 va;
  u64 phys;
  u64 idx;

  if (data_start == 0 || data_end == 0 || data_start >= data_end)
    return;

  obj = vm_object_create(VM_OBJ_ANON, data_end - data_start, NULL);
  if (!obj) {
    com1_printf("[USERSPACE] Error: failed to create data/BSS object\n");
    return;
  }

  for (va = data_start, idx = 0; va < data_end; va += PAGE_SIZE, idx++) {
    phys = pmap_extract(va);
    if (phys == 0) {
      phys = vm_page_alloc_phys(0);
      if (phys == 0) {
        com1_printf("[USERSPACE] Error: failed to allocate data/BSS page at %p\n", (void *)va);
        break;
      }
      memset((void *)phys, 0, PAGE_SIZE);
      pmap_enter(va, phys, PTE_PRESENT | PTE_RW | PTE_USER);
    }
    vm_object_set_page(obj, idx, phys);
  }

  vm_map_insert(proc, data_start, data_end, API_MAP_READ | API_MAP_WRITE,
      API_MAP_PRIVATE | API_MAP_ANON, 0, obj, 0);
  vm_object_unref(obj);
}

process_t *userspace_load_elf(const char *name, void *elf_data, u64 elf_size) {
  com1_printf("[USERSPACE] Loading ELF process '%s' (%d bytes)\n", name,
              (int)elf_size);

  u64 new_cr3 = pmap_create();
  if (new_cr3 == 0) {
    com1_printf("[USERSPACE] Error: Failed to create address space\n");
    return NULL;
  }

  u64 old_cr3 = pmap_get_cr3();
  pmap_load(new_cr3);

  /* Validate and load ELF */
  elf_loadinfo_t li;
  u64 entry = elf_load_full(elf_data, elf_size, &li);
  if (entry == 0) {
    com1_printf("[USERSPACE] Error: Failed to load ELF\n");
    pmap_load(old_cr3);
    pmap_destroy(new_cr3);
    return NULL;
  }

  /* Allocate user stack */
  u64 user_stack = allocate_user_stack();
  if (user_stack == 0) {
    pmap_load(old_cr3);
    pmap_destroy(new_cr3);
    return NULL;
  }

  pmap_load(old_cr3);

  /* Allocate a new process slot */
  process_t *new_proc = alloc_process();
  if (!new_proc) {
    com1_printf("[USERSPACE] Error: No free process slots\n");
    pmap_destroy(new_cr3);
    return NULL;
  }

  memset(new_proc, 0, sizeof(process_t));

  /* Initialize process */
  new_proc->pid = next_pid++;
  new_proc->ppid = 0; /* Kernel is parent for init */

  /* Copy name */
  int i;
  for (i = 0; i < PROCESS_NAME_LEN - 1 && name[i]; i++) {
    new_proc->name[i] = name[i];
  }
  new_proc->name[i] = '\0';

  /* Memory */
  new_proc->cr3 = new_cr3;
  new_proc->entry_point = entry;

  /* Stacks */
  new_proc->user_stack = user_stack;

  new_proc->exit_code = 0;
  new_proc->owns_address_space = 1;
  new_proc->mmap_base = MMAP_BASE;
  if (li.data_end != 0) {
    new_proc->brk_min = li.data_end;
  } else if (li.load_addr_max != 0) {
    new_proc->brk_min = (li.load_addr_max + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
  } else {
    new_proc->brk_min = MMAP_BASE;
  }
  new_proc->brk = new_proc->brk_min;
  register_data_bss(new_proc, li.data_start, li.data_end);
  api_init_process(new_proc);
  posix_init_process(new_proc);
  new_proc->personality = PERSONALITY_OTSOS;

  thread_t *td = thread_create(new_proc, entry, user_stack,
                               USER_CS, USER_DS);
  if (!td) {
    com1_printf("[USERSPACE] error: Failed to create thread\n");
    pmap_destroy(new_cr3);
    memset(new_proc, 0, sizeof(process_t));
    return NULL;
  }

  new_proc->main_thread = td;
  new_proc->cur_thread = td;

  com1_printf("[USERSPACE] Created process '%s' (PID %d)\n", new_proc->name,
              new_proc->pid);
  com1_printf("[USERSPACE]   Entry: %p\n", (void *)entry);
  com1_printf("[USERSPACE]   User stack: %p\n", (void *)user_stack);
  com1_printf("[USERSPACE]   Kernel stack: %p\n",
              (void *)td->kernel_stack);

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

  /* Init always has kusr rights */
  init->kusr_auth = 1;
  com1_printf("[USERSPACE] Granted kusr rights to init (PID %d)\n", init->pid);

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

  thread_t *td = proc->main_thread;
  if (!td) {
    com1_printf("[USERSPACE] Error: No main thread for process\n");
    return;
  }

  com1_printf("[USERSPACE] Jumping to userspace: %s (PID %d)\n", proc->name,
              proc->pid);
  com1_printf("[USERSPACE]   Entry: %p\n", (void *)proc->entry_point);
  com1_printf("[USERSPACE]   Stack: %p\n", (void *)proc->user_stack);
  com1_printf("[USERSPACE]   CS: 0x%x, SS: 0x%x\n", (u32)td->context.cs,
              (u32)td->context.ss);

  /* Set as current process */
  process_set_current(proc);
  pmap_load(proc->cr3);

  /* Enter userspace via iretq */
  userspace_enter(proc->entry_point, proc->user_stack, td->context.cs,
                  td->context.ss);

  /* Should never return */
  com1_printf("[USERSPACE] FATAL: Returned from userspace!\n");
  while (1)
    __asm__ volatile("hlt");
}
