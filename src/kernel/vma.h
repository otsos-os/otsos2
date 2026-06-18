/*
 * Copyright (c) 2026, otsos team
 */

#ifndef KERNEL_VMA_H
#define KERNEL_VMA_H

#include <kernel/process.h>

/* Find a free virtual address range of `length` bytes (page-aligned) in the
 * process's mmap region. Uses the VMA list, not page-table walks. */
u64 vma_find_free(process_t *proc, u64 length);

/* Register a new VMA. Returns 0 on success, negative on failure. */
int vma_add(process_t *proc, u64 start, u64 end, u32 prot, u32 flags,
            u32 gem_handle);

/* Remove and free the VMA covering `addr`. Returns 0 on success. Does NOT
 * unmap pages — caller's responsibility. */
int vma_remove(process_t *proc, u64 addr);

/* Find the VMA containing `addr`, or NULL. */
vma_t *vma_find(process_t *proc, u64 addr);

/* Free all VMAs for a process (used on exit). Does NOT unmap pages. */
void vma_free_all(process_t *proc);

/* Deep-copy the VMA list from src to dst (used on fork). */
int vma_copy(process_t *dst, const process_t *src);

#endif
