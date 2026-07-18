/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type vnode_t as VFS vnode
$define %type vfs_back_ops_t as backend operation table

$define %func vfs_back_init as function with args void
$define %func vfs_back_mount as function with args const char *, const vfs_back_ops_t *
$define %func vfs_back_mount_flags as function with args const char *, const vfs_back_ops_t *, u64
$define %func vfs_back_mount_named as function with args const char *, const char *, u64
$define %func vfs_back_umount as function with args const char *
$define %func vfs_back_mount_can_read as function with args u32
$define %func vfs_back_mount_can_write as function with args u32
$define %func vfs_back_mount_can_exec as function with args const char *
$define %func vfs_back_mount_can_exec_id as function with args u32
$define %func vfs_back_mount_ref as function with args u32
$define %func vfs_back_mount_unref as function with args u32
$define %func vfs_back_resolve as function with args const char *, vnode_t **, int
$define %func vfs_back_create_file as function with args const char *
$define %func vfs_back_mkdir as function with args const char *
$define %func vfs_back_rmdir as function with args const char *
$define %func vfs_back_unlink as function with args const char *
$define %func vfs_back_rename as function with args const char *, const char *
$define %func vfs_back_truncate as function with args const char *, u64
$define %func vfs_back_symlink as function with args const char *, const char *
$define %func vfs_back_link as function with args const char *, const char *
$define %func vfs_back_readlink as function with args const char *, char *, size_t
$define %func vfs_back_chdir as function with args const char *
$define %func vfs_back_getcwd as function with args char *, u32
$define %func vfs_back_write_file as function with args const char *, const u8 *, u32

*/

/* !SPACE!

$space %export vfs_back_init, vfs_back_mount, vfs_back_mount_flags
$space %export vfs_back_mount_named, vfs_back_umount
$space %export vfs_back_mount_can_read, vfs_back_mount_can_write
$space %export vfs_back_mount_can_exec, vfs_back_mount_can_exec_id
$space %export vfs_back_mount_ref, vfs_back_mount_unref
$space %export vfs_back_resolve
$space %export vfs_back_create_file, vfs_back_mkdir, vfs_back_rmdir
$space %export vfs_back_unlink, vfs_back_rename, vfs_back_truncate
$space %export vfs_back_symlink, vfs_back_link, vfs_back_readlink
$space %export vfs_back_chdir, vfs_back_getcwd, vfs_back_write_file

*/

#ifndef KERNEL_DRIVERS_FS_VFS_BACK_VFS_BACK_H
#define KERNEL_DRIVERS_FS_VFS_BACK_VFS_BACK_H

#include <kernel/drivers/fs/vfs/vfs.h>

typedef struct vfs_back_ops {
	const char	*name;
	int		(*init)(void);
	vnode_t		*(*lookup)(const char *);
	int		(*create_file)(const char *);
	int		(*mkdir)(const char *);
	int		(*rmdir)(const char *);
	int		(*unlink)(const char *);
	int		(*rename)(const char *, const char *);
	int		(*truncate)(const char *, u64);
	int		(*symlink)(const char *, const char *);
	int		(*link)(const char *, const char *);
	int		(*chdir)(const char *);
	int		(*getcwd)(char *, u32);
	int		(*write_file)(const char *, const u8 *, u32);
	int		(*umount)(const char *);
} vfs_back_ops_t;

int	vfs_back_init(void);
int	vfs_back_mount(const char *path, const vfs_back_ops_t *ops);
int	vfs_back_mount_flags(const char *path, const vfs_back_ops_t *ops,
	    u64 flags);
int	vfs_back_mount_named(const char *path, const char *fstype, u64 flags);
int	vfs_back_umount(const char *path);
int	vfs_back_mount_can_read(u32 mount_id);
int	vfs_back_mount_can_write(u32 mount_id);
int	vfs_back_mount_can_exec(const char *path);
int	vfs_back_mount_can_exec_id(u32 mount_id);
int	vfs_back_mount_ref(u32 mount_id);
int	vfs_back_mount_unref(u32 mount_id);
int	vfs_back_resolve(const char *path, vnode_t **out, int follow);
int	vfs_back_create_file(const char *path);
int	vfs_back_mkdir(const char *path);
int	vfs_back_rmdir(const char *path);
int	vfs_back_unlink(const char *path);
int	vfs_back_rename(const char *oldpath, const char *newpath);
int	vfs_back_truncate(const char *path, u64 length);
int	vfs_back_symlink(const char *target, const char *linkpath);
int	vfs_back_link(const char *oldpath, const char *newpath);
int	vfs_back_readlink(const char *path, char *buf, size_t bufsize);
int	vfs_back_chdir(const char *path);
int	vfs_back_getcwd(char *buf, u32 size);
int	vfs_back_write_file(const char *path, const u8 *data, u32 size);

#endif
