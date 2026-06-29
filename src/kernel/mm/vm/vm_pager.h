/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type vm_pager_t as struct with type, size, handle, path, getpage, putpage, haspage

$define %func vm_pager_create_default as function with args u64
$define %func vm_pager_create_vnode as function with args const char *, u64
$define %func vm_pager_create_device as function with args void *, u64
$define %func vm_pager_destroy as procedure with args vm_pager_t *

*/

/* !SPACE!

$space %export vm_pager_create_default, vm_pager_create_vnode
$space %export vm_pager_create_device, vm_pager_destroy

*/

#ifndef VM_PAGER_H
#define VM_PAGER_H

#include <mlibc/mlibc.h>

#define VM_PAGER_DEFAULT	0x01
#define VM_PAGER_VNODE		0x02
#define VM_PAGER_DEVICE		0x04

#define VM_PAGER_PATH_MAX	256

typedef struct vm_pager {
	u32	type;
	u64	size;
	void	*handle;
	char	path[VM_PAGER_PATH_MAX];
	int	(*getpage)(struct vm_pager *pager, u64 offset,
		    u64 *out_phys);
	int	(*putpage)(struct vm_pager *pager, u64 offset,
		    u64 phys);
	int	(*haspage)(struct vm_pager *pager, u64 offset);
} vm_pager_t;

vm_pager_t	*vm_pager_create_default(u64 size);
vm_pager_t	*vm_pager_create_vnode(const char *path, u64 size);
vm_pager_t	*vm_pager_create_device(void *data, u64 size);
void		vm_pager_destroy(vm_pager_t *pager);

#endif
