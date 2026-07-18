/* !DEFINES!

$define %type size_t as native object size
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

#ifndef YABOX_H
#define YABOX_H

#include <stddef.h>
#include <stdint.h>

#define YBX_PATH_MAX	256
#define YBX_DIRENT_MAX	128
#define YBX_IO_BUFSIZE	1024

const char	*ybx_strerror(int code);
void		ybx_error(const char *tool, const char *path, int code);
int		ybx_copy_path(char *dst, size_t size, const char *src);
int		ybx_join_path(char *dst, size_t size, const char *dir,
		    const char *name);
int		ybx_basename_copy(char *dst, size_t size, const char *path);
int		ybx_target_path(char *out, size_t size, const char *src,
		    const char *dst, int force_dir);
int		ybx_is_dir(const char *path);
int		ybx_parse_u32(const char *text, uint32_t *out);

#endif
