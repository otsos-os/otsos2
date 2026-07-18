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
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

static char *
copy_user_string(const char *user, int max_len)
{
	char	*kbuf;
	int	len;

	if (!user || !is_user_address(user, 1)) {
		return (NULL);
	}

	len = 0;
	while (len < max_len) {
		if (!is_user_address(user + len, 1)) {
			return (NULL);
		}
		if (user[len] == '\0') {
			break;
		}
		len++;
	}

	if (len >= max_len) {
		return (NULL);
	}

	kbuf = (char *)kmem_calloc(len + 1, 1);
	if (!kbuf) {
		return (NULL);
	}

	memcpy(kbuf, user, len);
	kbuf[len] = '\0';
	return (kbuf);
}

static s64
posix_api_ret(int ret)
{
	int	err;

	if (ret >= 0) {
		return ((s64)ret);
	}

	err = -ret;
	switch (err) {
	case API_ERR_PERM:
		return (-POSIX_EPERM);
	case API_ERR_ACCESS:
		return (-POSIX_EACCES);
	case API_ERR_NOT_FOUND:
		return (-POSIX_ENOENT);
	case API_ERR_BAD_ADDR:
		return (-POSIX_EFAULT);
	case API_ERR_NO_MEMORY:
	case API_ERR_NOMEM:
		return (-POSIX_ENOMEM);
	case API_ERR_BUSY:
		return (-POSIX_EBUSY);
	case API_ERR_EXISTS:
		return (-POSIX_EEXIST);
	case API_ERR_NODEV:
		return (-POSIX_ENODEV);
	case API_ERR_NOT_DIR:
		return (-POSIX_ENOTDIR);
	case API_ERR_IS_DIR:
		return (-POSIX_EISDIR);
	case API_ERR_NOT_SUPPORTED:
		return (-POSIX_ENOSYS);
	case API_ERR_TOO_BIG:
		return (-POSIX_ENAMETOOLONG);
	case API_ERR_FILE_TOO_BIG:
		return (-POSIX_EFBIG);
	case API_ERR_HANDLES_FULL:
		return (-POSIX_EMFILE);
	case API_ERR_OBJECTS_FULL:
		return (-POSIX_ENOSPC);
	case API_ERR_READ_ONLY:
		return (-POSIX_EROFS);
	case API_ERR_NO_SPACE:
		return (-POSIX_ENOSPC);
	case API_ERR_IO:
		return (-POSIX_EIO);
	case API_ERR_CROSS_DEVICE:
		return (-POSIX_EXDEV);
	case API_ERR_NOT_EMPTY:
		return (-POSIX_ENOTEMPTY);
	case API_ERR_BAD_VALUE:
	case API_ERR_INVAL:
	default:
		return (-POSIX_EINVAL);
	}
}

s64
posix_mount(u64 source, u64 target, u64 fstype, u64 flags, u64 data,
    u64 a6, registers_t *regs)
{
	int	ret;

	(void)a6;
	(void)regs;

	ret = api_fs_mnt((const char *)source, (const char *)target,
	    (const char *)fstype, flags, (const void *)data);
	return (posix_api_ret(ret));
}

s64
posix_umount2(u64 target, u64 flags, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	int	ret;

	(void)a3;
	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;

	ret = api_fs_umnt((const char *)target, flags);
	return (posix_api_ret(ret));
}

s64
posix_chdir(u64 path_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char	*path;
	int	ret;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	ret = vfs_chdir(path);
	kmem_free(path);

	if (ret != 0) {
		return (posix_api_ret(ret));
	}

	return (0);
}

s64
posix_getcwd(u64 buf_u, u64 size_u, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char	kbuf[256];
	char	*path;
	u32	path_len;
	char	*buf;
	u32	size;
	int	ret;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	buf = (char *)buf_u;
	size = (u32)size_u;

	if (!buf || size == 0) {
		return (-POSIX_EINVAL);
	}

	if (!is_user_address(buf, size)) {
		return (-POSIX_EFAULT);
	}

	memset(kbuf, 0, sizeof(kbuf));
	ret = vfs_getcwd(kbuf, sizeof(kbuf));
	if (ret != 0) {
		return (posix_api_ret(ret));
	}

	path = kbuf;

	path_len = strlen(path);
	if (path_len >= size) {
		return (-POSIX_ERANGE);
	}

	memcpy(buf, path, path_len);
	buf[path_len] = '\0';

	return ((s64)(u64)buf);
}

s64
posix_mkdir(u64 path_u, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char	*path;
	int	ret;

	(void)mode; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	ret = vfs_mkdir(path);
	kmem_free(path);

	if (ret != 0) {
		return (posix_api_ret(ret));
	}

	return (0);
}

s64
posix_rmdir(u64 path_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char	*path;
	int	ret;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	ret = vfs_rmdir(path);
	kmem_free(path);

	if (ret != 0) {
		return (posix_api_ret(ret));
	}

	return (0);
}

s64
posix_unlink(u64 path_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char	*path;
	int	ret;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	ret = vfs_unlink(path);
	kmem_free(path);

	if (ret != 0) {
		return (posix_api_ret(ret));
	}

	return (0);
}

s64
posix_rename(u64 oldpath_u, u64 newpath_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	char	*oldpath, *newpath;
	int	ret;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	oldpath = copy_user_string((const char *)oldpath_u, 256);
	if (!oldpath) {
		return (-POSIX_EFAULT);
	}

	newpath = copy_user_string((const char *)newpath_u, 256);
	if (!newpath) {
		kmem_free(oldpath);
		return (-POSIX_EFAULT);
	}

	ret = vfs_rename(oldpath, newpath);
	kmem_free(oldpath);
	kmem_free(newpath);

	if (ret != 0) {
		return (posix_api_ret(ret));
	}

	return (0);
}

s64
posix_readlink(u64 path_u, u64 buf_u, u64 bufsize, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	char	*path;
	char	*kbuf;
	int	ret;
	(void)a4; (void)a5; (void)a6; (void)regs;
	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}
	if (!buf_u || bufsize == 0) {
		kmem_free(path);
		return (-POSIX_EINVAL);
	}

	if (!is_user_address((void *)buf_u, bufsize)) {
		kmem_free(path);
		return (-POSIX_EFAULT);
	}

	kbuf = (char *)kmem_calloc(bufsize, 1);
	if (!kbuf) {
		kmem_free(path);
		return (-POSIX_ENOMEM);
	}

	ret = vfs_readlink(path, kbuf, bufsize);
	kmem_free(path);

	if (ret < 0) {
		kmem_free(kbuf);
		return (posix_api_ret(ret));
	}

	memcpy((void *)buf_u, kbuf, ret);
	kmem_free(kbuf);
	return ((s64)ret);
}
s64
posix_symlink(u64 target_u, u64 linkpath_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	char	*target;
	char	*linkpath;
	int	ret;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	target = copy_user_string((const char *)target_u, 256);
	if (!target) {
		return (-POSIX_EFAULT);
	}

	linkpath = copy_user_string((const char *)linkpath_u, 256);
	if (!linkpath) {
		kmem_free(target);
		return (-POSIX_EFAULT);
	}

	ret = vfs_symlink(target, linkpath);
	kmem_free(target);
	kmem_free(linkpath);

	if (ret != 0) {
		return (posix_api_ret(ret));
	}

	return (0);
}

s64
posix_chmod(u64 path_u, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	(void)path_u; (void)mode; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;
	return (0);
}

s64
posix_fchmod(u64 fd_u, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	(void)fd_u; (void)mode; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;
	return (0);
}

s64
posix_link(u64 oldpath_u, u64 newpath_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	char	*oldpath;
	char	*newpath;
	int	ret;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	oldpath = copy_user_string((const char *)oldpath_u, 256);
	if (!oldpath) {
		return (-POSIX_EFAULT);
	}

	newpath = copy_user_string((const char *)newpath_u, 256);
	if (!newpath) {
		kmem_free(oldpath);
		return (-POSIX_EFAULT);
	}

	ret = vfs_link(oldpath, newpath);
	kmem_free(oldpath);
	kmem_free(newpath);

	if (ret != 0) {
		return (posix_api_ret(ret));
	}
	return (0);
}
