/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#ifndef VM_MAP_H
#define VM_MAP_H

#include <mlibc/mlibc.h>
#include <kernel/process.h>

#define VM_MAP_READ   0x1
#define VM_MAP_WRITE  0x2
#define VM_MAP_EXEC   0x4

#define VM_MAP_PRIVATE 0x02
#define VM_MAP_FIXED   0x10
#define VM_MAP_ANON    0x20
#define VM_MAP_GEM     0x40

/* The vma_t structure is still defined in process.h for now.
 * These functions operate on the vma_list inside process_t. */

u64 vm_map_find_free(process_t *proc, u64 length);
int vm_map_insert(process_t *proc, u64 start, u64 end, u32 prot, u32 flags,
                  u32 gem_handle);
int vm_map_remove(process_t *proc, u64 addr);
vma_t *vm_map_lookup(process_t *proc, u64 addr);
void vm_map_free_all(process_t *proc);
int vm_map_copy(process_t *dst, const process_t *src);

#endif
