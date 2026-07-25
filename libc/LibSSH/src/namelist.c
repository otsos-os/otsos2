/* !DEFINES!

$define %type lssh_slice as borrowed byte span
$define %type size_t as object size
$define %func lssh_namelist_next as function with args lssh_slice, size_t *, lssh_slice *
$define %func lssh_namelist_contains_slice as function with args lssh_slice, lssh_slice
$define %func lssh_namelist_contains as function with args lssh_slice, const char *
$define %func lssh_namelist_first_match as function with args lssh_slice, lssh_slice, char *, size_t

*/

/* !SPACE!

$space %internal lssh_namelist_next, lssh_namelist_contains_slice
$space %export lssh_namelist_contains, lssh_namelist_first_match

*/

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

#include <libssh.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int
lssh_namelist_next(lssh_slice list, size_t *off, lssh_slice *name)
{
	size_t	start, end;
	uint8_t	ch;

	if (!off || !name || *off > list.len ||
	    (!list.data && list.len != 0)) {
		return (LSSH_ERR_INVALID);
	}
	if (*off == list.len) {
		name->data = NULL;
		name->len = 0;
		return (0);
	}
	start = *off;
	end = start;
	while (end < list.len && list.data[end] != ',') {
		ch = list.data[end];
		if (ch <= 32 || ch >= 127) {
			return (LSSH_ERR_FORMAT);
		}
		end++;
	}
	if (end == start) {
		return (LSSH_ERR_FORMAT);
	}
	if (end < list.len && end + 1 == list.len) {
		return (LSSH_ERR_FORMAT);
	}
	name->data = list.data + start;
	name->len = end - start;
	if (end < list.len) {
		*off = end + 1;
	} else {
		*off = end;
	}
	return (1);
}

static int
lssh_namelist_contains_slice(lssh_slice list, lssh_slice needle)
{
	lssh_slice	name;
	size_t		off;
	int		ret;

	if (!needle.data || needle.len == 0) {
		return (LSSH_ERR_INVALID);
	}
	off = 0;
	for (;;) {
		ret = lssh_namelist_next(list, &off, &name);
		if (ret <= 0) {
			return (ret);
		}
		if (name.len == needle.len &&
		    memcmp(name.data, needle.data, name.len) == 0) {
			return (1);
		}
	}
}

int
lssh_namelist_contains(lssh_slice list, const char *name)
{
	lssh_slice	needle;
	int		ret;

	if (!name || name[0] == '\0') {
		return (0);
	}
	needle.data = (const uint8_t *)name;
	needle.len = strlen(name);
	ret = lssh_namelist_contains_slice(list, needle);
	return (ret == 1);
}

int
lssh_namelist_first_match(lssh_slice preferred, lssh_slice available,
    char *out, size_t out_size)
{
	lssh_slice	name;
	size_t		off;
	int		ret;

	if (!out || out_size == 0) {
		return (LSSH_ERR_INVALID);
	}
	out[0] = '\0';
	off = 0;
	for (;;) {
		ret = lssh_namelist_next(preferred, &off, &name);
		if (ret < 0) {
			return (ret);
		}
		if (ret == 0) {
			return (LSSH_ERR_NO_MATCH);
		}
		ret = lssh_namelist_contains_slice(available, name);
		if (ret < 0) {
			return (ret);
		}
		if (ret == 1) {
			if (name.len + 1 > out_size) {
				return (LSSH_ERR_RANGE);
			}
			memcpy(out, name.data, name.len);
			out[name.len] = '\0';
			return (LSSH_OK);
		}
	}
}
