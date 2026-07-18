/* !DEFINES!

$define %type api_fs_stat as native file metadata
$define %type uint32_t as 32 bit unsigned
$define %func ybx_strerror as function with args int
$define %func ybx_error as procedure with args const char *, const char *, int
$define %func ybx_copy_path as function with args char *, size_t, const char *
$define %func ybx_join_path as function with args path parts
$define %func ybx_basename_copy as function with args char *, size_t, const char *
$define %func ybx_target_path as function with args path parts
$define %func ybx_is_dir as function with args const char *
$define %func ybx_parse_u32 as function with args const char *, uint32_t *

*/

/* !SPACE!

$space %export ybx_strerror, ybx_error, ybx_copy_path, ybx_join_path
$space %export ybx_basename_copy, ybx_target_path, ybx_is_dir
$space %export ybx_parse_u32

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yabox.h"

const char *
ybx_strerror(int code)
{
	if (code < 0) {
		code = -code;
	}
	switch (code) {
	case EPERM:
		return ("operation not permitted");
	case ENOENT:
		return ("no such file or directory");
	case ESRCH:
		return ("no such process");
	case EIO:
		return ("i/o error");
	case E2BIG:
		return ("name or argument too large");
	case EBADF:
		return ("bad file handle");
	case EAGAIN:
		return ("resource busy");
	case ENOMEM:
		return ("out of memory");
	case EACCES:
		return ("permission denied");
	case EFAULT:
		return ("bad address");
	case EINVAL:
		return ("invalid argument");
	case EBUSY:
		return ("device or resource busy");
	case EEXIST:
		return ("file exists");
	case EXDEV:
		return ("cross-device link");
	case ENODEV:
		return ("no such device");
	case ENOTDIR:
		return ("not a directory");
	case EISDIR:
		return ("is a directory");
	case EMFILE:
		return ("too many open files");
	case EFBIG:
		return ("file too large");
	case ENOSPC:
		return ("no space left on device");
	case EROFS:
		return ("read-only filesystem");
	case EPIPE:
		return ("broken pipe");
	case ENOSYS:
		return ("no such syscall");
	case ENOTEMPTY:
		return ("directory not empty");
	case ENOTSUP:
		return ("operation not supported");
	case 22:
		return ("invalid argument");
	default:
		return ("unknown error");
	}
}

void
ybx_error(const char *tool, const char *path, int code)
{
	if (code == 0) {
		code = errno;
	}
	if (!tool) {
		tool = "yabox";
	}
	fprintf(stderr, "%s: ", tool);
	if (path && path[0]) {
		fprintf(stderr, "%s: ", path);
	}
	fprintf(stderr, "%s\n", ybx_strerror(code));
}

int
ybx_copy_path(char *dst, size_t size, const char *src)
{
	size_t	len;

	if (!dst || size == 0 || !src) {
		errno = EINVAL;
		return (-1);
	}
	len = strlen(src);
	if (len + 1 > size) {
		errno = E2BIG;
		return (-1);
	}
	memcpy(dst, src, len + 1);
	return (0);
}

int
ybx_join_path(char *dst, size_t size, const char *dir, const char *name)
{
	size_t	dir_len, name_len, total;
	int	need_slash;

	if (!dst || size == 0 || !dir || !name || name[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	dir_len = strlen(dir);
	name_len = strlen(name);
	need_slash = 0;
	if (dir_len > 0 && dir[dir_len - 1] != '/') {
		need_slash = 1;
	}
	total = dir_len + (size_t)need_slash + name_len + 1;
	if (total > size) {
		errno = E2BIG;
		return (-1);
	}
	strcpy(dst, dir);
	if (need_slash) {
		strcat(dst, "/");
	}
	strcat(dst, name);
	return (0);
}

int
ybx_basename_copy(char *dst, size_t size, const char *path)
{
	const char	*start, *end, *p;
	size_t		len, i;

	if (!dst || size == 0 || !path || path[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	end = path + strlen(path);
	while (end > path && end[-1] == '/') {
		end--;
	}
	if (end == path) {
		return (ybx_copy_path(dst, size, "/"));
	}
	p = end;
	while (p > path && p[-1] != '/') {
		p--;
	}
	start = p;
	len = (size_t)(end - start);
	if (len + 1 > size) {
		errno = E2BIG;
		return (-1);
	}
	for (i = 0; i < len; i++) {
		dst[i] = start[i];
	}
	dst[len] = '\0';
	return (0);
}

int
ybx_target_path(char *out, size_t size, const char *src, const char *dst,
    int force_dir)
{
	char	base[YBX_PATH_MAX];

	if (force_dir || ybx_is_dir(dst)) {
		if (ybx_basename_copy(base, sizeof(base), src) < 0) {
			return (-1);
		}
		return (ybx_join_path(out, size, dst, base));
	}
	return (ybx_copy_path(out, size, dst));
}

int
ybx_is_dir(const char *path)
{
	struct api_fs_stat	st;

	if (!path || fsStat(path, &st) < 0) {
		return (0);
	}
	return (st.type == API_FS_TYPE_DIR);
}

int
ybx_parse_u32(const char *text, uint32_t *out)
{
	char		*end;
	unsigned long	value;

	if (!text || !out || text[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	errno = 0;
	value = strtoul(text, &end, 10);
	if (errno != 0 || !end || *end != '\0' || value > 0xffffffffUL) {
		errno = EINVAL;
		return (-1);
	}
	*out = (uint32_t)value;
	return (0);
}
