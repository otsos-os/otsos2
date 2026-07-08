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

$define %type shm_segment_t as shared memory segment metadata
$define %type api_shmget_args as struct with key, size, flags, id
$define %type api_shmmap_args as struct with id, addr, size, prot, flags
$define %type api_shminfo_args as struct with id, key, size, refs, mode

$define %func shm_get as function with args int
$define %func shm_put as procedure with args shm_segment_t *
$define %func shm_get_or_create as function with args u64, u64, int, int *
$define %func shm_remove as function with args int
$define %func shm_attach as function with args int
$define %func shm_detach as procedure with args int
$define %func shm_map as function with args shm_segment_t *, u64, u64, u32, u32
$define %func shm_info as function with args int, struct api_shminfo_args *
$define %func api_shm_get as function with args struct api_shmget_args *
$define %func api_shm_map as function with args struct api_shmmap_args *
$define %func api_shm_ctl as function with args int, int, void *

*/

/* !SPACE!

$space %export shm_get, shm_put, shm_get_or_create, shm_remove
$space %export shm_attach, shm_detach
$space %export shm_map, shm_info
$space %export api_shm_get, api_shm_map, api_shm_ctl

*/

#ifndef SHM_H
#define SHM_H

#include <mlibc/mlibc.h>
#include <mm/vm/vm_object.h>

#define SHM_MAX_SEGMENTS	64
#define SHM_PRIVATE		0
#define SHM_CREAT		01000
#define SHM_EXCL		02000
#define SHM_RDONLY		010000
#define SHM_CTL_RMID		0
#define SHM_CTL_STAT		2

typedef struct shm_segment {
	int		used;
	int		removed;
	int		id;
	u32		mode;
	u32		refs;
	u32		attaches;
	u64		key;
	u64		size;
	vm_object_t	*object;
} shm_segment_t;

struct api_shmget_args {
	u64	key;
	u64	size;
	u32	flags;
	u32	id;
};

struct api_shmmap_args {
	u32	id;
	u32	prot;
	u32	flags;
	u64	addr;
	u64	size;
};

struct api_shminfo_args {
	u32	id;
	u32	mode;
	u32	refs;
	u32	removed;
	u64	key;
	u64	size;
};

shm_segment_t	*shm_get(int id);
void		shm_put(shm_segment_t *seg);
shm_segment_t	*shm_get_or_create(u64 key, u64 size, int flags,
		    int *error);
int		shm_remove(int id);
int		shm_attach(int id);
void		shm_detach(int id);
u64		shm_map(shm_segment_t *seg, u64 addr, u64 size, u32 prot,
		    u32 flags);
int		shm_info(int id, struct api_shminfo_args *info);
int		api_shm_get(struct api_shmget_args *uargs);
u64		api_shm_map(struct api_shmmap_args *uargs);
int		api_shm_ctl(int id, int cmd, void *uarg);

#endif
