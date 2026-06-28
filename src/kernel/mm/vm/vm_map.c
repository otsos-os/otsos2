/*
 * Copyright (c) 2026, otsos team
 */

#include <mm/vm/vm_map.h>
#include <kernel/api/api.h>
#include <mm/kmem.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mlibc/mlibc.h>

#define PAGE_SIZE 4096

static u64 align_up(u64 val, u64 align) {
  return (val + align - 1) & ~(align - 1);
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

  /* Wrap around. */
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

  /* Insert sorted by start address. */
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

int vm_map_copy(process_t *dst, const process_t *src) {
  dst->vma_list = NULL;
  vma_t **tail = &dst->vma_list;

  for (vma_t *v = src->vma_list; v; v = v->next) {
    vma_t *copy = (vma_t *)kmem_calloc(sizeof(vma_t), 1);
    if (!copy) {
      vm_map_free_all(dst);
      return -1;
    }
    copy->start = v->start;
    copy->end = v->end;
    copy->prot = v->prot;
    copy->flags = v->flags;
    copy->gem_handle = v->gem_handle;
    copy->object_offset = v->object_offset;
    copy->object = NULL;
    copy->next = NULL;
    *tail = copy;
    tail = &copy->next;
  }

  return 0;
}
