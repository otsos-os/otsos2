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

$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type process_t as process state

$define %func copy_user_string as function with args const char *, char *, int, int
$define %func api_fs_mnt as function with args const char *, const char *, const char *, u64, const void *
$define %func api_fs_umnt as function with args const char *, u64

*/

/* !SPACE!

$space %internal copy_user_string
$space %export api_fs_mnt, api_fs_umnt

*/

#include <kernel/api/api.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/other/restrict.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

static int
copy_user_string(const char *user, char *out, int size, int nullable)
{
	int	len;

	if (!out || size <= 1) {
		return (-API_ERR_BAD_VALUE);
	}
	out[0] = '\0';
	if (!user) {
		if (nullable) {
			return (0);
		}
		return (-API_ERR_BAD_ADDR);
	}
	if (!is_user_address(user, 1)) {
		return (-API_ERR_BAD_ADDR);
	}

	len = 0;
	while (len < size - 1) {
		if (!is_user_address(user + len, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
		out[len] = user[len];
		if (user[len] == '\0') {
			return (0);
		}
		len++;
	}

	out[0] = '\0';
	return (-API_ERR_TOO_BIG);
}

int
api_fs_mnt(const char *source, const char *target, const char *fstype,
    u64 flags, const void *data)
{
	process_t	*proc;
	char		ksource[256];
	char		ktarget[256];
	char		kfstype[32];
	int		ret;

	(void)data;

	proc = process_current();
	if (!proc_has_privilege(proc)) {
		return (-API_ERR_PERM);
	}
	if ((flags & ~VFS_MNT_SUPPORTED) != 0) {
		return (-API_ERR_NOT_SUPPORTED);
	}

	ret = copy_user_string(source, ksource, sizeof(ksource), 1);
	if (ret != 0) {
		return (ret);
	}
	ret = copy_user_string(target, ktarget, sizeof(ktarget), 0);
	if (ret != 0) {
		return (ret);
	}
	ret = copy_user_string(fstype, kfstype, sizeof(kfstype), 0);
	if (ret != 0) {
		return (ret);
	}
	if (ktarget[0] == '\0' || kfstype[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (restrict_kusr_check(ktarget)) {
		return (-API_ERR_PERM);
	}

	ret = vfs_mount_named(ktarget, kfstype, flags);
	if (ret == -2) {
		return (-API_ERR_NODEV);
	}
	if (ret != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	return (0);
}

int
api_fs_umnt(const char *target, u64 flags)
{
	process_t	*proc;
	char		ktarget[256];
	int		ret;

	proc = process_current();
	if (!proc_has_privilege(proc)) {
		return (-API_ERR_PERM);
	}
	if (flags != 0) {
		return (-API_ERR_NOT_SUPPORTED);
	}

	ret = copy_user_string(target, ktarget, sizeof(ktarget), 0);
	if (ret != 0) {
		return (ret);
	}
	if (ktarget[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (restrict_kusr_check(ktarget)) {
		return (-API_ERR_PERM);
	}

	ret = vfs_umount(ktarget);
	if (ret == -2) {
		return (-API_ERR_BUSY);
	}
	if (ret != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	return (0);
}
