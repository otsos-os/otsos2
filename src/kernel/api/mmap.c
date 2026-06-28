/*
 * Copyright (c) 2026, otsos team
 */

#include <kernel/api/api.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/video/drm/gem.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_object.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mm/vm/vm_map.h>
#include <lib/com1.h>
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

  /* GEM owns its backing pages; mmap only aliases them into userspace. */
  u64 phys_base = (u64)buf->data;
  for (u64 off = 0; off < aligned; off += PAGE_SIZE) {
    pmap_enter(addr + off, phys_base + off, pflags);
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

/* Anonymous mapping: allocate fresh physical pages. */
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

  u64 pflags = page_flags_for_prot(prot);
  vm_object_t *obj = vm_object_create(VM_OBJ_ANON, aligned, NULL);
  if (!obj) {
    return (u64)(-API_ERR_NO_MEMORY);
  }

  for (u64 off = 0; off < aligned; off += PAGE_SIZE) {
    u64 page = vm_page_alloc_phys(0);
    if (!page) {
      for (u64 rollback = 0; rollback < off; rollback += PAGE_SIZE) {
        u64 phys = pmap_extract(addr + rollback);
        pmap_remove(addr + rollback);
        vm_page_free_phys(phys);
        vm_object_set_page(obj, rollback / PAGE_SIZE, 0);
      }
      vm_object_unref(obj);
      return (u64)(-API_ERR_NO_MEMORY);
    }
    memset((void *)page, 0, PAGE_SIZE);
    vm_object_set_page(obj, off / PAGE_SIZE, page);
    pmap_enter(addr + off, page, pflags);
  }

  if (vm_map_insert(proc, addr, addr + aligned, prot, flags, 0, obj, 0) != 0) {
    for (u64 off = 0; off < aligned; off += PAGE_SIZE) {
      u64 phys = pmap_extract(addr + off);
      pmap_remove(addr + off);
      vm_page_free_phys(phys);
      vm_object_set_page(obj, off / PAGE_SIZE, 0);
    }
    vm_object_unref(obj);
    return (u64)(-API_ERR_NO_MEMORY);
  }
  vm_object_unref(obj);

  return addr;
}

/* File-backed mapping: read file contents into freshly allocated pages. */
static u64 mmap_file(process_t *proc, u64 length, u32 prot, u32 flags,
                     u64 addr, int fd, u64 offset) {
  api_handle_t *handles = api_get_handle_table();
  api_object_t *objects = api_get_object_table();

  if (fd < 0 || fd >= MAX_HANDLES || !handles[fd].used) {
    return (u64)(-API_ERR_BAD_HANDLE);
  }
  int oi = handles[fd].object_index;
  if (oi < 0 || oi >= MAX_DATA_OBJECTS || !objects[oi].used) {
    return (u64)(-API_ERR_BAD_HANDLE);
  }
  if (objects[oi].type != API_OBJECT_FILE) {
    return (u64)(-API_ERR_NO_DEVICE);
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(objects[oi].path, &entry, &entry_block,
                        &entry_offset) != 0) {
    return (u64)(-API_ERR_NOT_FOUND);
  }

  u32 file_size = entry.size;
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
  vm_object_t *obj = vm_object_create(VM_OBJ_FILE, aligned,
                                      (void *)objects[oi].path);
  if (!obj) {
    return (u64)(-API_ERR_NO_MEMORY);
  }

  for (u64 off = 0; off < aligned; off += PAGE_SIZE) {
    u64 page = vm_page_alloc_phys(0);
    if (!page) {
      for (u64 rollback = 0; rollback < off; rollback += PAGE_SIZE) {
        u64 phys = pmap_extract(addr + rollback);
        pmap_remove(addr + rollback);
        vm_page_free_phys(phys);
        vm_object_set_page(obj, rollback / PAGE_SIZE, 0);
      }
      vm_object_unref(obj);
      return (u64)(-API_ERR_NO_MEMORY);
    }
    memset((void *)page, 0, PAGE_SIZE);

    u64 file_off = offset + off;
    if (file_off < file_size) {
      u32 to_copy = (u32)(file_size - file_off);
      if (to_copy > PAGE_SIZE) to_copy = PAGE_SIZE;
      u32 bytes_read = 0;
      chainfs_read_file_range(objects[oi].path, (u8 *)page, to_copy,
                              (u32)file_off, &bytes_read);
    }

    vm_object_set_page(obj, off / PAGE_SIZE, page);
    pmap_enter(addr + off, page, pflags);
  }

  if (vm_map_insert(proc, addr, addr + aligned, prot, flags, 0, obj,
                    offset) != 0) {
    for (u64 off = 0; off < aligned; off += PAGE_SIZE) {
      u64 phys = pmap_extract(addr + off);
      pmap_remove(addr + off);
      vm_page_free_phys(phys);
      vm_object_set_page(obj, off / PAGE_SIZE, 0);
    }
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
