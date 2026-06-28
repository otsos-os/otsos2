/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <mm/vm/vm_object.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <mm/kmem.h>
#include <mm/vm/vm_page.h>
#include <mlibc/mlibc.h>
#include <lib/com1.h>

#define PAGE_SIZE 4096

static vm_object_t *vm_object_list = NULL;

void
vm_object_init(void)
{
    vm_object_list = NULL;
    com1_printf("[vm_object] initialized\n");
}

vm_object_t *
vm_object_create(u32 type, u64 size, void *backing)
{
    vm_object_t *obj;

    obj = kmem_calloc(1, sizeof(vm_object_t));
    if (obj == NULL)
        return NULL;

    obj->type = type;
    obj->ref_count = 1;
    obj->size = size;
    obj->backing = backing;
    if (type == VM_OBJ_FILE && backing != NULL) {
        const char *path = (const char *)backing;
        u32 i;
        for (i = 0; i < VM_OBJECT_BACKING_MAX - 1 && path[i]; i++)
            obj->backing_path[i] = path[i];
        obj->backing_path[i] = '\0';
        obj->backing = obj->backing_path;
    }
    obj->page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (obj->page_count != 0) {
        obj->pages = kmem_calloc(obj->page_count, sizeof(u64));
        if (obj->pages == NULL) {
            kmem_free(obj);
            return NULL;
        }
    }
    obj->next = vm_object_list;
    vm_object_list = obj;

    return obj;
}

void
vm_object_ref(vm_object_t *obj)
{
    if (obj == NULL)
        return;
    obj->ref_count++;
}

void
vm_object_unref(vm_object_t *obj)
{
    vm_object_t *cur;
    vm_object_t *prev;

    if (obj == NULL)
        return;

    obj->ref_count--;

    if (obj->ref_count > 0)
        return;

    prev = NULL;
    cur = vm_object_list;
    while (cur != NULL) {
        if (cur == obj) {
            if (prev != NULL)
                prev->next = cur->next;
            else
                vm_object_list = cur->next;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    if (obj->pages != NULL) {
        for (u64 i = 0; i < obj->page_count; i++) {
            if (obj->pages[i] != 0)
                vm_page_free_phys(obj->pages[i]);
        }
        kmem_free(obj->pages);
    }

    kmem_free(obj);
}

u32
vm_object_type(vm_object_t *obj)
{
    if (obj == NULL)
        return 0;
    return obj->type;
}

u64
vm_object_page(vm_object_t *obj, u64 index)
{
    if (obj == NULL || index >= obj->page_count)
        return 0;
    return obj->pages[index];
}

int
vm_object_set_page(vm_object_t *obj, u64 index, u64 phys)
{
    if (obj == NULL || index >= obj->page_count)
        return -1;
    obj->pages[index] = phys;
    return 0;
}

u64
vm_object_get_page(vm_object_t *obj, u64 index, u64 file_offset)
{
    u64 phys;

    if (obj == NULL || index >= obj->page_count)
        return 0;

    if (obj->pages[index] != 0)
        return obj->pages[index];

    if (obj->type == VM_OBJ_GEM)
        return 0;

    phys = vm_page_alloc_phys(0);
    if (phys == 0)
        return 0;

    memset((void *)phys, 0, PAGE_SIZE);

    if (obj->type == VM_OBJ_FILE && obj->backing != NULL) {
        u32 bytes_read = 0;
        chainfs_read_file_range((const char *)obj->backing, (u8 *)phys,
                                PAGE_SIZE, (u32)file_offset, &bytes_read);
    }

    obj->pages[index] = phys;
    return phys;
}
