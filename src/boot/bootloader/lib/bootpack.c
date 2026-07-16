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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type tar_header_t as ustar header
$define %type bootpack_t as tar-backed boot payload
$define %type bootpack_file_t as file entry inside boot payload
$define %type bootpack_iter_cb as callback for each file

$define %func tar_octal as function with args const char *, u32
$define %func tar_empty as function with args const tar_header_t *
$define %func tar_name as function with args const tar_header_t *
$define %func fill_file as procedure with args const tar_header_t *, bootpack_file_t *
$define %func bootpack_init as procedure with args bootpack_t *, const void *, u32
$define %func bootpack_find as function with args bootpack_t *, const char *, bootpack_file_t *
$define %func bootpack_foreach as function with args bootpack_t *, bootpack_iter_cb, void *

*/

/* !SPACE!

$space %internal tar_octal, tar_empty, tar_name, fill_file
$space %export bootpack_init, bootpack_find, bootpack_foreach

*/

#include <boot/bootloader/lib/bootpack.h>
#include <boot/bootloader/lib/string.h>

#define TAR_BLOCK_SIZE	512

typedef struct {
	char	name[100];
	char	mode[8];
	char	uid[8];
	char	gid[8];
	char	size[12];
	char	mtime[12];
	char	chksum[8];
	char	typeflag;
	char	linkname[100];
	char	magic[6];
	char	version[2];
	char	uname[32];
	char	gname[32];
	char	devmajor[8];
	char	devminor[8];
	char	prefix[155];
	char	pad[12];
} __attribute__((packed)) tar_header_t;

static u32
tar_octal(const char *str, u32 len)
{
	u32	i, value;

	value = 0;
	for (i = 0; i < len; i++) {
		if (str[i] == 0 || str[i] == ' ') {
			continue;
		}
		if (str[i] < '0' || str[i] > '7') {
			break;
		}
		value = (value << 3) + (u32)(str[i] - '0');
	}
	return (value);
}

static int
tar_empty(const tar_header_t *hdr)
{
	const u8	*p;
	u32		i;

	p = (const u8 *)hdr;
	for (i = 0; i < TAR_BLOCK_SIZE; i++) {
		if (p[i] != 0) {
			return (0);
		}
	}
	return (1);
}

static const char *
tar_name(const tar_header_t *hdr)
{
	const char	*name;

	name = hdr->name;
	if (name[0] == '.' && name[1] == '/') {
		name += 2;
	}
	return (name);
}

static void
fill_file(const tar_header_t *hdr, bootpack_file_t *out)
{
	out->name = tar_name(hdr);
	out->data = (const u8 *)hdr + TAR_BLOCK_SIZE;
	out->size = tar_octal(hdr->size, sizeof(hdr->size));
}

void
bootpack_init(bootpack_t *pack, const void *data, u32 size)
{
	pack->data = (const u8 *)data;
	pack->size = size;
}

int
bootpack_find(bootpack_t *pack, const char *name, bootpack_file_t *out)
{
	bootpack_file_t	file;
	const tar_header_t	*hdr;
	u32		off, file_size, next;

	off = 0;
	while (off + TAR_BLOCK_SIZE <= pack->size) {
		hdr = (const tar_header_t *)(pack->data + off);
		if (tar_empty(hdr)) {
			break;
		}
		fill_file(hdr, &file);
		file_size = file.size;
		next = TAR_BLOCK_SIZE + ((file_size + 511) & ~511U);
		if (off + next > pack->size) {
			return (-1);
		}
		if (hdr->typeflag != '5' && bl_strcmp(file.name, name) == 0) {
			*out = file;
			return (0);
		}
		off += next;
	}
	return (-1);
}

int
bootpack_foreach(bootpack_t *pack, bootpack_iter_cb cb, void *ctx)
{
	bootpack_file_t	file;
	const tar_header_t	*hdr;
	u32		off, file_size, next;
	int		rc;

	off = 0;
	while (off + TAR_BLOCK_SIZE <= pack->size) {
		hdr = (const tar_header_t *)(pack->data + off);
		if (tar_empty(hdr)) {
			break;
		}
		fill_file(hdr, &file);
		file_size = file.size;
		next = TAR_BLOCK_SIZE + ((file_size + 511) & ~511U);
		if (off + next > pack->size) {
			return (-1);
		}
		if (hdr->typeflag != '5') {
			rc = cb(&file, ctx);
			if (rc != 0) {
				return (rc);
			}
		}
		off += next;
	}
	return (0);
}
