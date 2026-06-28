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

#include <kernel/api/posix/posix.h>
#include <kernel/api/api.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/video/drm/gem.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_object.h>
#include <mm/vm/vm_map.h>

#define PAGE_SIZE	4096

static u64
align_up(u64 val, u64 align)
{
	return ((val + align - 1) & ~(align - 1));
}

static u64
page_flags_for_prot(u64 prot)
{
	u64	flags;

	flags = PTE_PRESENT | PTE_USER;
	if (prot & POSIX_PROT_WRITE) {
		flags |= PTE_RW;
	}
	if (!(prot & POSIX_PROT_EXEC)) {
		flags |= PTE_NX;
	}
	return (flags);
}

s64
posix_mmap(u64 addr_u, u64 length, u64 prot, u64 flags, u64 fd_u,
    u64 offset, registers_t *regs)
{
	struct process	*proc;
	u64		aligned;
	u64		addr;

	(void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	if (length == 0) {
		return (-POSIX_EINVAL);
	}

	aligned = align_up(length, PAGE_SIZE);
	addr = addr_u;

	if (flags & POSIX_MAP_FIXED) {
		if (addr == 0 || (addr & (PAGE_SIZE - 1)) != 0) {
			return (-POSIX_EINVAL);
		}
	} else {
		addr = vm_map_find_free(proc, aligned);
		if (!addr) {
			return (-POSIX_ENOMEM);
		}
	}

	if (flags & POSIX_MAP_ANON) {
		u32		api_prot;
		u32		api_flags;
		vm_object_t	*obj;

		api_prot = 0;
		if (prot & POSIX_PROT_READ) api_prot |= API_MAP_READ;
		if (prot & POSIX_PROT_WRITE) api_prot |= API_MAP_WRITE;
		if (prot & POSIX_PROT_EXEC) api_prot |= API_MAP_EXEC;

		api_flags = API_MAP_ANON | API_MAP_PRIVATE;
		if (flags & POSIX_MAP_FIXED) api_flags |= API_MAP_FIXED;

		obj = vm_object_create(VM_OBJ_ANON, aligned, NULL);
		if (!obj) {
			return (-POSIX_ENOMEM);
		}

		if (vm_map_insert(proc, addr, addr + aligned, api_prot,
		    api_flags, 0, obj, 0) != 0) {
			vm_object_unref(obj);
			return (-POSIX_ENOMEM);
		}
		vm_object_unref(obj);

		return ((s64)addr);
	}

	{
		posix_fd_t	*pfd;
		vnode_t		*vn;
		chainfs_file_entry_t	entry;
		u32			entry_block, entry_offset;

		pfd = posix_get_fd(proc, (int)fd_u);
		if (!pfd || !pfd->vnode) {
			return (-POSIX_EBADF);
		}

		vn = pfd->vnode;
		if (vn->type == VCHR) {
			return (-POSIX_ENODEV);
		}

		char	*path;
		path = (char *)vn->data;
		if (!path) {
			return (-POSIX_EINVAL);
		}

		if (chainfs_find_file(path, &entry, &entry_block,
		    &entry_offset) != 0) {
			return (-POSIX_ENOENT);
		}

		u32		api_prot;
		u32		api_flags;
		vm_object_t	*obj;

		api_prot = 0;
		if (prot & POSIX_PROT_READ) api_prot |= API_MAP_READ;
		if (prot & POSIX_PROT_WRITE) api_prot |= API_MAP_WRITE;
		if (prot & POSIX_PROT_EXEC) api_prot |= API_MAP_EXEC;

		api_flags = API_MAP_PRIVATE;
		if (flags & POSIX_MAP_FIXED) api_flags |= API_MAP_FIXED;

		obj = vm_object_create(VM_OBJ_FILE, aligned,
		    (void *)path);
		if (!obj) {
			return (-POSIX_ENOMEM);
		}

		if (vm_map_insert(proc, addr, addr + aligned, api_prot,
		    api_flags, 0, obj, offset) != 0) {
			vm_object_unref(obj);
			return (-POSIX_ENOMEM);
		}
		vm_object_unref(obj);

		return ((s64)addr);
	}
}

s64
posix_munmap(u64 addr_u, u64 length, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	u64		vaddr;
	u64		aligned;
	vma_t		*vma;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc || length == 0) {
		return (-POSIX_EINVAL);
	}

	vaddr = addr_u;
	if ((vaddr & (PAGE_SIZE - 1)) != 0) {
		return (-POSIX_EINVAL);
	}

	if (!is_user_address((void *)vaddr, length)) {
		return (-POSIX_EFAULT);
	}

	vma = vm_map_lookup(proc, vaddr);
	if (!vma) {
		return (-POSIX_EINVAL);
	}

	aligned = align_up(length, PAGE_SIZE);

	{
		u64	off;
		for (off = 0; off < aligned && vaddr + off < vma->end;
		    off += PAGE_SIZE) {
			u64	va;
			va = vaddr + off;
			if (vma->object == NULL) {
				u64	phys;
				phys = pmap_extract(va);
				if (phys) {
					vm_page_free_phys(phys);
				}
			}
			pmap_remove(va);
		}
	}

	vm_map_remove(proc, vaddr);
	return (0);
}

s64
posix_mprotect(u64 addr_u, u64 length, u64 prot, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	u64		vaddr;
	u64		aligned;
	vma_t		*vma;
	u64		pflags;
	u64		off;

	(void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc || length == 0) {
		return (-POSIX_EINVAL);
	}

	vaddr = addr_u;
	if ((vaddr & (PAGE_SIZE - 1)) != 0) {
		return (-POSIX_EINVAL);
	}

	aligned = align_up(length, PAGE_SIZE);
	vma = vm_map_lookup(proc, vaddr);
	if (!vma) {
		return (-POSIX_ENOMEM);
	}

	pflags = page_flags_for_prot(prot);

	for (off = 0; off < aligned && vaddr + off < vma->end;
	    off += PAGE_SIZE) {
		u64	va;
		u64	phys;
		va = vaddr + off;
		phys = pmap_extract(va);
		if (phys) {
			pmap_remove(va);
			pmap_enter(va, phys, pflags);
		}
	}

	vma->prot = 0;
	if (prot & POSIX_PROT_READ) vma->prot |= API_MAP_READ;
	if (prot & POSIX_PROT_WRITE) vma->prot |= API_MAP_WRITE;
	if (prot & POSIX_PROT_EXEC) vma->prot |= API_MAP_EXEC;

	return (0);
}

s64
posix_brk(u64 addr_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	if (addr_u == 0) {
		if (proc->brk == 0) {
			proc->brk = MMAP_BASE;
		}
		return ((s64)proc->brk);
	}

	proc->brk = addr_u;
	return ((s64)proc->brk);
}
