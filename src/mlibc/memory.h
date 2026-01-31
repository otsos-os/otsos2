#ifndef MEMORY_H
#define MEMORY_H

void *kmalloc(unsigned long size);
void kfree(void *ptr);
void *kcalloc(unsigned long nmemb, unsigned long size);
void *krealloc(void *ptr, unsigned long size);
void *kmalloc_aligned(unsigned long size, unsigned long align);
unsigned long kmalloc_usable_size(void *ptr);
unsigned long kget_free_memory();
void kheap_dump();
void init_heap();

#endif
