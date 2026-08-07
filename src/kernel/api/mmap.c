/*
 * Copyright (c) 2026, otsos team
 */

#include <kernel/api/api.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/video/drm/gem.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_object.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mm/vm/vm_map.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define PAGE_SIZE 4096

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

static u64 page_flags_for_prot(u32 prot) {
  u64 flags = PTE_PRESENT | PTE_USER;
  if (prot & API_MAP_WRITE) flags |= PTE_RW;
  if (!(prot & API_MAP_EXEC)) flags |= PTE_NX;
  return flags;
}

static u64 kernel_virt_to_phys(void *ptr) {
  u64 vaddr = (u64)ptr;
  u64 page = vaddr & ~(u64)(PAGE_SIZE - 1);
  u64 phys = pmap_extract(page);

  if (phys == 0 && vaddr >= KERNEL_VMA) {
    phys = page - KERNEL_VMA;
  }
  if (phys == 0 && vaddr >= DMAP_BASE && vaddr < KERNEL_VMA) {
    phys = page - DMAP_BASE;
  }
  if (phys == 0) {
    return 0;
  }
  return phys | (vaddr & (PAGE_SIZE - 1));
}

/* Map a GEM buffer into the current process's address space. The GEM
 * buffer's physical pages are shared — writes are visible to the kernel
 * (and to other processes that map the same handle). This is zero-copy:
 * no data is duplicated. */
static u64 mmap_gem(process_t *proc, u32 gem_handle, u64 length, u32 prot,
                    u32 flags, u64 addr) {
  drm_gem_buffer_t *buf = drm_gem_lookup(gem_handle);
  if (!buf) {
    return (u64)(-API_ERR_NOT_FOUND);
  }
  if (length > buf->size) {
    length = buf->size;
  }
  u64 aligned = align_up(length, PAGE_SIZE);

  if (flags & API_MAP_FIXED) {
    if (addr == 0 || (addr & (PAGE_SIZE - 1)) != 0) {
      return (u64)(-API_ERR_BAD_VALUE);
    }
  } else {
    addr = vm_map_find_free(proc, aligned);
    if (!addr) {
      return (u64)(-API_ERR_NO_MEMORY);
    }
  }

  u64 pflags = page_flags_for_prot(prot);

  for (u64 off = 0; off < aligned; off += PAGE_SIZE) {
    u64 phys = kernel_virt_to_phys(buf->data + off);
    if (phys == 0) {
      for (u64 done = 0; done < off; done += PAGE_SIZE) {
        pmap_remove(addr + done);
      }
      return (u64)(-API_ERR_NO_MEMORY);
    }
    pmap_enter(addr + off, phys, pflags);
  }

  vm_object_t *obj = vm_object_create(VM_OBJ_GEM, aligned, buf);
  if (!obj) {
    for (u64 off = 0; off < aligned; off += PAGE_SIZE)
      pmap_remove(addr + off);
    return (u64)(-API_ERR_NO_MEMORY);
  }

  if (vm_map_insert(proc, addr, addr + aligned, prot, flags, gem_handle, obj,
                    0) != 0) {
    for (u64 off = 0; off < aligned; off += PAGE_SIZE) {
      pmap_remove(addr + off);
    }
    vm_object_unref(obj);
    return (u64)(-API_ERR_NO_MEMORY);
  }
  vm_object_unref(obj);

  return addr;
}

/* Anonymous mapping: pages are populated by vm_map_fault(). */
static u64 mmap_anon(process_t *proc, u64 length, u32 prot, u32 flags,
                     u64 addr) {
  u64 aligned = align_up(length, PAGE_SIZE);

  if (flags & API_MAP_FIXED) {
    if (addr == 0 || (addr & (PAGE_SIZE - 1)) != 0) {
      return (u64)(-API_ERR_BAD_VALUE);
    }
  } else {
    addr = vm_map_find_free(proc, aligned);
    if (!addr) {
      return (u64)(-API_ERR_NO_MEMORY);
    }
  }

  vm_object_t *obj = vm_object_create(VM_OBJ_ANON, aligned, NULL);
  if (!obj) {
    return (u64)(-API_ERR_NO_MEMORY);
  }

  if (vm_map_insert(proc, addr, addr + aligned, prot, flags, 0, obj, 0) != 0) {
    vm_object_unref(obj);
    return (u64)(-API_ERR_NO_MEMORY);
  }
  vm_object_unref(obj);

  return addr;
}

/* File-backed mapping: pages are read by vm_map_fault(). */
static u64 mmap_file(process_t *proc, u64 length, u32 prot, u32 flags,
                     u64 addr, int fd, u64 offset) {
  entity_id_t id;
  u32 access;
  vnode_t *vn;
  void *path;
  int ret;

  ret = entity_handle_lookup(proc, fd, &id, &access);
  if (ret != 0) {
    return (u64)(-API_ERR_BAD_HANDLE);
  }
  if (entity_arch(id) != ENTITY_ARCH_FILE) {
    return (u64)(-API_ERR_NO_DEVICE);
  }
  vn = (vnode_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
  path = entity_io_ptr(id, ENTITY_IO_PTR_PATH);
  if (vn == NULL || path == NULL) {
    return (u64)(-API_ERR_BAD_HANDLE);
  }

  posix_stat_t st;
  if (vnode_stat(vn, &st) != 0) {
    return (u64)(-API_ERR_NOT_FOUND);
  }

  u64 aligned = align_up(length, PAGE_SIZE);

  if (flags & API_MAP_FIXED) {
    if (addr == 0 || (addr & (PAGE_SIZE - 1)) != 0) {
      return (u64)(-API_ERR_BAD_VALUE);
    }
  } else {
    addr = vm_map_find_free(proc, aligned);
    if (!addr) {
      return (u64)(-API_ERR_NO_MEMORY);
    }
  }

  vm_object_t *obj = vm_object_create(VM_OBJ_FILE, aligned,
                                      path);
  if (!obj) {
    return (u64)(-API_ERR_NO_MEMORY);
  }

  if (vm_map_insert(proc, addr, addr + aligned, prot, flags, 0, obj,
                    offset) != 0) {
    vm_object_unref(obj);
    return (u64)(-API_ERR_NO_MEMORY);
  }
  vm_object_unref(obj);

  return addr;
}

u64 api_mem_map(const void *uargs) {
  process_t *proc = process_current();
  if (!proc) {
    return (u64)(-API_ERR_BAD_VALUE);
  }
  if (!is_user_address(uargs, sizeof(mmap_args_t))) {
    return (u64)(-API_ERR_BAD_ADDR);
  }

  mmap_args_t args;
  memcpy(&args, uargs, sizeof(args));

  if (args.length == 0) {
    return (u64)(-API_ERR_BAD_VALUE);
  }

  u64 length = align_up(args.length, PAGE_SIZE);
  u64 addr = args.addr;

  /* GEM buffer mapping — maps GPU memory into userspace, zero-copy. */
  if (args.flags & API_MAP_GEM) {
    return mmap_gem(proc, (u32)args.fd, length, args.prot, args.flags, addr);
  }

  /* Anonymous mapping — fresh zero-filled pages. */
  if (args.flags & API_MAP_ANON) {
    return mmap_anon(proc, length, args.prot, args.flags, addr);
  }

  /* File-backed mapping. */
  return mmap_file(proc, length, args.prot, args.flags, addr, args.fd,
                   args.offset);
}

int api_mem_unmap(void *addr, u64 length) {
  process_t *proc = process_current();
  if (!proc || !addr || length == 0) {
    return -API_ERR_BAD_VALUE;
  }

  u64 vaddr = (u64)addr;
  if ((vaddr & (PAGE_SIZE - 1)) != 0) {
    return -API_ERR_BAD_VALUE;
  }
  if (!is_user_address(addr, length)) {
    return -API_ERR_BAD_ADDR;
  }

  vma_t *vma = vm_map_lookup(proc, vaddr);
  if (!vma) {
    return -API_ERR_NOT_FOUND;
  }

  u64 aligned = align_up(length, PAGE_SIZE);

  /* Unmap pages. For GEM mappings, we do NOT free the physical pages —
   * they belong to the GEM buffer. For anonymous/file mappings, we free
   * the physical pages. */
  for (u64 off = 0; off < aligned && vaddr + off < vma->end; off += PAGE_SIZE) {
    u64 va = vaddr + off;
    if (vma->object == NULL) {
      u64 phys = pmap_extract(va);
      if (phys)
        vm_page_free_phys(phys);
    }
    pmap_remove(va);
  }

  vm_map_remove(proc, vaddr);
  return 0;
}
