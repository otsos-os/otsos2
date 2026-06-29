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

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type vm_pager_t as struct with type, size, handle, path, getpage, putpage, haspage

$define %func default_getpage as function with args vm_pager_t *, u64, u64 *
$define %func default_putpage as function with args vm_pager_t *, u64, u64
$define %func default_haspage as function with args vm_pager_t *, u64
$define %func vnode_getpage as function with args vm_pager_t *, u64, u64 *
$define %func vnode_putpage as function with args vm_pager_t *, u64, u64
$define %func vnode_haspage as function with args vm_pager_t *, u64
$define %func device_getpage as function with args vm_pager_t *, u64, u64 *
$define %func device_putpage as function with args vm_pager_t *, u64, u64
$define %func device_haspage as function with args vm_pager_t *, u64
$define %func vm_pager_create_default as function with args u64
$define %func vm_pager_create_vnode as function with args const char *, u64
$define %func vm_pager_create_device as function with args void *, u64
$define %func vm_pager_destroy as procedure with args vm_pager_t *

*/

/* !SPACE!

$space %internal default_getpage, default_putpage, default_haspage
$space %internal vnode_getpage, vnode_putpage, vnode_haspage
$space %internal device_getpage, device_putpage, device_haspage
$space %export vm_pager_create_default, vm_pager_create_vnode
$space %export vm_pager_create_device, vm_pager_destroy

*/

#include <mm/vm/vm_pager.h>
#include <mm/vm/vm_page.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <kernel/drivers/fs/vfs/vfs.h>

#define PAGE_SIZE 4096

static int
default_getpage(vm_pager_t *pager, u64 offset, u64 *out_phys)
{
	u64 phys;

	phys = vm_page_alloc_phys(0);
	if (phys == 0) {
		return (-1);
	}
	memset((void *)phys, 0, PAGE_SIZE);
	*out_phys = phys;
	return (0);
}

static int
default_putpage(vm_pager_t *pager, u64 offset, u64 phys)
{
	return (0);
}

static int
default_haspage(vm_pager_t *pager, u64 offset)
{
	return (0);
}

vm_pager_t *
vm_pager_create_default(u64 size)
{
	vm_pager_t *p;

	p = kmem_calloc(1, sizeof(vm_pager_t));
	if (p == NULL) {
		return (NULL);
	}
	p->type = VM_PAGER_DEFAULT;
	p->size = size;
	p->getpage = default_getpage;
	p->putpage = default_putpage;
	p->haspage = default_haspage;
	return (p);
}

static int
vnode_getpage(vm_pager_t *pager, u64 offset, u64 *out_phys)
{
	u64	phys;
	vnode_t	*vn;
	int	n;

	phys = vm_page_alloc_phys(0);
	if (phys == 0) {
		return (-1);
	}
	memset((void *)phys, 0, PAGE_SIZE);
	if (pager->path[0] != '\0') {
		vn = NULL;
		if (vfs_resolve(pager->path, &vn) == 0 && vn != NULL) {
			n = vnode_read(vn, (void *)phys, PAGE_SIZE,
			    offset);
			vnode_release(vn);
			(void)n;
		}
	}
	*out_phys = phys;
	return (0);
}

static int
vnode_putpage(vm_pager_t *pager, u64 offset, u64 phys)
{
	return (0);
}

static int
vnode_haspage(vm_pager_t *pager, u64 offset)
{
	return (1);
}

vm_pager_t *
vm_pager_create_vnode(const char *path, u64 size)
{
	vm_pager_t *p;
	u32 i;

	p = kmem_calloc(1, sizeof(vm_pager_t));
	if (p == NULL) {
		return (NULL);
	}
	p->type = VM_PAGER_VNODE;
	p->size = size;
	if (path != NULL) {
		for (i = 0; i < VM_PAGER_PATH_MAX - 1 && path[i]; i++) {
			p->path[i] = path[i];
		}
		p->path[i] = '\0';
		p->handle = p->path;
	}
	p->getpage = vnode_getpage;
	p->putpage = vnode_putpage;
	p->haspage = vnode_haspage;
	return (p);
}

static int
device_getpage(vm_pager_t *pager, u64 offset, u64 *out_phys)
{
	*out_phys = (u64)pager->handle + offset;
	return (0);
}

static int
device_putpage(vm_pager_t *pager, u64 offset, u64 phys)
{
	return (0);
}

static int
device_haspage(vm_pager_t *pager, u64 offset)
{
	return (1);
}

vm_pager_t *
vm_pager_create_device(void *data, u64 size)
{
	vm_pager_t *p;

	p = kmem_calloc(1, sizeof(vm_pager_t));
	if (p == NULL) {
		return (NULL);
	}
	p->type = VM_PAGER_DEVICE;
	p->size = size;
	p->handle = data;
	p->getpage = device_getpage;
	p->putpage = device_putpage;
	p->haspage = device_haspage;
	return (p);
}

void
vm_pager_destroy(vm_pager_t *pager)
{
	if (pager == NULL) {
		return;
	}
	kmem_free(pager);
}
