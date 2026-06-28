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
#include <kernel/interrupts/idt.h>
#include <kernel/api/api.h>
#include <kernel/api/posix/posix.h>
#include <kernel/event/event.h>
#include <kernel/process.h>
#include <kernel/syscall.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <lib/com1.h>
#include <mm/vm/pmap.h>

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

extern void syscall_entry(void);
static int syscall_initialized = 0;

static inline void wrmsr(u32 msr, u64 value) {
  u32 low = value & 0xFFFFFFFF;
  u32 high = value >> 32;
  __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline u64 rdmsr(u32 msr) {
  u32 low, high;
  __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
  return ((u64)high << 32) | low;
}

void syscall_init(void) {
  /* 1. Enable SCE (System Call Extensions) in EFER */
  u64 efer = rdmsr(MSR_EFER);
  wrmsr(MSR_EFER, efer | 1);

  /* 2. Configure STAR register
   * STAR[47:32] = Kernel CS/SS base  (0x08 -> KCODE=0x08, KDATA=0x10)
   * STAR[63:48] = User CS/SS base    (0x10 -> UDATA=0x18, UCODE=0x20)
   */
  u64 star = ((u64)GDT_KERNEL_CODE << 32) | ((u64)GDT_KERNEL_DATA << 48);
  wrmsr(MSR_STAR, star);

  /* 3. Set LSTAR to our entry point */
  wrmsr(MSR_LSTAR, (u64)syscall_entry);

  /* 4. Configure SFMASK (RFLAGS mask)
   * Mask IF (interrupts), TF (trap), etc.
   */
  wrmsr(MSR_SFMASK, 0x200); /* Mask interrupts (IF) */

  com1_printf("[SYSCALL] syscall/sysret initialized\n");
  syscall_initialized = 1;
}

int syscall_is_initialized(void) { return syscall_initialized; }

void syscall_handler(registers_t *regs) {
  static u32 last_magic = 0;
  if (last_magic == 0) {
    last_magic = g_chainfs.superblock.magic;
  } else if (g_chainfs.superblock.magic != last_magic) {
    process_t *proc = process_current();
    com1_printf("[CHAINFS] magic changed in syscall (pid=%d) old=0x%x new=0x%x "
                "rip=%p cs=0x%x cr3=%p phys=%p init_phys=%p\n",
                proc ? proc->pid : -1, last_magic, g_chainfs.superblock.magic,
                (void *)(regs ? regs->rip : 0), regs ? regs->cs : 0,
                (void *)pmap_get_cr3(),
                (void *)pmap_extract((u64)&g_chainfs),
                (void *)g_chainfs_phys);
    last_magic = g_chainfs.superblock.magic;
  }

  u64 syscall_number = regs->rax;
  u64 arg1 = regs->rdi;
  u64 arg2 = regs->rsi;
  u64 arg3 = regs->rdx;

  if (syscall_number == CALL_PERSONALITY) {
    process_t *proc = process_current();
    if (!proc) {
      regs->rax = (u64)(-API_ERR_BAD_VALUE);
      return;
    }
    u64 old_personality = (u64)proc->personality;
    u64 new_personality = arg1;
    if (new_personality <= 1) {
      proc->personality = (int)new_personality;
      com1_printf("[SYSCALL] PID %d personality: %d -> %d\n",
                  proc->pid, (int)old_personality,
                  (int)new_personality);
    }
    regs->rax = old_personality;
    return;
  }

  process_t *cur_proc = process_current();
  if (cur_proc && cur_proc->personality == PERSONALITY_POSIX) {
    posix_signal_deliver(cur_proc, regs);
    posix_syscall_handler(regs);
    return;
  }

  switch (syscall_number) {
  case CALL_TERM_READ:
    regs->rax = (u64)api_term_read((void *)arg1, (u32)arg2);
    break;
  case CALL_TERM_WRITE:
    regs->rax = (u64)api_term_write((const void *)arg1, (u32)arg2);
    break;
  case CALL_DATA_OPEN:
    regs->rax = (u64)api_data_open((const char *)arg1, (int)arg2);
    break;
  case CALL_DATA_CLOSE:
    regs->rax = (u64)api_data_close((int)arg1);
    break;
  case CALL_DATA_READ:
    regs->rax = (u64)api_data_read((int)arg1, (void *)arg2, (u32)arg3);
    break;
  case CALL_DATA_WRITE:
    regs->rax = (u64)api_data_write((int)arg1, (const void *)arg2, (u32)arg3);
    break;
  case CALL_DATA_SEEK:
    regs->rax = (u64)api_data_seek((int)arg1, (long)arg2, (int)arg3);
    break;
  case CALL_DATA_PIPE:
    regs->rax = (u64)api_data_pipe((int *)arg1);
    break;
  case CALL_FS_CHDIR:
    regs->rax = (u64)api_fs_chdir((const char *)arg1);
    break;
  case CALL_FS_GETCWD:
    regs->rax = (u64)api_fs_getcwd((char *)arg1, (u32)arg2);
    break;
  case CALL_FS_LISTDIR:
    regs->rax = (u64)api_fs_listdir((const char *)arg1,
                                (struct api_dirent *)arg2, (u32)arg3);
    break;
  case CALL_MEM_MAP:
    regs->rax = (u64)api_mem_map((const void *)arg1);
    break;
  case CALL_MEM_UNMAP:
    regs->rax = (u64)api_mem_unmap((void *)arg1, arg2);
    break;
  case CALL_PROC_CLONE:
    regs->rax = (u64)api_proc_clone(arg1, arg2, arg3, regs);
    break;
  case CALL_PROC_COPY:
    regs->rax = (u64)api_proc_copy(regs);
    break;
  case CALL_PROC_SPAWN:
    regs->rax = (u64)api_proc_spawn((const char *)arg1, (const char *const *)arg2,
                                (const char *const *)arg3);
    break;
  case CALL_PROC_EXIT:
    process_exit((int)arg1);
    break;
  case CALL_PROC_WAIT:
    regs->rax = (u64)api_proc_wait((int *)arg1);
    break;
  case CALL_PROC_KILL:
    regs->rax = process_send_signal((u32)arg1, (int)arg2);
    break;
  case CALL_PROC_LIST:
    regs->rax = (u64)api_proc_list((struct api_proc_info *)arg1, (u32)arg2);
    break;
  case CALL_KUSR_AUTH:
    regs->rax = (u64)api_kusr_auth((const char *)arg1);
    break;
  case CALL_SYS_INFO:
    regs->rax = (u64)api_info((struct api_sysinfo *)arg1);
    break;
  case CALL_SYS_MEMINFO:
    regs->rax = (u64)api_meminfo((struct api_meminfo *)arg1);
    break;
  case CALL_SYS_KMEMINFO:
    regs->rax = (u64)api_kmeminfo((struct api_kmeminfo *)arg1);
    break;
  case CALL_DRM_CALL:
    regs->rax = (u64)api_drm_call(arg1, (void *)arg2);
    break;
  case CALL_EVENT_KQUEUE:
    regs->rax = (u64)kqueue_create();
    break;
  case CALL_EVENT_KEVENT: {
    /* arg1 = kq_idx, arg2 = pointer to kevent_args struct */
    struct {
      int kq_idx;
      struct kevent *changelist;
      int nchanges;
      struct kevent *eventlist;
      int nevents;
      s64 timeout_ms;
    } *args = (void *)arg2;

    if (!args) {
      regs->rax = (u64)(-API_ERR_BAD_ADDR);
      break;
    }
    regs->rax = (u64)kevent_process(args->kq_idx, args->changelist,
                                    args->nchanges, args->eventlist,
                                    args->nevents, args->timeout_ms);
    break;
  }
  case CALL_EVENT_CLOSE:
    regs->rax = (u64)kqueue_destroy((int)arg1);
    break;
  case CALL_PROC_GETPID:
    regs->rax = (u64)api_proc_getpid();
    break;
  case CALL_PROC_GETPPID:
    regs->rax = (u64)api_proc_getppid();
    break;
  default:
    com1_printf("Unknown syscall: %d\n", syscall_number);
    regs->rax = -API_ERR_NO_CALL;
    break;
  }
}
