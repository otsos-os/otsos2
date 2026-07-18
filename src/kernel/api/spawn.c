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
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <kernel/api/api.h>
#include <kernel/api/session.h>
#include <kernel/api/auxv.h>
#include <kernel/api/posix/posix.h>
#include <kernel/crypto/rng/rng.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/fs/devfs/devfs.h>
#include <kernel/console/terminal.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/vm/vm_map.h>
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

  char *out = (char *)kmem_calloc(len + 1, 1);
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

  char **arr = (char **)kmem_calloc(max_count + 1, sizeof(char *));
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
        kmem_free(arr[i]);
      }
      kmem_free(arr);
      return -API_ERR_BAD_ADDR;
    }
    arr[count++] = copy;
  }

  if (count == max_count) {
    if (is_user_address(&user[count], sizeof(char *)) && user[count] != NULL) {
      for (int i = 0; i < count; i++) {
        kmem_free(arr[i]);
      }
      kmem_free(arr);
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
    kmem_free(arr[i]);
  }
  kmem_free(arr);
}

static int read_file_into_buffer(const char *path, u8 **out_buf,
                                 u32 *out_size) {
  vnode_t *vn;
  posix_stat_t st;
  u32 size;
  u8 *buf;
  u32 bytes_read;

  if (vfs_resolve(path, &vn) != 0 || vn == NULL) {
    return -API_ERR_NOT_FOUND;
  }
  if (!vnode_can_exec(vn)) {
    vnode_release(vn);
    return -API_ERR_ACCESS;
  }

  if (vn->type == VDIR) {
    vnode_release(vn);
    return -API_ERR_BAD_IMAGE;
  }

  if (vnode_stat(vn, &st) != 0) {
    vnode_release(vn);
    return -API_ERR_IO;
  }

  size = (u32)st.st_size;
  if (size == 0) {
    vnode_release(vn);
    return -API_ERR_BAD_IMAGE;
  }

  buf = (u8 *)kmem_calloc(size, 1);
  if (!buf) {
    vnode_release(vn);
    return -API_ERR_NO_MEMORY;
  }

  bytes_read = 0;
  {
    int n = vnode_read(vn, buf, size, 0);
    if (n < 0) {
      vnode_release(vn);
      kmem_free(buf);
      return -API_ERR_IO;
    }
    bytes_read = (u32)n;
  }

  vnode_release(vn);

  if (bytes_read != size) {
    kmem_free(buf);
    return -API_ERR_IO;
  }

  *out_buf = buf;
  *out_size = size;
  return 0;
}

static u64 allocate_user_stack(process_t *proc) {
  if (vm_map_create_user_stack(proc) != 0) {
    return 0;
  }
  return USER_STACK_BASE;
}

#define SPAWN_AUXV_COUNT 8

static int build_user_stack(char **argv, int argc, char **envp, int envc,
                            const auxv_desc_t *aux, u64 *out_rsp, u64 *out_argv,
                            u64 *out_envp) {
  u64 sp = USER_STACK_BASE & ~0xFULL;
  u64 stack_min = USER_STACK_TOP;

  u64 *argv_ptrs = (u64 *)kmem_calloc(argc ? argc : 1, sizeof(u64));
  u64 *envp_ptrs = (u64 *)kmem_calloc(envc ? envc : 1, sizeof(u64));
  if (!argv_ptrs || !envp_ptrs) {
    kmem_free(argv_ptrs);
    kmem_free(envp_ptrs);
    return -API_ERR_NO_MEMORY;
  }

  for (int i = envc - 1; i >= 0; i--) {
    size_t len = strlen(envp[i]) + 1;
    if (sp < stack_min + len) {
      kmem_free(argv_ptrs);
      kmem_free(envp_ptrs);
      return -API_ERR_TOO_BIG;
    }
    sp -= len;
    memcpy((void *)sp, envp[i], len);
    envp_ptrs[i] = sp;
  }

  for (int i = argc - 1; i >= 0; i--) {
    size_t len = strlen(argv[i]) + 1;
    if (sp < stack_min + len) {
      kmem_free(argv_ptrs);
      kmem_free(envp_ptrs);
      return -API_ERR_TOO_BIG;
    }
    sp -= len;
    memcpy((void *)sp, argv[i], len);
    argv_ptrs[i] = sp;
  }

  sp &= ~0xFULL;
  if (sp < stack_min + 16) {
    kmem_free(argv_ptrs);
    kmem_free(envp_ptrs);
    return -API_ERR_TOO_BIG;
  }
  sp -= 16;
  u64 at_random_addr = sp;
  if (crypto_rng_bytes((u8 *)sp, 16) != 0) {
    memset((void *)sp, 0, 16);
  }

  sp &= ~0xFULL;


  u64 words = 1                    /* argc */
              + (u64)argc + 1      /* argv[] + NULL */
              + (u64)envc + 1      /* envp[] + NULL */
              + 2 * SPAWN_AUXV_COUNT; /* auxv pairs incl AT_NULL */

  u64 total = words * 8;
  if (sp < stack_min + total + 8) {
    kmem_free(argv_ptrs);
    kmem_free(envp_ptrs);
    return -API_ERR_TOO_BIG;
  }
  if (((sp - total) & 0xFULL) != 8) {
    sp -= 8;
  }

  sp -= 8; *(u64 *)sp = 0;// AT_NULL.a_val
  sp -= 8; *(u64 *)sp = AT_NULL;// AT_NULL.a_type

  u64 aux_pairs[SPAWN_AUXV_COUNT - 1][2] = {
      {AT_PHDR, aux->at_phdr},
      {AT_PHENT, aux->at_phent},
      {AT_PHNUM, aux->at_phnum},
      {AT_ENTRY, aux->at_entry},
      {AT_BASE, aux->at_base},
      {AT_PAGESZ, aux->at_pagesz},
      {AT_RANDOM, at_random_addr},
  };
  for (int i = SPAWN_AUXV_COUNT - 2; i >= 0; i--) {
    sp -= 8; *(u64 *)sp = aux_pairs[i][1];
    sp -= 8; *(u64 *)sp = aux_pairs[i][0];
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

  kmem_free(argv_ptrs);
  kmem_free(envp_ptrs);
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
  pmap_destroy(cr3);
}

int api_proc_spawn(const struct api_proc_spawn_args *uargs) {
  struct api_proc_spawn_args args;
  int child_personality;

  process_t *parent = process_current();
  if (!parent) {
    printk("[SPAWN] Error: no current process\n");
    return -API_ERR_BAD_VALUE;
  }

  if (!uargs || !is_user_address(uargs, sizeof(args))) {
    printk("[SPAWN] Error: invalid user spawn args %p\n",
                (void *)uargs);
    return -API_ERR_BAD_ADDR;
  }

  memcpy(&args, uargs, sizeof(args));
  if (args.size < sizeof(args) || args.flags != 0) {
    return -API_ERR_BAD_VALUE;
  }

  switch (args.abi) {
  case API_PROC_SPAWN_ABI_POSIX:
    child_personality = PERSONALITY_POSIX;
    break;
  case API_PROC_SPAWN_ABI_NATIVE:
    child_personality = PERSONALITY_OTSOS;
    break;
  default:
    return -API_ERR_BAD_VALUE;
  }

  if (!is_user_address(args.path, 1)) {
    printk("[SPAWN] Error: invalid user path pointer %p\n",
                (void *)args.path);
    return -API_ERR_BAD_ADDR;
  }

  char *kpath = copy_user_string(args.path, SPAWN_MAX_STR);
  if (!kpath) {
    printk("[SPAWN] Error: failed to copy user path\n");
    return -API_ERR_BAD_ADDR;
  }

  char **kargv = NULL;
  char **kenvp = NULL;
  int argc = copy_user_string_array(args.argv, &kargv, SPAWN_MAX_ARGS);
  if (argc < 0) {
    printk("[SPAWN] Error: failed to copy argv\n");
    kmem_free(kpath);
    return argc;
  }
  int envc = copy_user_string_array(args.envp, &kenvp, SPAWN_MAX_ENVP);
  if (envc < 0) {
    printk("[SPAWN] Error: failed to copy envp\n");
    free_string_array(kargv);
    kmem_free(kpath);
    return envc;
  }

  u8 *elf_buf = NULL;
  u32 elf_size = 0;
  int err = read_file_into_buffer(kpath, &elf_buf, &elf_size);
  if (err < 0) {
    printk("[SPAWN] Error: failed to read file '%s'\n", kpath);
    free_string_array(kargv);
    free_string_array(kenvp);
    kmem_free(kpath);
    return err;
  }

  process_t *child = alloc_process();
  if (!child) {
    kmem_free(elf_buf);
    free_string_array(kargv);
    free_string_array(kenvp);
    kmem_free(kpath);
    return -API_ERR_RETRY;
  }
  memset(child, 0, sizeof(process_t));

  u64 new_cr3 = pmap_create();
  if (!new_cr3) {
    printk("[SPAWN] Error: failed to create address space\n");
    memset(child, 0, sizeof(process_t));
    kmem_free(elf_buf);
    free_string_array(kargv);
    free_string_array(kenvp);
    kmem_free(kpath);
    return -API_ERR_NO_MEMORY;
  }

  u64 old_cr3 = pmap_get_cr3();
  pmap_load(new_cr3);

  elf_loadinfo_t li;
  u64 entry = elf_load_full(elf_buf, elf_size, &li);
  if (entry == 0) {
    printk("[SPAWN] Error: elf_load failed for '%s'\n", kpath);
    kmem_free(elf_buf);
    pmap_load(old_cr3);
    free_spawn_cr3(new_cr3);
    memset(child, 0, sizeof(process_t));
    free_string_array(kargv);
    free_string_array(kenvp);
    kmem_free(kpath);
    return -API_ERR_BAD_IMAGE;
  }

  auxv_desc_t aux = {
      .at_phdr = li.phdr_vaddr,
      .at_phent = li.phent,
      .at_phnum = li.phnum,
      .at_entry = li.entry,
      .at_base = 0,
      .at_pagesz = PAGE_SIZE,
  };

  if (li.interp_off != 0 && li.interp_len != 0) {
    char interp_path[256];
    u64 ilen = li.interp_len;
    if (ilen > sizeof(interp_path)) ilen = sizeof(interp_path);
    memcpy(interp_path, (char *)elf_buf + li.interp_off, ilen);
    interp_path[ilen - 1] = '\0';
    kmem_free(elf_buf);
    elf_buf = NULL;

    printk("[SPAWN] PT_INTERP '%s'\n", interp_path);

    u8 *interp_buf = NULL;
    u32 interp_size = 0;
    int ierr = read_file_into_buffer(interp_path, &interp_buf, &interp_size);
    if (ierr < 0) {
      printk("[SPAWN] Error: cannot load interpreter '%s'\n", interp_path);
      pmap_load(old_cr3);
      free_spawn_cr3(new_cr3);
      memset(child, 0, sizeof(process_t));
      free_string_array(kargv);
      free_string_array(kenvp);
      kmem_free(kpath);
      return ierr;
    }

    u64 interp_entry =
        elf_load_interp(interp_buf, interp_size, ELF_INTERP_BASE);
    kmem_free(interp_buf);
    if (interp_entry == 0) {
      printk("[SPAWN] Error: failed to load interpreter image\n");
      pmap_load(old_cr3);
      free_spawn_cr3(new_cr3);
      memset(child, 0, sizeof(process_t));
      free_string_array(kargv);
      free_string_array(kenvp);
      kmem_free(kpath);
      return -API_ERR_BAD_IMAGE;
    }

    aux.at_base = ELF_INTERP_BASE;
    entry = interp_entry;
  } else {
    kmem_free(elf_buf);
    elf_buf = NULL;
  }

  child->cr3 = new_cr3;
  child->owns_address_space = 1;
  child->mmap_base = MMAP_BASE;

  u64 user_stack = allocate_user_stack(child);
  if (user_stack == 0) {
    printk("[SPAWN] Error: allocate_user_stack failed\n");
    pmap_load(old_cr3);
    free_spawn_cr3(new_cr3);
    memset(child, 0, sizeof(process_t));
    free_string_array(kargv);
    free_string_array(kenvp);
    kmem_free(kpath);
    return -API_ERR_NO_MEMORY;
  }

  u64 new_rsp = 0;
  u64 argv_addr = 0;
  u64 envp_addr = 0;
  err = build_user_stack(kargv, argc, kenvp, envc, &aux, &new_rsp, &argv_addr,
                         &envp_addr);
  if (err < 0) {
    printk("[SPAWN] Error: build_user_stack failed\n");
    vm_map_free_all(child);
    pmap_load(old_cr3);
    free_spawn_cr3(new_cr3);
    memset(child, 0, sizeof(process_t));
    free_string_array(kargv);
    free_string_array(kenvp);
    kmem_free(kpath);
    return err;
  }

  free_string_array(kargv);
  free_string_array(kenvp);
  pmap_load(old_cr3);

  u32 pid = next_pid++;

  child->pid = pid;
  child->ppid = parent->pid;
  copy_process_name(child->name, kpath);

  child->cr3 = new_cr3;
  child->entry_point = entry;
  child->user_stack = user_stack;

  child->exit_code = 0;
  child->owns_address_space = 1;
  child->preferred_cpu = -1;
  child->last_cpu = -1;
  child->mmap_base = MMAP_BASE;
  /* Spawn inherits parent credentials for Linux-compatible privilege
   * semantics.  A root parent creates a root child; the child can drop
   * privileges via POSIX setuid/setgid if needed. */
  child->kusr_auth = parent->kusr_auth;
  child->uid = parent->uid;
  child->gid = parent->gid;
  child->euid = parent->euid;
  child->egid = parent->egid;
  child->suid = parent->suid;
  child->sgid = parent->sgid;
  api_copy_handles(child, parent);
  posix_init_process(child);
  posix_copy_fds(child, parent);
  child->personality = child_personality;
  api_session_fork(parent, child);
  if (parent->controlling_tty >= 0) {
    terminal_set_session(parent->controlling_tty, child->sid);
    terminal_set_pgrp(parent->controlling_tty, child->pgid);
  }
  if (child->controlling_tty >= 0) {
    child->pgid = child->pid;
    terminal_set_pgrp(child->controlling_tty, child->pgid);
  }
  scheduler_assign_process(child);
  posix_setup_stdio(child);

  /* Create the main thread for the child process */
  thread_t *td = thread_create(child, entry, new_rsp,
                               USER_CS, USER_DS);
  if (!td) {
    printk("[SPAWN] Error: failed to create thread\n");
    pmap_load(new_cr3);
    vm_map_free_all(child);
    pmap_load(old_cr3);
    pmap_destroy(new_cr3);
    memset(child, 0, sizeof(process_t));
    kmem_free(kpath);
    return -API_ERR_NO_MEMORY;
  }

  td->context.rsp = new_rsp;
  td->context.rdi = (u64)argc;
  td->context.rsi = argv_addr;
  td->context.rdx = envp_addr;
  td->context.rax = 0;

  child->main_thread = td;
  child->cur_thread = td;
  if (child->controlling_tty >= 0) {
    terminal_set_pgrp(child->controlling_tty, child->pgid);
  }

  printk("[SPAWN] Created '%s' (PID %d) from '%s'\n", child->name,
              child->pid, kpath);
  kmem_free(kpath);

  return (int)pid;
}
