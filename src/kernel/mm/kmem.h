/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license — see individual source files for full text.
 */

#ifndef KMEM_H
#define KMEM_H

#include <mlibc/mlibc.h>

#define KMEM_HEAP_SIZE     (8 * 1024 * 1024)
#define KMEM_REDZONE_SIZE  16
#define KMEM_REDZONE_BYTE  0xCC
#define KMEM_POISON_BYTE   0xAA
#define KMEM_ALIGN         16

void  kmem_init(void);
void *kmem_alloc(unsigned long size);
void  kmem_free(void *ptr);
void *kmem_calloc(unsigned long nmemb, unsigned long size);
void *kmem_realloc(void *ptr, unsigned long size);
void *kmem_alloc_aligned(unsigned long size, unsigned long align);
unsigned long kmem_usable_size(void *ptr);
unsigned long kmem_free_bytes(void);
int  kmem_is_initialized(void);
void kmem_dump(void);

unsigned long kmem_total_bytes(void);
unsigned long kmem_used_bytes(void);

#endif
