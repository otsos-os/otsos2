/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#ifndef VM_PAGER_H
#define VM_PAGER_H

#include <mlibc/mlibc.h>

#define VM_PAGER_DEFAULT  0x01
#define VM_PAGER_VNODE    0x02
#define VM_PAGER_DEVICE   0x04

#define VM_PAGER_PATH_MAX 256

typedef struct vm_pager {
    u32  type;
    u64  size;
    void *handle;
    char path[VM_PAGER_PATH_MAX];
    int  (*getpage)(struct vm_pager *pager, u64 offset, u64 *out_phys);
    int  (*putpage)(struct vm_pager *pager, u64 offset, u64 phys);
    int  (*haspage)(struct vm_pager *pager, u64 offset);
} vm_pager_t;

vm_pager_t *vm_pager_create_default(u64 size);
vm_pager_t *vm_pager_create_vnode(const char *path, u64 size);
vm_pager_t *vm_pager_create_device(void *data, u64 size);
void vm_pager_destroy(vm_pager_t *pager);

#endif
