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
#include <kernel/mmu.h>
#include <kernel/posix/posix.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <lib/com1.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

typedef struct {
  u64 addr;
  u64 length;
  u32 prot;
  u32 flags;
  int fd;
  u64 offset;
} __attribute__((packed)) mmap_args_t;

static u64 align_up(u64 val, u64 align) {
  return (val + align - 1) & ~(align - 1);
}

static int range_is_free(u64 base, u64 pages) {
  for (u64 i = 0; i < pages; i++) {
    u64 vaddr = base + i * PAGE_SIZE;
    u64 flags = mmu_get_pte_flags(vaddr);
    if (flags & PTE_PRESENT) {
      return 0;
    }
  }
  return 1;
}

static u64 find_free_region(process_t *proc, u64 length) {
  u64 pages = align_up(length, PAGE_SIZE) / PAGE_SIZE;
  u64 start = proc->mmap_base;
  if (start < MMAP_BASE) {
    start = MMAP_BASE;
  }
  if (start >= MMAP_LIMIT) {
    start = MMAP_BASE;
  }

  for (u64 addr = align_up(start, PAGE_SIZE); addr + pages * PAGE_SIZE < MMAP_LIMIT;
       addr += PAGE_SIZE) {
    if (range_is_free(addr, pages)) {
      proc->mmap_base = addr + pages * PAGE_SIZE;
      return addr;
    }
  }

  for (u64 addr = MMAP_BASE; addr + pages * PAGE_SIZE < start;
       addr += PAGE_SIZE) {
    if (range_is_free(addr, pages)) {
      proc->mmap_base = addr + pages * PAGE_SIZE;
      return addr;
    }
  }

  return 0;
}

u64 sys_mmap(const void *uargs) {
  process_t *proc = process_current();
  if (!proc) {
    return 0;
  }
  if (!is_user_address(uargs, sizeof(mmap_args_t))) {
    return 0;
  }

  mmap_args_t args;
  memcpy(&args, uargs, sizeof(args));

  if (args.length == 0) {
    return 0;
  }
  if (!(args.flags & MAP_PRIVATE)) {
    return 0;
  }

  u64 length = align_up(args.length, PAGE_SIZE);
  u64 addr = args.addr;

  if (args.flags & MAP_FIXED) {
    if (addr == 0 || (addr & (PAGE_SIZE - 1)) != 0) {
      return 0;
    }
    if (!range_is_free(addr, length / PAGE_SIZE)) {
      return 0;
    }
  } else {
    addr = find_free_region(proc, length);
    if (!addr) {
      return 0;
    }
  }

  int file_backed = !(args.flags & MAP_ANONYMOUS);
  char file_path[256];
  u32 file_size = 0;

  if (file_backed) {
    file_descriptor_t *fd_table = posix_get_fd_table();
    open_file_t *oft = posix_get_open_file_table();
    if (args.fd < 0 || args.fd >= MAX_FDS || !fd_table[args.fd].used) {
      return 0;
    }
    int of_index = fd_table[args.fd].of_index;
    if (of_index < 0 || of_index >= MAX_OPEN_FILES || !oft[of_index].used) {
      return 0;
    }
    if (oft[of_index].type != OFT_TYPE_FILE) {
      return 0;
    }
    chainfs_file_entry_t entry;
    u32 entry_block, entry_offset;
    if (chainfs_find_file(oft[of_index].path, &entry, &entry_block,
                          &entry_offset) != 0) {
      return 0;
    }
    file_size = entry.size;
    memset(file_path, 0, sizeof(file_path));
    int len = strlen(oft[of_index].path);
    if (len >= (int)sizeof(file_path)) {
      len = (int)sizeof(file_path) - 1;
    }
    memcpy(file_path, oft[of_index].path, len);
  }

  u64 page_flags = PTE_PRESENT | PTE_USER;
  if (args.prot & PROT_WRITE) {
    page_flags |= PTE_RW;
  }
  if (!(args.prot & PROT_EXEC)) {
    page_flags |= PTE_NX;
  }

  for (u64 off = 0; off < length; off += PAGE_SIZE) {
    void *page = kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (!page) {
      for (u64 rollback = 0; rollback < off; rollback += PAGE_SIZE) {
        u64 vaddr = addr + rollback;
        u64 paddr = mmu_virt_to_phys(vaddr);
        mmu_unmap_page(vaddr);
        if (paddr) {
          kfree((void *)paddr);
        }
      }
      return 0;
    }
    memset(page, 0, PAGE_SIZE);
    mmu_map_page(addr + off, (u64)page, page_flags);

    if (file_backed) {
      u64 file_off = args.offset + off;
      if (file_off < file_size) {
        u32 to_copy = (u32)(file_size - file_off);
        if (to_copy > PAGE_SIZE) {
          to_copy = PAGE_SIZE;
        }
        u32 bytes_read = 0;
        if (chainfs_read_file_range(file_path, (u8 *)page, to_copy,
                                    (u32)file_off, &bytes_read) != 0) {
          com1_printf("[MMAP] Error: file read failed\n");
        }
      }
    }
  }

  return addr;
}
