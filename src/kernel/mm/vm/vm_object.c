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
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <lib/com1.h>

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

    kmem_free(obj);
}

u32
vm_object_type(vm_object_t *obj)
{
    if (obj == NULL)
        return 0;
    return obj->type;
}
