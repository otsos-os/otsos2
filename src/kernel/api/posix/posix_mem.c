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
#include <kernel/api/shm.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/video/drm/fbdev.h>
#include <kernel/drivers/video/drm/gem.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
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

static s64
posix_mmap_fbdev(struct process *proc, vnode_t *vn, u64 addr, u64 length,
    u64 prot, u64 flags, u64 offset)
{
	drm_fbdev_info_t	info;
	vm_object_t		*obj;
	u64			map_size;
	u32			api_prot;
	u32			api_flags;

	(void)vn;
	if (!proc_has_privilege(proc)) {
		return (-POSIX_EACCES);
	}
	if ((flags & POSIX_MAP_SHARED) == 0) {
		return (-POSIX_EINVAL);
	}
	if ((offset & (PAGE_SIZE - 1)) != 0) {
		return (-POSIX_EINVAL);
	}
	if (drm_fbdev_get_info(&info) != 0) {
		return (-POSIX_ENODEV);
	}
	if ((info.hw_address & (PAGE_SIZE - 1)) != 0) {
		return (-POSIX_ENODEV);
	}
	if (offset >= info.size || length > info.size - offset) {
		return (-POSIX_EINVAL);
	}

	map_size = align_up(length, PAGE_SIZE);
	api_prot = 0;
	if (prot & POSIX_PROT_READ) api_prot |= API_MAP_READ;
	if (prot & POSIX_PROT_WRITE) api_prot |= API_MAP_WRITE;
	if (prot & POSIX_PROT_EXEC) api_prot |= API_MAP_EXEC;

	api_flags = API_MAP_SHARED;
	if (flags & POSIX_MAP_FIXED) api_flags |= API_MAP_FIXED;

	obj = vm_object_create(VM_OBJ_DEVICE, map_size,
	    (void *)(info.hw_address + offset));
	if (!obj) {
		return (-POSIX_ENOMEM);
	}
	if (vm_map_insert(proc, addr, addr + map_size, api_prot,
	    api_flags, 0, obj, 0) != 0) {
		vm_object_unref(obj);
		return (-POSIX_ENOMEM);
	}
	vm_object_unref(obj);
	return ((s64)addr);
}

struct posix_ipc_perm {
	s32		key;
	u32		uid;
	u32		gid;
	u32		cuid;
	u32		cgid;
	u32		mode;
	s32		seq;
	s64		pad1;
	s64		pad2;
};

struct posix_shmid_ds {
	struct posix_ipc_perm	shm_perm;
	u64			shm_segsz;
	s64			shm_atime;
	s64			shm_dtime;
	s64			shm_ctime;
	s32			shm_cpid;
	s32			shm_lpid;
	u64			shm_nattch;
	u64			pad1;
	u64			pad2;
};

static s64
posix_errno_from_api(int error)
{
	if (error > 0) {
		error = -error;
	}

	switch (-error) {
	case API_ERR_NOT_FOUND:
		return (-POSIX_ENOENT);
	case API_ERR_EXISTS:
		return (-POSIX_EEXIST);
	case API_ERR_NO_MEMORY:
	case API_ERR_NOMEM:
		return (-POSIX_ENOMEM);
	case API_ERR_BAD_ADDR:
		return (-POSIX_EFAULT);
	case API_ERR_PERM:
	case API_ERR_ACCESS:
		return (-POSIX_EACCES);
	case API_ERR_BAD_VALUE:
	case API_ERR_INVAL:
		return (-POSIX_EINVAL);
	default:
		return (-POSIX_EINVAL);
	}
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
		posix_stat_t	st;

		pfd = posix_get_fd(proc, (int)fd_u);
		if (!pfd || !pfd->vnode) {
			return (-POSIX_EBADF);
		}

		vn = pfd->vnode;
		if (vn->type == VCHR) {
			if (strcmp(vn->name, "fb0") == 0) {
				return (posix_mmap_fbdev(proc, vn, addr,
				    length, prot, flags, offset));
			}
			return (-POSIX_ENODEV);
		}

		if (vnode_stat(vn, &st) != 0) {
			return (-POSIX_EIO);
		}

		char	*path;
		path = (char *)vn->data;
		if (!path) {
			return (-POSIX_EINVAL);
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
posix_shmget(u64 key, u64 size, u64 shmflg, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	shm_segment_t	*seg;
	int		error;

	(void)a4; (void)a5; (void)a6; (void)regs;

	seg = shm_get_or_create(key, size, (int)shmflg, &error);
	if (seg == NULL) {
		return (posix_errno_from_api(error));
	}

	error = seg->id;
	shm_put(seg);
	return ((s64)error);
}

s64
posix_shmat(u64 shmid, u64 shmaddr, u64 shmflg, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	shm_segment_t	*seg;
	u64		addr;
	u32		prot;
	u32		flags;

	(void)a4; (void)a5; (void)a6; (void)regs;

	seg = shm_get((int)shmid);
	if (seg == NULL) {
		return (-POSIX_EINVAL);
	}

	addr = shmaddr;
	if ((shmflg & POSIX_SHM_RND) && addr != 0) {
		addr &= ~((u64)PAGE_SIZE - 1);
	}

	prot = API_MAP_READ;
	if (!(shmflg & POSIX_SHM_RDONLY)) {
		prot |= API_MAP_WRITE;
	}
	if (shmflg & POSIX_SHM_EXEC) {
		prot |= API_MAP_EXEC;
	}

	flags = API_MAP_SHARED;
	if (addr != 0) {
		flags |= API_MAP_FIXED;
	}

	addr = shm_map(seg, addr, seg->size, prot, flags);
	shm_put(seg);
	if ((s64)addr < 0) {
		return (posix_errno_from_api((int)addr));
	}
	return ((s64)addr);
}

s64
posix_shmdt(u64 shmaddr, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	vma_t		*vma;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (proc == NULL || shmaddr == 0) {
		return (-POSIX_EINVAL);
	}
	if ((shmaddr & (PAGE_SIZE - 1)) != 0) {
		return (-POSIX_EINVAL);
	}

	vma = vm_map_lookup(proc, shmaddr);
	if (vma == NULL || !(vma->flags & API_MAP_SHARED) ||
	    vma->start != shmaddr) {
		return (-POSIX_EINVAL);
	}

	return (posix_munmap(shmaddr, vma->end - vma->start, 0, 0, 0, 0,
	    regs));
}

s64
posix_shmctl(u64 shmid, u64 cmd, u64 buf, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct posix_shmid_ds	ds;
	struct api_shminfo_args	info;
	int			ret;

	(void)a4; (void)a5; (void)a6; (void)regs;

	if ((int)cmd == POSIX_IPC_RMID) {
		ret = shm_remove((int)shmid);
		if (ret != 0) {
			return (posix_errno_from_api(ret));
		}
		return (0);
	}

	if ((int)cmd == POSIX_IPC_SET) {
		return (-POSIX_EPERM);
	}
	if ((int)cmd != POSIX_IPC_STAT) {
		return (-POSIX_EINVAL);
	}
	if (!is_user_address((void *)buf, sizeof(ds))) {
		return (-POSIX_EFAULT);
	}

	ret = shm_info((int)shmid, &info);
	if (ret != 0) {
		return (posix_errno_from_api(ret));
	}

	memset(&ds, 0, sizeof(ds));
	ds.shm_perm.key = (s32)info.key;
	ds.shm_perm.uid = 0;
	ds.shm_perm.gid = 0;
	ds.shm_perm.cuid = 0;
	ds.shm_perm.cgid = 0;
	ds.shm_perm.mode = info.mode;
	ds.shm_segsz = info.size;
	ds.shm_nattch = info.refs;
	memcpy((void *)buf, &ds, sizeof(ds));
	return (0);
}

s64
posix_brk(u64 addr_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	vma_t		*vma;
	u64		old_brk;
	u64		new_brk;
	u64		va;
	u64		phys;
	u64		obj_size;
	u64		idx;
	u64		p;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	if (addr_u == 0) {
		return ((s64)proc->brk);
	}

	old_brk = proc->brk;
	new_brk = (addr_u + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	if (new_brk < proc->brk_min) {
		new_brk = proc->brk_min;
	}
	if (new_brk == old_brk) {
		return ((s64)proc->brk);
	}

	vma = NULL;
	if (old_brk > proc->brk_min) {
		vma = vm_map_lookup(proc, old_brk - 1);
	}
	if (vma == NULL && new_brk > proc->brk_min) {
		vma = vm_map_lookup(proc, new_brk - 1);
	}

	if (vma == NULL) {
		/* No data/BSS vma; fallback to direct page mapping. */
		if (new_brk > old_brk) {
			for (va = old_brk; va < new_brk; va += PAGE_SIZE) {
				phys = vm_page_alloc_phys(0);
				if (phys == 0) {
					for (va = old_brk; va < new_brk;
					    va += PAGE_SIZE) {
						p = pmap_extract(va);
						if (p) {
							pmap_remove(va);
							vm_page_free_phys(p);
						}
					}
					return (-POSIX_ENOMEM);
				}
				memset((void *)phys, 0, PAGE_SIZE);
				pmap_enter(va, phys,
				    PTE_PRESENT | PTE_RW | PTE_USER);
			}
		} else {
			for (va = new_brk; va < old_brk; va += PAGE_SIZE) {
				p = pmap_extract(va);
				if (p) {
					pmap_remove(va);
					vm_page_free_phys(p);
				}
			}
		}
		proc->brk = new_brk;
		return ((s64)proc->brk);
	}

	obj_size = new_brk - vma->start;
	if (vm_object_resize(vma->object, obj_size) != 0) {
		return (-POSIX_ENOMEM);
	}
	vma->end = new_brk;

	if (new_brk > old_brk) {
		for (va = old_brk; va < new_brk; va += PAGE_SIZE) {
			idx = (va - vma->start) / PAGE_SIZE;
			phys = vm_object_get_page(vma->object, idx, 0);
			if (phys == 0) {
				vm_object_resize(vma->object,
				    old_brk - vma->start);
				vma->end = old_brk;
				for (va = old_brk; va < new_brk;
				    va += PAGE_SIZE) {
					pmap_remove(va);
				}
				return (-POSIX_ENOMEM);
			}
			pmap_enter(va, phys,
			    PTE_PRESENT | PTE_RW | PTE_USER);
		}
	} else {
		for (va = new_brk; va < old_brk; va += PAGE_SIZE) {
			pmap_remove(va);
		}
	}

	proc->brk = new_brk;
	return ((s64)proc->brk);
}
