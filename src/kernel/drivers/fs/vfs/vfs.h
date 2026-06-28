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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, NOT LIMITED TO, THE
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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type vnode_t as struct with type, refcount, size, mode, name, ops, data
$define %type vfs_ops_t as struct with read, write, stat, readdir function pointers
$define %type posix_stat_t as struct with POSIX stat fields

$define %func vfs_init as procedure with args void
$define %func vfs_resolve as function with args const char *, vnode_t **
$define %func vfs_create_file as function with args const char *
$define %func vfs_mkdir as function with args const char *
$define %func vfs_rmdir as function with args const char *
$define %func vfs_unlink as function with args const char *
$define %func vfs_rename as function with args const char *, const char *
$define %func vfs_truncate as function with args const char *, u64
$define %func vnode_acquire as function with args vnode_t *
$define %func vnode_release as procedure with args vnode_t *
$define %func vnode_read as function with args vnode_t *, void *, u64, u64
$define %func vnode_write as function with args vnode_t *, const void *, u64, u64
$define %func vnode_stat as function with args vnode_t *, posix_stat_t *
$define %func vnode_readdir as function with args vnode_t *, u32, char *, int *
$define %func vnode_alloc as function with args int, const char *
$define %func vfs_is_initialized as function with args void

*/

/* !SPACE!

$space %export vfs_init, vfs_resolve, vfs_create_file
$space %export vfs_mkdir, vfs_rmdir, vfs_unlink, vfs_rename
$space %export vfs_truncate
$space %export vnode_acquire, vnode_release
$space %export vnode_read, vnode_write, vnode_stat, vnode_readdir
$space %export vnode_alloc
$space %export vfs_is_initialized

*/

#ifndef VFS_H
#define VFS_H

#include <mlibc/mlibc.h>

#define VREG	1
#define VDIR	2
#define VCHR	3
#define VPIPE	4

#define VFS_MAX_VNODES	256

#define POSIX_S_IFMT	0xF000
#define POSIX_S_IFREG	0x8000
#define POSIX_S_IFDIR	0x4000
#define POSIX_S_IFCHR	0x2000
#define POSIX_S_IFIFO	0x1000
#define POSIX_S_IRWXU	0x01C0
#define POSIX_S_IRWXG	0x0038
#define POSIX_S_IRWXO	0x0007
#define POSIX_S_ISUID	0x0800
#define POSIX_S_ISGID	0x0400
#define POSIX_S_ISVTX	0x0200

typedef struct posix_stat {
	u64	st_dev;
	u64	st_ino;
	u64	st_nlink;
	u32	st_mode;
	u32	st_uid;
	u32	st_gid;
	u32	__pad0;
	u64	st_rdev;
	s64	st_size;
	s64	st_blksize;
	s64	st_blocks;
	s64	st_atime;
	s64	st_atime_nsec;
	s64	st_mtime;
	s64	st_mtime_nsec;
	s64	st_ctime;
	s64	st_ctime_nsec;
	s64	__unused[3];
} __attribute__((packed)) posix_stat_t;

typedef struct vnode {
	int		type;
	int		refcount;
	int		data_owned;
	u64		size;
	u32		mode;
	char		name[32];
	void		*data;
	int		(*read_fn)(struct vnode *, void *, u64, u64);
	int		(*write_fn)(struct vnode *, const void *, u64, u64);
	int		(*stat_fn)(struct vnode *, posix_stat_t *);
	int		(*readdir_fn)(struct vnode *, u32, char *, int *);
} vnode_t;

void		vfs_init(void);
int		vfs_is_initialized(void);
int		vfs_resolve(const char *path, vnode_t **out);
int		vfs_create_file(const char *path);
int		vfs_mkdir(const char *path);
int		vfs_rmdir(const char *path);
int		vfs_unlink(const char *path);
int		vfs_rename(const char *oldpath, const char *newpath);
int		vfs_truncate(const char *path, u64 length);

vnode_t		*vnode_alloc(int type, const char *name);
vnode_t		*vnode_acquire(vnode_t *vn);
void		vnode_release(vnode_t *vn);
int		vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset);
int		vnode_write(vnode_t *vn, const void *buf, u64 count,
		    u64 offset);
int		vnode_stat(vnode_t *vn, posix_stat_t *st);
int		vnode_readdir(vnode_t *vn, u32 index, char *name,
		    int *type);

#endif
