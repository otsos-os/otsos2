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

$define %func tar_octal as function with args const char *, u32, u32 *
$define %func tar_empty as function with args const tar_header_t *
$define %func tar_name_len as function with args const tar_header_t *
$define %func tar_magic_ok as function with args const tar_header_t *
$define %func tar_checksum as function with args const tar_header_t *
$define %func tar_checksum_ok as function with args const tar_header_t *
$define %func tar_type_ok as function with args const tar_header_t *
$define %func tar_header_ok as function with args const tar_header_t *
$define %func tar_regular as function with args const tar_header_t *
$define %func tar_name as function with args const tar_header_t *
$define %func fill_file as function with args const tar_header_t *, bootpack_file_t *
$define %func bootpack_init as procedure with args bootpack_t *, const void *, u32
$define %func bootpack_find as function with args bootpack_t *, const char *, bootpack_file_t *
$define %func bootpack_foreach as function with args bootpack_t *, bootpack_iter_cb, void *

*/

/* !SPACE!

$space %internal tar_octal, tar_empty, tar_name_len, tar_magic_ok
$space %internal tar_checksum, tar_checksum_ok, tar_type_ok
$space %internal tar_header_ok, tar_regular, tar_name, fill_file
$space %export bootpack_init, bootpack_find, bootpack_foreach

*/

#include <boot/bootloader/lib/bootpack.h>
#include <boot/bootloader/lib/string.h>

#define TAR_BLOCK_SIZE	512
#define TAR_NAME_BAD	0xffffffffU
#define TAR_CHKSUM_OFF	148
#define TAR_CHKSUM_LEN	8

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

static int
tar_octal(const char *str, u32 len, u32 *out)
{
	u32	i, value;
	char	c;

	value = 0;
	for (i = 0; i < len; i++) {
		c = str[i];
		if (c == 0 || c == ' ') {
			continue;
		}
		if (c < '0' || c > '7') {
			return (-1);
		}
		if (value > 0x1fffffffU) {
			return (-1);
		}
		value = (value << 3) + (u32)(c - '0');
	}
	*out = value;
	return (0);
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

static u32
tar_name_len(const tar_header_t *hdr)
{
	u32	i;

	for (i = 0; i < sizeof(hdr->name); i++) {
		if (hdr->name[i] == 0) {
			return (i);
		}
	}
	return (TAR_NAME_BAD);
}

static int
tar_magic_ok(const tar_header_t *hdr)
{
	return (hdr->magic[0] == 'u' && hdr->magic[1] == 's' &&
	    hdr->magic[2] == 't' && hdr->magic[3] == 'a' &&
	    hdr->magic[4] == 'r');
}

static u32
tar_checksum(const tar_header_t *hdr)
{
	const u8	*p;
	u32		i, sum;

	p = (const u8 *)hdr;
	sum = 0;
	for (i = 0; i < TAR_BLOCK_SIZE; i++) {
		if (i >= TAR_CHKSUM_OFF &&
		    i < TAR_CHKSUM_OFF + TAR_CHKSUM_LEN) {
			sum += ' ';
		} else {
			sum += p[i];
		}
	}
	return (sum);
}

static int
tar_checksum_ok(const tar_header_t *hdr)
{
	u32	want, sum;

	if (tar_octal(hdr->chksum, sizeof(hdr->chksum), &want) != 0) {
		return (0);
	}
	sum = tar_checksum(hdr);
	return (sum == want);
}

static int
tar_type_ok(const tar_header_t *hdr)
{
	return (hdr->typeflag == 0 || hdr->typeflag == '0' ||
	    hdr->typeflag == '5');
}

static int
tar_header_ok(const tar_header_t *hdr)
{
	u32	len, size;

	if (!tar_magic_ok(hdr) || !tar_checksum_ok(hdr) ||
	    !tar_type_ok(hdr)) {
		return (0);
	}
	if (hdr->prefix[0] != 0) {
		return (0);
	}
	len = tar_name_len(hdr);
	if (len == TAR_NAME_BAD || len == 0) {
		return (0);
	}
	if (tar_octal(hdr->size, sizeof(hdr->size), &size) != 0) {
		return (0);
	}
	return (1);
}

static int
tar_regular(const tar_header_t *hdr)
{
	return (hdr->typeflag == 0 || hdr->typeflag == '0');
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

static int
fill_file(const tar_header_t *hdr, bootpack_file_t *out)
{
	u32	size;

	if (tar_octal(hdr->size, sizeof(hdr->size), &size) != 0) {
		return (-1);
	}
	out->name = tar_name(hdr);
	out->data = (const u8 *)hdr + TAR_BLOCK_SIZE;
	out->size = size;
	return (0);
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
	u32		off, file_size, next, rounded;

	off = 0;
	while (off + TAR_BLOCK_SIZE <= pack->size) {
		hdr = (const tar_header_t *)(pack->data + off);
		if (tar_empty(hdr)) {
			break;
		}
		if (!tar_header_ok(hdr) || fill_file(hdr, &file) != 0) {
			return (-1);
		}
		file_size = file.size;
		if (file_size > 0xfffffe00U) {
			return (-1);
		}
		rounded = (file_size + 511U) & ~511U;
		if (rounded > 0xffffffffU - TAR_BLOCK_SIZE) {
			return (-1);
		}
		next = TAR_BLOCK_SIZE + rounded;
		if (next > pack->size - off) {
			return (-1);
		}
		if (tar_regular(hdr) && bl_strcmp(file.name, name) == 0) {
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
	u32		off, file_size, next, rounded;
	int		rc;

	off = 0;
	while (off + TAR_BLOCK_SIZE <= pack->size) {
		hdr = (const tar_header_t *)(pack->data + off);
		if (tar_empty(hdr)) {
			break;
		}
		if (!tar_header_ok(hdr) || fill_file(hdr, &file) != 0) {
			return (-1);
		}
		file_size = file.size;
		if (file_size > 0xfffffe00U) {
			return (-1);
		}
		rounded = (file_size + 511U) & ~511U;
		if (rounded > 0xffffffffU - TAR_BLOCK_SIZE) {
			return (-1);
		}
		next = TAR_BLOCK_SIZE + rounded;
		if (next > pack->size - off) {
			return (-1);
		}
		if (tar_regular(hdr)) {
			rc = cb(&file, ctx);
			if (rc != 0) {
				return (rc);
			}
		}
		off += next;
	}
	return (0);
}
