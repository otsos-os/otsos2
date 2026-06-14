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

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/gdt.h>
#include <kernel/mmu.h>
#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>
#include <userland/elf.h>
#include <userland/userspace.h>

#define SPAWN_MAX_ARGS 64
#define SPAWN_MAX_ENVP 64
#define SPAWN_MAX_STR 256

static char *copy_user_string(const char *user, size_t max_len) {
  if (!user) {
    return NULL;
  }
  if (!is_user_address(user, 1)) {
    return NULL;
  }

  size_t len = 0;
  while (len < max_len) {
    if (!is_user_address(user + len, 1)) {
      return NULL;
    }
    if (user[len] == '\0') {
      break;
    }
    len++;
  }

  if (len == max_len) {
    return NULL;
  }

  char *out = (char *)kcalloc(len + 1, 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, user, len);
  out[len] = '\0';
  return out;
}

static int copy_user_string_array(const char *const *user, char ***out,
                                  int max_count) {
  if (!user) {
    *out = NULL;
    return 0;
  }

  if (!is_user_address(user, sizeof(char *))) {
    return -API_ERR_BAD_ADDR;
  }

  char **arr = (char **)kcalloc(max_count + 1, sizeof(char *));
  if (!arr) {
    return -API_ERR_NO_MEMORY;
  }

  int count = 0;
  while (count < max_count) {
    if (!is_user_address(&user[count], sizeof(char *))) {
      break;
    }
    const char *ptr = user[count];
    if (!ptr) {
      break;
    }
    char *copy = copy_user_string(ptr, SPAWN_MAX_STR);
    if (!copy) {
      for (int i = 0; i < count; i++) {
        kfree(arr[i]);
      }
      kfree(arr);
      return -API_ERR_BAD_ADDR;
    }
    arr[count++] = copy;
  }

  if (count == max_count) {
    if (is_user_address(&user[count], sizeof(char *)) && user[count] != NULL) {
      for (int i = 0; i < count; i++) {
        kfree(arr[i]);
      }
      kfree(arr);
      return -API_ERR_TOO_BIG;
    }
  }

  arr[count] = NULL;
  *out = arr;
  return count;
}

static void free_string_array(char **arr) {
  if (!arr) {
    return;
  }
  for (int i = 0; arr[i]; i++) {
    kfree(arr[i]);
  }
  kfree(arr);
}

static int read_file_into_buffer(const char *path, u8 **out_buf,
                                 u32 *out_size) {
  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(path, &entry, &entry_block, &entry_offset) != 0) {
    return -API_ERR_NOT_FOUND;
  }

  u32 size = entry.size;
  if (size == 0) {
    return -API_ERR_BAD_IMAGE;
  }

  u8 *buf = (u8 *)kcalloc(size, 1);
  if (!buf) {
    return -API_ERR_NO_MEMORY;
  }

  u32 bytes_read = 0;
  if (chainfs_read_file(path, buf, size, &bytes_read) != 0 ||
      bytes_read != size) {
    kfree(buf);
    return -API_ERR_IO;
  }

  *out_buf = buf;
  *out_size = size;
  return 0;
}

static u64 allocate_user_stack(void) {
  u64 stack_pages = (USER_STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
  u64 stack_bottom = USER_STACK_TOP;

  for (u64 i = 0; i < stack_pages; i++) {
    void *page = kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (!page) {
      for (u64 j = 0; j < i; j++) {
        u64 vaddr = stack_bottom + (j * PAGE_SIZE);
        u64 paddr = mmu_virt_to_phys(vaddr);
        mmu_unmap_page(vaddr);
        if (paddr) {
          kfree((void *)paddr);
        }
      }
      return 0;
    }
    memset(page, 0, PAGE_SIZE);

    u64 vaddr = stack_bottom + (i * PAGE_SIZE);
    mmu_map_page(vaddr, (u64)page, PTE_PRESENT | PTE_RW | PTE_USER | PTE_NX);
  }

  return USER_STACK_BASE;
}

static int build_user_stack(char **argv, int argc, char **envp, int envc,
                            u64 *out_rsp, u64 *out_argv, u64 *out_envp) {
  u64 sp = USER_STACK_BASE & ~0xFULL;
  u64 stack_min = USER_STACK_TOP;

  u64 *argv_ptrs = (u64 *)kcalloc(argc ? argc : 1, sizeof(u64));
  u64 *envp_ptrs = (u64 *)kcalloc(envc ? envc : 1, sizeof(u64));
  if (!argv_ptrs || !envp_ptrs) {
    kfree(argv_ptrs);
    kfree(envp_ptrs);
    return -API_ERR_NO_MEMORY;
  }

  for (int i = envc - 1; i >= 0; i--) {
    size_t len = strlen(envp[i]) + 1;
    if (sp < stack_min + len) {
      kfree(argv_ptrs);
      kfree(envp_ptrs);
      return -API_ERR_TOO_BIG;
    }
    sp -= len;
    memcpy((void *)sp, envp[i], len);
    envp_ptrs[i] = sp;
  }

  for (int i = argc - 1; i >= 0; i--) {
    size_t len = strlen(argv[i]) + 1;
    if (sp < stack_min + len) {
      kfree(argv_ptrs);
      kfree(envp_ptrs);
      return -API_ERR_TOO_BIG;
    }
    sp -= len;
    memcpy((void *)sp, argv[i], len);
    argv_ptrs[i] = sp;
  }

  sp &= ~0xFULL;

  if (sp < stack_min + (u64)(8 * (argc + envc + 3))) {
    kfree(argv_ptrs);
    kfree(envp_ptrs);
    return -API_ERR_TOO_BIG;
  }

  sp -= 8;
  *(u64 *)sp = 0;
  for (int i = envc - 1; i >= 0; i--) {
    sp -= 8;
    *(u64 *)sp = envp_ptrs[i];
  }

  u64 envp_addr = sp;

  sp -= 8;
  *(u64 *)sp = 0;
  for (int i = argc - 1; i >= 0; i--) {
    sp -= 8;
    *(u64 *)sp = argv_ptrs[i];
  }

  u64 argv_addr = sp;

  sp -= 8;
  *(u64 *)sp = (u64)argc;

  *out_rsp = sp;
  *out_argv = argv_addr;
  *out_envp = envp_addr;

  kfree(argv_ptrs);
  kfree(envp_ptrs);
  return 0;
}

static void copy_process_name(char *dst, const char *path) {
  const char *base = path;
  for (const char *p = path; *p; p++) {
    if (*p == '/') {
      base = p + 1;
    }
  }

  memset(dst, 0, PROCESS_NAME_LEN);
  for (int i = 0; i < PROCESS_NAME_LEN - 1 && base[i]; i++) {
    dst[i] = base[i];
  }
}

static void free_spawn_cr3(u64 cr3) {
  if (!cr3) {
    return;
  }
  mmu_free_user_space(cr3);
  kfree((void *)(cr3 & PTE_ADDR_MASK));
}

int api_proc_spawn(const char *path, const char *const *argv,
                   const char *const *envp) {
  process_t *parent = process_current();
  if (!parent) {
    com1_printf("[SPAWN] Error: no current process\n");
    return -API_ERR_BAD_VALUE;
  }

  if (!is_user_address(path, 1)) {
    com1_printf("[SPAWN] Error: invalid user path pointer %p\n",
                (void *)path);
    return -API_ERR_BAD_ADDR;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    com1_printf("[SPAWN] Error: ChainFS not initialized (magic=0x%x)\n",
                g_chainfs.superblock.magic);
    return -API_ERR_IO;
  }

  char *kpath = copy_user_string(path, SPAWN_MAX_STR);
  if (!kpath) {
    com1_printf("[SPAWN] Error: failed to copy user path\n");
    return -API_ERR_BAD_ADDR;
  }

  char **kargv = NULL;
  char **kenvp = NULL;
  int argc = copy_user_string_array(argv, &kargv, SPAWN_MAX_ARGS);
  if (argc < 0) {
    com1_printf("[SPAWN] Error: failed to copy argv\n");
    kfree(kpath);
    return argc;
  }
  int envc = copy_user_string_array(envp, &kenvp, SPAWN_MAX_ENVP);
  if (envc < 0) {
    com1_printf("[SPAWN] Error: failed to copy envp\n");
    free_string_array(kargv);
    kfree(kpath);
    return envc;
  }

  u8 *elf_buf = NULL;
  u32 elf_size = 0;
  int err = read_file_into_buffer(kpath, &elf_buf, &elf_size);
  if (err < 0) {
    com1_printf("[SPAWN] Error: failed to read file '%s'\n", kpath);
    free_string_array(kargv);
    free_string_array(kenvp);
    kfree(kpath);
    return err;
  }

  process_t *child = alloc_process();
  if (!child) {
    kfree(elf_buf);
    free_string_array(kargv);
    free_string_array(kenvp);
    kfree(kpath);
    return -API_ERR_RETRY;
  }
  child->state = PROC_STATE_EMBRYO;

  u64 new_cr3 = mmu_create_address_space();
  if (!new_cr3) {
    com1_printf("[SPAWN] Error: failed to create address space\n");
    memset(child, 0, sizeof(process_t));
    child->state = PROC_STATE_UNUSED;
    kfree(elf_buf);
    free_string_array(kargv);
    free_string_array(kenvp);
    kfree(kpath);
    return -API_ERR_NO_MEMORY;
  }

  u8 *kstack = (u8 *)kmalloc_aligned(KERNEL_STACK_SIZE, 16);
  if (!kstack) {
    free_spawn_cr3(new_cr3);
    memset(child, 0, sizeof(process_t));
    child->state = PROC_STATE_UNUSED;
    kfree(elf_buf);
    free_string_array(kargv);
    free_string_array(kenvp);
    kfree(kpath);
    return -API_ERR_NO_MEMORY;
  }
  memset(kstack, 0, KERNEL_STACK_SIZE);

  u64 old_cr3 = mmu_read_cr3();
  mmu_write_cr3(new_cr3);

  u64 entry = elf_load(elf_buf, elf_size);
  kfree(elf_buf);
  if (entry == 0) {
    com1_printf("[SPAWN] Error: elf_load failed for '%s'\n", kpath);
    mmu_write_cr3(old_cr3);
    free_spawn_cr3(new_cr3);
    kfree(kstack);
    memset(child, 0, sizeof(process_t));
    child->state = PROC_STATE_UNUSED;
    free_string_array(kargv);
    free_string_array(kenvp);
    kfree(kpath);
    return -API_ERR_BAD_IMAGE;
  }

  u64 user_stack = allocate_user_stack();
  if (user_stack == 0) {
    com1_printf("[SPAWN] Error: allocate_user_stack failed\n");
    mmu_write_cr3(old_cr3);
    free_spawn_cr3(new_cr3);
    kfree(kstack);
    memset(child, 0, sizeof(process_t));
    child->state = PROC_STATE_UNUSED;
    free_string_array(kargv);
    free_string_array(kenvp);
    kfree(kpath);
    return -API_ERR_NO_MEMORY;
  }

  u64 new_rsp = 0;
  u64 argv_addr = 0;
  u64 envp_addr = 0;
  err = build_user_stack(kargv, argc, kenvp, envc, &new_rsp, &argv_addr,
                         &envp_addr);
  if (err < 0) {
    com1_printf("[SPAWN] Error: build_user_stack failed\n");
    mmu_write_cr3(old_cr3);
    free_spawn_cr3(new_cr3);
    kfree(kstack);
    memset(child, 0, sizeof(process_t));
    child->state = PROC_STATE_UNUSED;
    free_string_array(kargv);
    free_string_array(kenvp);
    kfree(kpath);
    return err;
  }

  free_string_array(kargv);
  free_string_array(kenvp);
  mmu_write_cr3(old_cr3);

  u32 pid = next_pid++;
  memset(child, 0, sizeof(process_t));

  child->pid = pid;
  child->ppid = parent->pid;
  child->state = PROC_STATE_RUNNABLE;
  copy_process_name(child->name, kpath);

  child->cr3 = new_cr3;
  child->entry_point = entry;
  child->kernel_stack = (u64)(kstack + KERNEL_STACK_SIZE);
  child->user_stack = user_stack;

  memset(&child->context, 0, sizeof(cpu_context_t));
  child->context.rip = entry;
  child->context.rsp = new_rsp;
  child->context.cs = USER_CS;
  child->context.ss = USER_DS;
  child->context.rflags = 0x202;
  child->context.rdi = (u64)argc;
  child->context.rsi = argv_addr;
  child->context.rdx = envp_addr;
  child->context.rax = 0;

  child->exit_code = 0;
  child->owns_address_space = 1;
  child->mmap_base = MMAP_BASE;
  api_copy_handles(child, parent);
  child->next = NULL;

  com1_printf("[SPAWN] Created '%s' (PID %d) from '%s'\n", child->name,
              child->pid, kpath);
  kfree(kpath);

  return (int)pid;
}
