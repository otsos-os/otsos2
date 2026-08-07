/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#ifndef BOOTMEM_H
#define BOOTMEM_H

#include <mlibc/mlibc.h>

typedef struct {
  u64 start;
  u64 end;
} bootmem_range_t;

void bootmem_init(u64 magic, u64 info_addr, u64 kernel_start, u64 kernel_end);
void *bootmem_alloc(u64 size, u64 align);
u64 bootmem_free_bytes(void);
u64 bootmem_highest_addr(void);
u32 bootmem_range_count(void);
const bootmem_range_t *bootmem_ranges(void);
void bootmem_dump(void);
void bootmem_set_reserve_cb(void (*cb)(u64 phys_start, u64 size));
void bootmem_reserve_phys(u64 phys_start, u64 size);

#endif
