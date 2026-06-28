/*
 * Copyright (c) 2026, otsos team
 */

#include <mm/vm/vm_map.h>
#include <kernel/api/api.h>
#include <mm/kmem.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_pager.h>
#include <mlibc/mlibc.h>

#define PAGE_SIZE 4096

static u64 align_up(u64 val, u64 align) {
  return (val + align - 1) & ~(align - 1);
}

static u64 page_flags_for_prot(u32 prot) {
  u64 flags = PTE_PRESENT | PTE_USER;
  if (prot & API_MAP_WRITE) flags |= PTE_RW;
  if (!(prot & API_MAP_EXEC)) flags |= PTE_NX;
  return flags;
}

u64 vm_map_find_free(process_t *proc, u64 length) {
  u64 aligned = align_up(length, PAGE_SIZE);
  if (aligned == 0) return 0;

  u64 search_start = proc->mmap_base;
  if (search_start < MMAP_BASE) search_start = MMAP_BASE;
  if (search_start >= MMAP_LIMIT) search_start = MMAP_BASE;

  u64 addr = align_up(search_start, PAGE_SIZE);

  while (addr + aligned <= MMAP_LIMIT) {
    u64 end = addr + aligned;
    int overlap = 0;

    for (vma_t *v = proc->vma_list; v; v = v->next) {
      if (addr < v->end && end > v->start) {
        addr = v->end;
        align_up_addr:
        addr = align_up(addr, PAGE_SIZE);
        overlap = 1;
        break;
      }
    }

    if (!overlap) {
      proc->mmap_base = end;
      return addr;
    }

    if (addr + aligned > MMAP_LIMIT) break;
    (void)end;
  }

  addr = MMAP_BASE;
  while (addr + aligned <= search_start) {
    u64 end = addr + aligned;
    int overlap = 0;

    for (vma_t *v = proc->vma_list; v; v = v->next) {
      if (addr < v->end && end > v->start) {
        addr = align_up(v->end, PAGE_SIZE);
        overlap = 1;
        break;
      }
    }

    if (!overlap) {
      proc->mmap_base = end;
      return addr;
    }
  }

  return 0;
}

int vm_map_insert(process_t *proc, u64 start, u64 end, u32 prot, u32 flags,
            u32 gem_handle, vm_object_t *object, u64 object_offset) {
  vma_t *vma = (vma_t *)kmem_calloc(sizeof(vma_t), 1);
  if (!vma) return -1;

  vma->start = start;
  vma->end = end;
  vma->prot = prot;
  vma->flags = flags;
  vma->gem_handle = gem_handle;
  vma->object_offset = object_offset;
  vma->object = object;
  vm_object_ref(object);

  vma_t **pp = &proc->vma_list;
  while (*pp && (*pp)->start < start) {
    pp = &(*pp)->next;
  }
  vma->next = *pp;
  *pp = vma;

  return 0;
}

int vm_map_remove(process_t *proc, u64 addr) {
  vma_t **pp = &proc->vma_list;
  while (*pp) {
    if (addr >= (*pp)->start && addr < (*pp)->end) {
      vma_t *v = *pp;
      *pp = v->next;
      vm_object_unref(v->object);
      kmem_free(v);
      return 0;
    }
    pp = &(*pp)->next;
  }
  return -1;
}

vma_t *vm_map_lookup(process_t *proc, u64 addr) {
  for (vma_t *v = proc->vma_list; v; v = v->next) {
    if (addr >= v->start && addr < v->end) {
      return v;
    }
  }
  return NULL;
}

void vm_map_free_all(process_t *proc) {
  vma_t *v = proc->vma_list;
  while (v) {
    vma_t *next = v->next;
    for (u64 va = v->start; va < v->end; va += PAGE_SIZE) {
      if (v->object == NULL) {
        u64 phys = pmap_extract(va);
        if (phys)
          vm_page_free_phys(phys);
      }
      pmap_remove(va);
    }
    vm_object_unref(v->object);
    kmem_free(v);
    v = next;
  }
  proc->vma_list = NULL;
}

int vm_map_fork(process_t *parent, process_t *child) {
  vma_t **child_tail = &child->vma_list;

  child->vma_list = NULL;

  for (vma_t *v = parent->vma_list; v; v = v->next) {
    vma_t *child_vma = (vma_t *)kmem_calloc(sizeof(vma_t), 1);
    if (!child_vma) {
      vm_map_free_all(child);
      return -1;
    }
    child_vma->start = v->start;
    child_vma->end = v->end;
    child_vma->prot = v->prot;
    child_vma->flags = v->flags;
    child_vma->gem_handle = v->gem_handle;
    child_vma->object_offset = v->object_offset;

    if (v->object) {
      if (v->flags & API_MAP_GEM) {
        child_vma->object = v->object;
        vm_object_ref(v->object);
      } else {
        vm_object_t *child_shadow = vm_object_create_shadow(v->object);
        if (!child_shadow) {
          kmem_free(child_vma);
          vm_map_free_all(child);
          return -1;
        }

        vm_object_t *parent_shadow = vm_object_create_shadow(v->object);
        if (!parent_shadow) {
          vm_object_unref(child_shadow);
          kmem_free(child_vma);
          vm_map_free_all(child);
          return -1;
        }

        vm_object_unref(v->object);
        v->object = parent_shadow;

        child_vma->object = child_shadow;
      }
    } else {
      child_vma->object = NULL;
    }

    child_vma->next = NULL;
    *child_tail = child_vma;
    child_tail = &child_vma->next;
  }

  return 0;
}

int vm_map_fault(process_t *proc, u64 addr, u64 err_code) {
  vma_t *v = vm_map_lookup(proc, addr);
  if (!v || !v->object) {
    return -1;
  }

  if ((err_code & 0x2) && !(v->prot & API_MAP_WRITE)) {
    return -1;
  }
  if ((err_code & 0x10) && !(v->prot & API_MAP_EXEC)) {
    return -1;
  }

  u64 page_va = addr & ~(PAGE_SIZE - 1);
  u64 map_off = page_va - v->start;
  u64 file_off = v->object_offset + map_off;
  u64 index = map_off / PAGE_SIZE;

  if ((err_code & 0x1) && (err_code & 0x2) && (v->prot & API_MAP_WRITE)) {
    u64 old_phys = vm_object_find_page(v->object, index);
    if (old_phys == 0)
      old_phys = pmap_extract(page_va);
    if (old_phys == 0)
      return -1;

    u32 ref = vm_page_ref_count(old_phys);
    if (ref > 1) {
      u64 new_phys = vm_page_alloc_phys(0);
      if (new_phys == 0)
        return -1;
      memcpy((void *)new_phys, (void *)(old_phys & ~0xFFF), PAGE_SIZE);
      vm_page_free_phys(old_phys);
      vm_object_set_page(v->object, index, new_phys);
      pmap_enter(page_va, new_phys, page_flags_for_prot(v->prot));
    } else {
      pmap_enter(page_va, old_phys, page_flags_for_prot(v->prot));
    }
    return 0;
  }

  u64 phys = vm_object_get_page(v->object, index, file_off);
  if (!phys) {
    return -1;
  }

  pmap_enter(page_va, phys, page_flags_for_prot(v->prot));
  return 0;
}

int vm_cow_fault(u64 addr, u64 err_code) {
  if (!(err_code & 0x1) || !(err_code & 0x2))
    return -1;

  u64 page_va = addr & ~(PAGE_SIZE - 1);
  u64 flags = pmap_extract_flags(page_va);
  if (!(flags & PTE_COW))
    return -1;

  u64 phys = pmap_extract(page_va);
  if (phys == 0)
    return -1;

  u64 new_flags = ((flags | PTE_RW) & ~PTE_COW) | PTE_PRESENT;
  u32 ref = vm_page_ref_count(phys);
  if (ref > 1) {
    u64 new_phys = vm_page_alloc_phys(0);
    if (new_phys == 0)
      return -1;
    memcpy((void *)new_phys, (void *)(phys & ~0xFFF), PAGE_SIZE);
    vm_page_free_phys(phys);
    pmap_enter(page_va, new_phys, new_flags);
  } else {
    pmap_enter(page_va, phys, new_flags);
  }
  return 0;
}
