/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#ifndef VM_OBJECT_H
#define VM_OBJECT_H

#include <mlibc/mlibc.h>

#define VM_OBJ_ANON     0x01
#define VM_OBJ_FILE     0x02
#define VM_OBJ_GEM      0x04
#define VM_OBJ_SHADOW   0x08

struct vm_pager;

typedef struct vm_object {
  u32  type;
  u32  ref_count;
  u64  size;
  u64 *pages;
  u64  page_count;
  struct vm_object *shadow;
  struct vm_pager  *pager;
  struct vm_object *next;
} vm_object_t;

vm_object_t *vm_object_create(u32 type, u64 size, void *backing);
vm_object_t *vm_object_create_shadow(vm_object_t *backing);
void vm_object_ref(vm_object_t *obj);
void vm_object_unref(vm_object_t *obj);
u32 vm_object_type(vm_object_t *obj);
u64 vm_object_page(vm_object_t *obj, u64 index);
u64 vm_object_find_page(vm_object_t *obj, u64 index);
int vm_object_set_page(vm_object_t *obj, u64 index, u64 phys);
u64 vm_object_get_page(vm_object_t *obj, u64 index, u64 file_offset);

void vm_object_init(void);

#endif
