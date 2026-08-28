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

#include <kernel/console/console.h>
#include <kernel/gdt.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_map.h>
#include <mm/vm/vm_object.h>
#include <mm/vm/vm_page.h>
#include <kernel/api/posix/posix.h>
#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <mlibc/mlibc.h>
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
  printk("[USERSPACE] Initializing userspace subsystem...\n");

  /* Initialize GDT with Ring 3 support */
  gdt_init();
  status_line("gdt", gdt_is_initialized());

  /* Initialize process subsystem */
  process_init();
  status_line("process", process_is_initialized());

  printk("[USERSPACE] Userspace ready\n");
}

/* Allocate and map user stack */
static u64 allocate_user_stack(process_t *proc) {
  if (vm_map_create_user_stack(proc) != 0) {
    printk("[USERSPACE] Error: Failed to allocate user stack\n");
    return 0;
  }
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
    printk("[USERSPACE] Error: failed to create data/BSS object\n");
    return;
  }

  for (va = data_start, idx = 0; va < data_end; va += PAGE_SIZE, idx++) {
    phys = pmap_extract(va);
    if (phys == 0) {
      phys = vm_page_alloc_phys(0);
      if (phys == 0) {
        printk("[USERSPACE] Error: failed to allocate data/BSS page at %p\n", (void *)va);
        break;
      }
      memset((void *)(phys + DMAP_BASE), 0, PAGE_SIZE);
      pmap_enter(va, phys, PTE_PRESENT | PTE_RW | PTE_USER);
    }
    vm_object_set_page(obj, idx, phys);
  }

  vm_map_insert(proc, data_start, data_end, API_MAP_READ | API_MAP_WRITE,
      API_MAP_PRIVATE | API_MAP_ANON, 0, obj, 0);
  vm_object_unref(obj);
}

process_t *userspace_load_elf(const char *name, void *elf_data, u64 elf_size) {
  printk("[USERSPACE] Loading ELF process '%s' (%d bytes)\n", name,
              (int)elf_size);

  u64 new_cr3 = pmap_create();
  if (new_cr3 == 0) {
    printk("[USERSPACE] Error: Failed to create address space\n");
    return NULL;
  }

  /* Allocate a new process slot */
  process_t *new_proc = alloc_process();
  if (!new_proc) {
    printk("[USERSPACE] Error: No free process slots\n");
    pmap_destroy(new_cr3);
    return NULL;
  }
  memset(new_proc, 0, sizeof(process_t));
  new_proc->cr3 = new_cr3;
  new_proc->owns_address_space = 1;
  new_proc->mmap_base = MMAP_BASE;

  u64 old_cr3 = pmap_get_cr3();
  pmap_load(new_cr3);

  /* Validate and load ELF */
  elf_loadinfo_t li;
  u64 entry = elf_load_full(elf_data, elf_size, &li);
  if (entry == 0) {
    printk("[USERSPACE] Error: Failed to load ELF\n");
    pmap_load(old_cr3);
    pmap_destroy(new_cr3);
    process_creation_abort(new_proc);
    return NULL;
  }

  /* Allocate user stack */
  u64 user_stack = allocate_user_stack(new_proc);
  if (user_stack == 0) {
    vm_map_free_all(new_proc);
    pmap_load(old_cr3);
    pmap_destroy(new_cr3);
    memset(new_proc, 0, sizeof(process_t));
    return NULL;
  }

  register_data_bss(new_proc, li.data_start, li.data_end);

  pmap_load(old_cr3);

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

  new_proc->owns_address_space = 1;
  new_proc->preferred_cpu = -1;
  new_proc->last_cpu = -1;
  new_proc->mmap_base = MMAP_BASE;
  new_proc->uid = 0;
  new_proc->gid = 0;
  new_proc->euid = 0;
  new_proc->egid = 0;
  new_proc->suid = 0;
  new_proc->sgid = 0;
  if (li.data_end != 0) {
    new_proc->brk_min = li.data_end;
  } else if (li.load_addr_max != 0) {
    new_proc->brk_min = (li.load_addr_max + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
  } else {
    new_proc->brk_min = MMAP_BASE;
  }
  new_proc->brk = new_proc->brk_min;
  api_init_process(new_proc);
  posix_init_process(new_proc);
  new_proc->personality = PERSONALITY_OTSOS;
  if (process_entity_attach(new_proc) != 0) {
    printk("[USERSPACE] error: entity attach failed\n");
    pmap_load(new_cr3);
    vm_map_free_all(new_proc);
    pmap_load(old_cr3);
    pmap_destroy(new_cr3);
    process_creation_abort(new_proc);
    return NULL;
  }
  scheduler_assign_process(new_proc);

  thread_t *td = thread_create(new_proc, entry, user_stack,
                               USER_CS, USER_DS);
  if (!td) {
    printk("[USERSPACE] error: Failed to create thread\n");
    pmap_load(new_cr3);
    vm_map_free_all(new_proc);
    pmap_load(old_cr3);
    pmap_destroy(new_cr3);
    process_creation_abort(new_proc);
    return NULL;
  }

  new_proc->main_thread = td;
  new_proc->cur_thread = td;

  printk("[USERSPACE] Created process '%s' (PID %d)\n", new_proc->name,
              new_proc->pid);
  printk("[USERSPACE]   Entry: %p\n", (void *)entry);
  printk("[USERSPACE]   User stack: %p\n", (void *)user_stack);
  printk("[USERSPACE]   Kernel stack: %p\n",
              (void *)td->kernel_stack);

  return new_proc;
}

void userspace_load_init(void *module_start, u64 module_size) {
  printk("[USERSPACE] Loading init process from module...\n");
  printk("[USERSPACE] Module at %p, size %d bytes\n", module_start,
              (int)module_size);

  process_t *init = userspace_load_elf("init", module_start, module_size);
  if (!init) {
    printk("[USERSPACE] FATAL: Failed to load init!\n");
    while (1)
      __asm__ volatile("hlt");
  }

  /* Init always has kusr rights */
  init->kusr_auth = 1;
  init->uid = 0;
  init->gid = 0;
  init->euid = 0;
  init->egid = 0;
  init->suid = 0;
  init->sgid = 0;
  printk("[USERSPACE] Granted kusr rights to init (PID %d)\n", init->pid);

  /* Init must be PID 1 */
  if (init->pid != 1) {
    printk("[USERSPACE] Warning: init is not PID 1 (got %d)\n", init->pid);
  }

  process_dump(init);

  /* Jump to userspace */
  userspace_jump(init);
}

void userspace_jump(process_t *proc) {
  if (!proc) {
    printk("[USERSPACE] Error: Cannot jump to NULL process\n");
    return;
  }

  thread_t *td = proc->main_thread;
  if (!td) {
    printk("[USERSPACE] Error: No main thread for process\n");
    return;
  }

  printk("[USERSPACE] Jumping to userspace: %s (PID %d)\n", proc->name,
              proc->pid);
  printk("[USERSPACE]   Entry: %p\n", (void *)proc->entry_point);
  printk("[USERSPACE]   Stack: %p\n", (void *)proc->user_stack);
  printk("[USERSPACE]   CS: 0x%x, SS: 0x%x\n", (u32)td->context.cs,
              (u32)td->context.ss);

  /* Set as current process */
  process_set_current(proc);
  thread_load_fpu_context(td);
  pmap_load(proc->cr3);

  /* Enter userspace via iretq */
  userspace_enter(proc->entry_point, proc->user_stack, td->context.cs,
                  td->context.ss);

  /* Should never return */
  printk("[USERSPACE] FATAL: Returned from userspace!\n");
  while (1)
    __asm__ volatile("hlt");
}
