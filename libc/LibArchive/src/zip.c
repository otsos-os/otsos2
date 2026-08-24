/* !DEFINES!

$define %type la_zip as open zip archive state
$define %type la_eocd as end of central directory data
$define %type la_zip_entry as central directory file entry
$define %type la_inflate_io as streaming state for one deflated entry
$define %func rd16 as function with args const unsigned char *
$define %func rd32 as function with args const unsigned char *
$define %func la_fail as function with args int
$define %func la_inflate_errno as function with args int
$define %func la_entry_read as function with args arg, buf, len
$define %func la_entry_write as function with args arg, buf, len
$define %func la_inflate_entry as function with args zip, out, entry
$define %func la_read_full as function with args int, void *, size_t
$define %func la_seek as function with args int, uint64_t
$define %func la_is_dir_path as function with args const char *
$define %func la_mkdir_one as function with args const char *
$define %func la_mkdirs as function with args const char *
$define %func la_parent_dirs as function with args const char *
$define %func la_join_path as function with args output, size, dir, name
$define %func la_part_ok as function with args const char *, size_t
$define %func la_name_ok as function with args const char *
$define %func la_name_is_dir as function with args const char *
$define %func la_find_eocd as function with args la_zip *, la_eocd *
$define %func la_read_entry as function with args zip, offset, entry
$define %func la_open_entry_data as function with args zip, entry
$define %func la_copy_entry as function with args zip, output, entry
$define %func la_extract_entry as function with args zip, entry
$define %func la_extract_all as function with args la_zip *, const la_eocd *
$define %func la_zip_extract as function with args zip path, options

*/

/* !SPACE!

$space %internal rd16, rd32, la_fail, la_inflate_errno
$space %internal la_inflate_io_t, la_entry_read, la_entry_write
$space %internal la_inflate_entry
$space %internal la_read_full, la_seek, la_is_dir_path, la_mkdir_one
$space %internal la_mkdirs, la_parent_dirs, la_join_path, la_part_ok
$space %internal la_name_ok, la_name_is_dir, la_find_eocd, la_read_entry
$space %internal la_open_entry_data, la_copy_entry, la_extract_entry
$space %internal la_extract_all
$space %internal la_inflate_state, la_inflate_window
$space %export la_zip_extract

*/

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

#include <errno.h>
#include <libarchive.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LA_ZIP_LOCAL_SIG	0x04034b50U
#define LA_ZIP_CENTRAL_SIG	0x02014b50U
#define LA_ZIP_EOCD_SIG		0x06054b50U
#define LA_ZIP_METHOD_STORE	0
#define LA_ZIP_METHOD_DEFLATE	8
#define LA_ZIP_GP_ENCRYPTED	0x0001
#define LA_ZIP_GP_STRONG	0x0040
#define LA_ZIP_GP_PATCHED	0x0020
#define LA_ZIP_GP_UTF8		0x0800
#define LA_ZIP_EOCD_MIN		22
#define LA_ZIP_EOCD_WINDOW	(65535 + LA_ZIP_EOCD_MIN)
#define LA_ZIP_NAME_MAX		255
#define LA_PATH_MAX		256
#define LA_IO_SIZE		1024

struct la_zip {
	int			 fd;
	uint64_t		 size;
	const char		*dest_dir;
	la_zip_entry_cb	 on_entry;
	void			*arg;
};

struct la_eocd {
	uint64_t	cd_offset;
	uint64_t	cd_size;
	uint32_t	entries;
};

struct la_zip_entry {
	char		name[LA_ZIP_NAME_MAX + 1];
	uint64_t	local_offset;
	uint64_t	comp_size;
	uint64_t	uncomp_size;
	uint32_t	crc32;
	uint16_t	flags;
	uint16_t	method;
};

static unsigned char	la_eocd_buf[LA_ZIP_EOCD_WINDOW];
static la_inflate_t	la_inflate_state;
static unsigned char	la_inflate_window[LA_INF_WINDOW];

static uint16_t
rd16(const unsigned char *p)
{
	return ((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t
rd32(const unsigned char *p)
{
	return ((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static int
la_fail(int code)
{
	errno = code;
	return (-1);
}

static int
la_inflate_errno(int err)
{
	switch (err) {
	case LA_INF_UNSUPPORTED:
		return (ENOTSUP);
	case LA_INF_TOO_LARGE:
		return (EFBIG);
	case LA_INF_INVAL:
		return (EINVAL);
	case LA_INF_IO:
	case LA_INF_CORRUPT:
	default:
		return (EIO);
	}
}

static int
la_read_full(int fd, void *buf, size_t size)
{
	if (dataReadFull(fd, buf, size) < 0) {
		return (-1);
	}
	return (0);
}

static int
la_seek(int fd, uint64_t offset)
{
	if (dataSeek(fd, (long)offset, API_SEEK_SET) < 0) {
		return (-1);
	}
	return (0);
}

static int
la_is_dir_path(const char *path)
{
	struct api_fs_stat	st;

	if (fsStat(path, &st) < 0) {
		return (0);
	}
	return (st.type == API_FS_TYPE_DIR);
}

static int
la_mkdir_one(const char *path)
{
	struct api_fs_stat	st;
	int			code;

	if (fsStat(path, &st) == 0) {
		if (st.type == API_FS_TYPE_DIR) {
			return (0);
		}
		return (la_fail(ENOTDIR));
	}
	if (dataDir(API_DATA_DIR_MKDIR, path, NULL) == 0) {
		return (0);
	}
	code = errno;
	if (code == EEXIST && la_is_dir_path(path)) {
		return (0);
	}
	errno = code;
	return (-1);
}

static int
la_mkdirs(const char *path)
{
	char	buf[LA_PATH_MAX];
	size_t	i, len, start;

	if (!path || path[0] == '\0') {
		return (la_fail(EINVAL));
	}
	if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
		return (0);
	}
	len = strlen(path);
	if (len >= sizeof(buf)) {
		return (la_fail(E2BIG));
	}
	memcpy(buf, path, len + 1);
	while (len > 1 && buf[len - 1] == '/') {
		len--;
		buf[len] = '\0';
	}
	if (strcmp(buf, ".") == 0 || strcmp(buf, "/") == 0) {
		return (0);
	}
	start = 0;
	if (buf[0] == '/') {
		start = 1;
	}
	for (i = start; buf[i] != '\0'; i++) {
		if (buf[i] == '/') {
			buf[i] = '\0';
			if (la_mkdir_one(buf) < 0) {
				return (-1);
			}
			buf[i] = '/';
		}
	}
	return (la_mkdir_one(buf));
}

static int
la_parent_dirs(const char *path)
{
	char	buf[LA_PATH_MAX];
	char	*slash;
	size_t	len;

	if (!path || path[0] == '\0') {
		return (la_fail(EINVAL));
	}
	len = strlen(path);
	if (len >= sizeof(buf)) {
		return (la_fail(E2BIG));
	}
	memcpy(buf, path, len + 1);
	slash = strrchr(buf, '/');
	if (!slash) {
		return (0);
	}
	if (slash == buf) {
		return (0);
	}
	*slash = '\0';
	return (la_mkdirs(buf));
}

static int
la_join_path(char *out, size_t size, const char *dir, const char *name)
{
	size_t	dir_len, name_len, total;
	int	need_slash;

	if (!out || size == 0 || !dir || !name || name[0] == '\0') {
		return (la_fail(EINVAL));
	}
	if (dir[0] == '\0' || strcmp(dir, ".") == 0) {
		name_len = strlen(name);
		if (name_len + 1 > size) {
			return (la_fail(E2BIG));
		}
		memcpy(out, name, name_len + 1);
		return (0);
	}
	dir_len = strlen(dir);
	name_len = strlen(name);
	need_slash = 0;
	if (dir_len > 0 && dir[dir_len - 1] != '/') {
		need_slash = 1;
	}
	total = dir_len + (size_t)need_slash + name_len + 1;
	if (total > size) {
		return (la_fail(E2BIG));
	}
	memcpy(out, dir, dir_len);
	if (need_slash) {
		out[dir_len] = '/';
		memcpy(out + dir_len + 1, name, name_len + 1);
	} else {
		memcpy(out + dir_len, name, name_len + 1);
	}
	return (0);
}

static int
la_part_ok(const char *part, size_t len)
{
	if (len == 0) {
		return (0);
	}
	if (len == 1 && part[0] == '.') {
		return (0);
	}
	if (len == 2 && part[0] == '.' && part[1] == '.') {
		return (0);
	}
	return (1);
}

static int
la_name_ok(const char *name)
{
	const char	*part;
	size_t		 len, i;

	if (!name || name[0] == '\0' || name[0] == '/') {
		return (la_fail(EINVAL));
	}
	part = name;
	len = strlen(name);
	for (i = 0; i < len; i++) {
		if (name[i] == '\0' || name[i] == '\\' || name[i] == ':') {
			return (la_fail(EINVAL));
		}
		if (name[i] == '/') {
			if (!la_part_ok(part, (size_t)(name + i - part))) {
				return (la_fail(EINVAL));
			}
			if (name[i + 1] == '\0') {
				return (0);
			}
			part = name + i + 1;
		}
	}
	if (!la_part_ok(part, (size_t)(name + len - part))) {
		return (la_fail(EINVAL));
	}
	return (0);
}

static int
la_name_is_dir(const char *name)
{
	size_t	len;

	len = strlen(name);
	return (len > 0 && name[len - 1] == '/');
}

static int
la_find_eocd(struct la_zip *zip, struct la_eocd *eocd)
{
	uint64_t	start, window, pos;
	uint32_t	sig, cd_size, cd_offset;
	uint16_t	disk_no, cd_disk, disk_entries, total_entries;
	uint16_t	comment_len;

	if (zip->size < LA_ZIP_EOCD_MIN) {
		return (la_fail(EINVAL));
	}
	window = zip->size;
	if (window > LA_ZIP_EOCD_WINDOW) {
		window = LA_ZIP_EOCD_WINDOW;
	}
	start = zip->size - window;
	if (la_seek(zip->fd, start) < 0) {
		return (-1);
	}
	if (la_read_full(zip->fd, la_eocd_buf, (size_t)window) < 0) {
		return (-1);
	}
	pos = window - LA_ZIP_EOCD_MIN;
	for (;;) {
		sig = rd32(la_eocd_buf + pos);
		if (sig == LA_ZIP_EOCD_SIG) {
			comment_len = rd16(la_eocd_buf + pos + 20);
			if (pos + LA_ZIP_EOCD_MIN + comment_len == window) {
				break;
			}
		}
		if (pos == 0) {
			return (la_fail(EINVAL));
		}
		pos--;
	}
	disk_no = rd16(la_eocd_buf + pos + 4);
	cd_disk = rd16(la_eocd_buf + pos + 6);
	disk_entries = rd16(la_eocd_buf + pos + 8);
	total_entries = rd16(la_eocd_buf + pos + 10);
	cd_size = rd32(la_eocd_buf + pos + 12);
	cd_offset = rd32(la_eocd_buf + pos + 16);
	if (disk_no != 0 || cd_disk != 0 || disk_entries != total_entries) {
		return (la_fail(ENOTSUP));
	}
	if (total_entries == 0xffff || cd_size == 0xffffffffU ||
	    cd_offset == 0xffffffffU) {
		return (la_fail(ENOTSUP));
	}
	if ((uint64_t)cd_offset + (uint64_t)cd_size > zip->size) {
		return (la_fail(EINVAL));
	}
	eocd->cd_offset = cd_offset;
	eocd->cd_size = cd_size;
	eocd->entries = total_entries;
	return (0);
}

static int
la_read_entry(struct la_zip *zip, uint64_t offset, struct la_zip_entry *entry)
{
	unsigned char	h[46];
	uint32_t	sig, comp_size, uncomp_size, local_offset;
	uint16_t	name_len, extra_len, comment_len, disk_no;
	uint16_t	flags, method;
	size_t		i;

	if (offset + sizeof(h) > zip->size) {
		return (la_fail(EINVAL));
	}
	if (la_seek(zip->fd, offset) < 0) {
		return (-1);
	}
	if (la_read_full(zip->fd, h, sizeof(h)) < 0) {
		return (-1);
	}
	sig = rd32(h);
	if (sig != LA_ZIP_CENTRAL_SIG) {
		return (la_fail(EINVAL));
	}
	flags = rd16(h + 8);
	method = rd16(h + 10);
	comp_size = rd32(h + 20);
	uncomp_size = rd32(h + 24);
	name_len = rd16(h + 28);
	extra_len = rd16(h + 30);
	comment_len = rd16(h + 32);
	disk_no = rd16(h + 34);
	local_offset = rd32(h + 42);
	if (name_len == 0 || name_len > LA_ZIP_NAME_MAX) {
		return (la_fail(EINVAL));
	}
	if (offset + sizeof(h) + name_len + extra_len + comment_len >
	    zip->size) {
		return (la_fail(EINVAL));
	}
	if (disk_no != 0 || comp_size == 0xffffffffU ||
	    uncomp_size == 0xffffffffU || local_offset == 0xffffffffU) {
		return (la_fail(ENOTSUP));
	}
	if ((flags & (LA_ZIP_GP_ENCRYPTED | LA_ZIP_GP_STRONG |
	    LA_ZIP_GP_PATCHED)) != 0) {
		return (la_fail(ENOTSUP));
	}
	if (method != LA_ZIP_METHOD_STORE &&
	    method != LA_ZIP_METHOD_DEFLATE) {
		return (la_fail(ENOTSUP));
	}
	if (method == LA_ZIP_METHOD_STORE && comp_size != uncomp_size) {
		return (la_fail(EINVAL));
	}
	if ((uint64_t)local_offset + 30 > zip->size) {
		return (la_fail(EINVAL));
	}
	if (la_read_full(zip->fd, entry->name, name_len) < 0) {
		return (-1);
	}
	entry->name[name_len] = '\0';
	for (i = 0; i < name_len; i++) {
		if (entry->name[i] == '\0') {
			return (la_fail(EINVAL));
		}
	}
	if (la_name_ok(entry->name) < 0) {
		return (-1);
	}
	entry->local_offset = local_offset;
	entry->comp_size = comp_size;
	entry->uncomp_size = uncomp_size;
	entry->crc32 = rd32(h + 16);
	entry->flags = flags;
	entry->method = method;
	return (0);
}

static int
la_open_entry_data(struct la_zip *zip, const struct la_zip_entry *entry)
{
	unsigned char	h[30];
	char		name[LA_ZIP_NAME_MAX + 1];
	uint64_t	data_offset;
	uint32_t	sig;
	uint16_t	name_len, extra_len, method;
	uint16_t	flags;
	size_t		entry_name_len, i;

	if (la_seek(zip->fd, entry->local_offset) < 0) {
		return (-1);
	}
	if (la_read_full(zip->fd, h, sizeof(h)) < 0) {
		return (-1);
	}
	sig = rd32(h);
	flags = rd16(h + 6);
	method = rd16(h + 8);
	name_len = rd16(h + 26);
	extra_len = rd16(h + 28);
	entry_name_len = strlen(entry->name);
	if (sig != LA_ZIP_LOCAL_SIG || method != entry->method ||
	    name_len != entry_name_len) {
		return (la_fail(EINVAL));
	}
	if ((flags & (LA_ZIP_GP_ENCRYPTED | LA_ZIP_GP_STRONG |
	    LA_ZIP_GP_PATCHED)) != 0) {
		return (la_fail(ENOTSUP));
	}
	data_offset = entry->local_offset + sizeof(h) + name_len + extra_len;
	if (data_offset + entry->comp_size > zip->size) {
		return (la_fail(EINVAL));
	}
	if (la_read_full(zip->fd, name, name_len) < 0) {
		return (-1);
	}
	name[name_len] = '\0';
	for (i = 0; i < name_len; i++) {
		if (name[i] == '\0') {
			return (la_fail(EINVAL));
		}
	}
	if (strcmp(name, entry->name) != 0) {
		return (la_fail(EINVAL));
	}
	if (dataSeek(zip->fd, (long)extra_len, API_SEEK_CUR) < 0) {
		return (-1);
	}
	return (0);
}

static int
la_copy_entry(struct la_zip *zip, int out, const struct la_zip_entry *entry)
{
	char		buf[LA_IO_SIZE];
	uint64_t	left;
	uint32_t	crc;
	size_t		chunk;

	left = entry->comp_size;
	crc = 0xffffffffU;
	while (left > 0) {
		chunk = sizeof(buf);
		if (left < chunk) {
			chunk = (size_t)left;
		}
		if (la_read_full(zip->fd, buf, chunk) < 0) {
			return (-1);
		}
		if (dataWriteFull(out, buf, chunk) < 0) {
			return (-1);
		}
		crc = la_crc32_update(crc, buf, chunk);
		left -= chunk;
	}
	crc = crc ^ 0xffffffffU;
	if (crc != entry->crc32) {
		return (la_fail(EIO));
	}
	return (0);
}

typedef struct la_inflate_io {
	struct la_zip	*zip;
	int		 out;
	uint64_t	 left;
	uint32_t	 crc;
	int		 err;
} la_inflate_io_t;

static long
la_entry_read(void *arg, void *buf, size_t len)
{
	la_inflate_io_t	*io;
	long		 got;

	io = (la_inflate_io_t *)arg;
	if (io->left == 0) {
		return (0);
	}
	if ((uint64_t)len > io->left) {
		len = (size_t)io->left;
	}
	got = dataRead(io->zip->fd, buf, len);
	if (got < 0) {
		io->err = errno;
		return (-1);
	}
	if (got == 0) {
		io->err = EIO;
		return (-1);
	}
	io->left -= (uint64_t)got;
	return (got);
}

static int
la_entry_write(void *arg, const void *buf, size_t len)
{
	la_inflate_io_t	*io;

	io = (la_inflate_io_t *)arg;
	if (dataWriteFull(io->out, buf, len) < 0) {
		io->err = errno;
		return (-1);
	}
	io->crc = la_crc32_update(io->crc, buf, len);
	return (0);
}

static int
la_inflate_entry(struct la_zip *zip, int out,
    const struct la_zip_entry *entry)
{
	la_inflate_io_t	io;
	uint64_t	produced;
	int		ret;

	io.zip = zip;
	io.out = out;
	io.left = entry->comp_size;
	io.crc = 0xffffffffU;
	io.err = 0;

	produced = 0;
	ret = la_inflate_stream(&la_inflate_state, la_inflate_window,
	    sizeof(la_inflate_window), la_entry_read, &io, la_entry_write, &io,
	    &produced);
	if (ret != LA_INF_OK) {
		return (la_fail((ret == LA_INF_IO && io.err != 0) ? io.err :
		    la_inflate_errno(ret)));
	}
	if (produced != entry->uncomp_size) {
		return (la_fail(EIO));
	}
	if ((io.crc ^ 0xffffffffU) != entry->crc32) {
		return (la_fail(EIO));
	}
	return (0);
}

static int
la_extract_entry(struct la_zip *zip, const struct la_zip_entry *entry)
{
	char	path[LA_PATH_MAX];
	int	out, code, ret;

	if (la_join_path(path, sizeof(path), zip->dest_dir, entry->name) < 0) {
		return (-1);
	}
	if (la_name_is_dir(entry->name)) {
		if (la_mkdirs(path) < 0) {
			return (-1);
		}
		if (zip->on_entry) {
			if (zip->on_entry(entry->name, path, entry->uncomp_size,
			    zip->arg) != 0) {
				return (la_fail(EINTR));
			}
		}
		return (0);
	}
	if (la_parent_dirs(path) < 0) {
		return (-1);
	}
	out = dataOpen(path, API_OPEN_WRITE | API_OPEN_CREATE |
	    API_OPEN_TRUNC);
	if (out < 0) {
		return (-1);
	}
	if (la_open_entry_data(zip, entry) < 0) {
		code = errno;
		dataClose(out);
		errno = code;
		return (-1);
	}
	if (entry->method == LA_ZIP_METHOD_DEFLATE) {
		ret = la_inflate_entry(zip, out, entry);
	} else {
		ret = la_copy_entry(zip, out, entry);
	}
	if (ret < 0) {
		code = errno;
		dataClose(out);
		errno = code;
		return (-1);
	}
	if (dataClose(out) < 0) {
		return (-1);
	}
	if (zip->on_entry) {
		if (zip->on_entry(entry->name, path, entry->uncomp_size,
		    zip->arg) != 0) {
			return (la_fail(EINTR));
		}
	}
	return (0);
}

static int
la_extract_all(struct la_zip *zip, const struct la_eocd *eocd)
{
	struct la_zip_entry	entry;
	unsigned char		h[46];
	uint64_t		pos, end, next;
	uint32_t		i;
	uint16_t		name_len, extra_len, comment_len;

	pos = eocd->cd_offset;
	end = eocd->cd_offset + eocd->cd_size;
	for (i = 0; i < eocd->entries; i++) {
		if (pos + sizeof(h) > end) {
			return (la_fail(EINVAL));
		}
		if (la_seek(zip->fd, pos) < 0) {
			return (-1);
		}
		if (la_read_full(zip->fd, h, sizeof(h)) < 0) {
			return (-1);
		}
		if (rd32(h) != LA_ZIP_CENTRAL_SIG) {
			return (la_fail(EINVAL));
		}
		name_len = rd16(h + 28);
		extra_len = rd16(h + 30);
		comment_len = rd16(h + 32);
		next = pos + sizeof(h) + name_len + extra_len + comment_len;
		if (next > end) {
			return (la_fail(EINVAL));
		}
		if (la_read_entry(zip, pos, &entry) < 0) {
			return (-1);
		}
		if (la_extract_entry(zip, &entry) < 0) {
			return (-1);
		}
		pos = next;
	}
	if (pos != end) {
		return (la_fail(EINVAL));
	}
	return (0);
}

int
la_zip_extract(const char *zip_path, const struct la_zip_options *opts)
{
	struct api_fs_stat	st;
	struct la_zip		zip;
	struct la_eocd		eocd;
	const char		*dest_dir;
	int			code;

	if (!zip_path || zip_path[0] == '\0') {
		return (la_fail(EINVAL));
	}
	dest_dir = ".";
	if (opts && opts->dest_dir) {
		dest_dir = opts->dest_dir;
	}
	if (dest_dir[0] == '\0') {
		return (la_fail(EINVAL));
	}
	if (fsStat(zip_path, &st) < 0) {
		return (-1);
	}
	if (st.type == API_FS_TYPE_DIR) {
		return (la_fail(EISDIR));
	}
	zip.fd = dataOpen(zip_path, API_OPEN_READ);
	if (zip.fd < 0) {
		return (-1);
	}
	zip.size = st.size;
	zip.dest_dir = dest_dir;
	zip.on_entry = NULL;
	zip.arg = NULL;
	if (opts) {
		zip.on_entry = opts->on_entry;
		zip.arg = opts->arg;
	}
	if (la_mkdirs(dest_dir) < 0 || la_find_eocd(&zip, &eocd) < 0 ||
	    la_extract_all(&zip, &eocd) < 0) {
		code = errno;
		dataClose(zip.fd);
		errno = code;
		return (-1);
	}
	if (dataClose(zip.fd) < 0) {
		return (-1);
	}
	return (0);
}
