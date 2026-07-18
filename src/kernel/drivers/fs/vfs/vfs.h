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
$define %type vfs_back_ops as backend operation table forward declaration
$define %type posix_stat_t as struct with POSIX stat fields

$define %func vfs_init as procedure with args void
$define %func vfs_mount as function with args const char *, const struct vfs_back_ops *
$define %func vfs_mount_named as function with args const char *, const char *, u64
$define %func vfs_umount as function with args const char *
$define %func vfs_unmount as function with args const char *
$define %func vfs_mount_can_exec as function with args const char *
$define %func vfs_resolve as function with args const char *, vnode_t **
$define %func vfs_resolve_nofollow as function with args const char *, vnode_t **
$define %func vfs_create_file as function with args const char *
$define %func vfs_mkdir as function with args const char *
$define %func vfs_rmdir as function with args const char *
$define %func vfs_unlink as function with args const char *
$define %func vfs_rename as function with args const char *, const char *
$define %func vfs_truncate as function with args const char *, u64
$define %func vfs_symlink as function with args const char *, const char *
$define %func vfs_link as function with args const char *, const char *
$define %func vfs_readlink as function with args const char *, char *, size_t
$define %func vnode_acquire as function with args vnode_t *
$define %func vnode_release as procedure with args vnode_t *
$define %func vnode_can_exec as function with args vnode_t *
$define %func vnode_read as function with args vnode_t *, void *, u64, u64
$define %func vnode_write as function with args vnode_t *, const void *, u64, u64
$define %func vnode_stat as function with args vnode_t *, posix_stat_t *
$define %func vnode_readdir as function with args vnode_t *, u32, char *, int *
$define %func vnode_alloc as function with args int, const char *
$define %func vfs_is_initialized as function with args void
$define %func vnode_readlink as function with args vnode_t *, char *, size_t

*/

/* !SPACE!

$space %export vfs_init, vfs_mount, vfs_mount_named
$space %export vfs_umount, vfs_unmount, vfs_mount_can_exec
$space %export vfs_resolve, vfs_resolve_nofollow, vfs_create_file
$space %export vfs_mkdir, vfs_rmdir, vfs_unlink, vfs_rename
$space %export vfs_truncate, vfs_symlink, vfs_link, vfs_readlink
$space %export vnode_acquire, vnode_release, vnode_can_exec
$space %export vnode_read, vnode_write, vnode_stat, vnode_readdir
$space %export vnode_readlink
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
#define VLNK	5
#define VSOCK	6

#define VFS_MAX_VNODES	256

#define	VFS_MNT_RDONLY		0x00000001ULL
#define	VFS_MNT_NOSUID		0x00000002ULL
#define	VFS_MNT_NODEV		0x00000004ULL
#define	VFS_MNT_NOEXEC		0x00000008ULL
#define	VFS_MNT_SYNCHRONOUS	0x00000010ULL
#define	VFS_MNT_MANDLOCK	0x00000040ULL
#define	VFS_MNT_DIRSYNC		0x00000080ULL
#define	VFS_MNT_NOATIME		0x00000400ULL
#define	VFS_MNT_NODIRATIME	0x00000800ULL
#define	VFS_MNT_RELATIME	0x00200000ULL
#define	VFS_MNT_KUSR_ONLY	0x100000000ULL

#define	VFS_MNT_SUPPORTED	(VFS_MNT_RDONLY | VFS_MNT_NOSUID | \
				    VFS_MNT_NODEV | VFS_MNT_NOEXEC | \
				    VFS_MNT_SYNCHRONOUS | \
				    VFS_MNT_MANDLOCK | VFS_MNT_DIRSYNC | \
				    VFS_MNT_NOATIME | \
				    VFS_MNT_NODIRATIME | \
				    VFS_MNT_RELATIME | \
				    VFS_MNT_KUSR_ONLY)

#define POSIX_S_IFMT	0xF000
#define POSIX_S_IFREG	0x8000
#define POSIX_S_IFDIR	0x4000
#define POSIX_S_IFCHR	0x2000
#define POSIX_S_IFIFO	0x1000
#define POSIX_S_IFLNK	0xA000
#define POSIX_S_IFSOCK	0xC000
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
	u32		uid;
	u32		gid;
	u32		mount_id;
	char		name[32];
	void		*data;
	int		(*read_fn)(struct vnode *, void *, u64, u64);
	int		(*write_fn)(struct vnode *, const void *, u64, u64);
	int		(*stat_fn)(struct vnode *, posix_stat_t *);
	int		(*readdir_fn)(struct vnode *, u32, char *, int *);
	int		(*ioctl_fn)(struct vnode *, u64, void *);
	int		(*readlink_fn)(struct vnode *, char *, size_t);
} vnode_t;

struct vfs_back_ops;

void		vfs_init(void);
int		vfs_is_initialized(void);
int		vfs_mount(const char *path,
		    const struct vfs_back_ops *ops);
int		vfs_mount_named(const char *path, const char *fstype,
		    u64 flags);
int		vfs_umount(const char *path);
int		vfs_unmount(const char *path);
int		vfs_mount_can_exec(const char *path);
int		vfs_resolve(const char *path, vnode_t **out);
int		vfs_resolve_nofollow(const char *path, vnode_t **out);
int		vfs_create_file(const char *path);
int		vfs_mkdir(const char *path);
int		vfs_rmdir(const char *path);
int		vfs_unlink(const char *path);
int		vfs_rename(const char *oldpath, const char *newpath);
int		vfs_truncate(const char *path, u64 length);
int	vfs_symlink(const char *target, const char *linkpath);
int	vfs_link(const char *oldpath, const char *newpath);
int	vfs_readlink(const char *path, char *buf, size_t bufsize);
int		vfs_chdir(const char *path);
int		vfs_getcwd(char *buf, u32 size);
int		vfs_read_file_full(const char *path, u8 *buf, u32 bufsize,
		    u32 *bytes_read);
int		vfs_write_file(const char *path, const u8 *data, u32 size);

vnode_t		*vnode_alloc(int type, const char *name);
vnode_t		*vnode_acquire(vnode_t *vn);
void		vnode_release(vnode_t *vn);
int		vnode_can_exec(vnode_t *vn);
int		vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset);
int		vnode_write(vnode_t *vn, const void *buf, u64 count,
		    u64 offset);
int		vnode_stat(vnode_t *vn, posix_stat_t *st);
int		vnode_readdir(vnode_t *vn, u32 index, char *name,
		    int *type);
int		vnode_ioctl(vnode_t *vn, u64 cmd, void *arg);
int		vnode_readlink(vnode_t *vn, char *buf, size_t bufsize);

#endif
