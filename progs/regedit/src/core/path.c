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

$define %type size_t as native object size
$define %type re_path as hive name plus dot separated key path

$define %func re_copy as function with args char *, size_t, const char *
$define %func re_is_sep as function with args int
$define %func re_name_ok as function with args const char *
$define %func re_path_reset as procedure with args re_path *
$define %func re_path_set_hive as function with args re_path *, const char *
$define %func re_path_push as function with args re_path *, const char *
$define %func re_path_pop as function with args re_path *
$define %func re_path_text as function with args re_path *, char *, size_t
$define %func re_path_parse as function with args re_path *, const char *
$define %func re_path_leaf as function with args const char *

*/

/* !SPACE!

$space %internal re_copy, re_is_sep, re_name_ok
$space %export re_path_reset, re_path_set_hive, re_path_push, re_path_pop
$space %export re_path_text, re_path_parse, re_path_leaf

*/

#include <errno.h>
#include <regedit/regedit.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int
re_copy(char *dst, size_t size, const char *src)
{
	size_t	len;

	if (!dst || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (!src) {
		src = "";
	}
	len = strlen(src);
	if (len + 1 > size) {
		dst[0] = '\0';
		errno = E2BIG;
		return (-1);
	}
	memcpy(dst, src, len + 1);
	return (0);
}

static int
re_is_sep(int c)
{
	return (c == '.' || c == '/' || c == '\\');
}

static int
re_name_ok(const char *name)
{
	size_t	i;

	if (!name || name[0] == '\0') {
		return (0);
	}
	for (i = 0; name[i] != '\0'; i++) {
		if (re_is_sep((unsigned char)name[i])) {
			return (0);
		}
	}
	return (i + 1 <= RE_NAME_MAX);
}

void
re_path_reset(re_path_t *path)
{
	if (!path) {
		return;
	}
	memset(path, 0, sizeof(*path));
}

int
re_path_set_hive(re_path_t *path, const char *hive)
{
	if (!path) {
		errno = EINVAL;
		return (-1);
	}
	if (hive && hive[0] != '\0' && !re_name_ok(hive)) {
		errno = EINVAL;
		return (-1);
	}
	memset(path, 0, sizeof(*path));
	return (re_copy(path->hive, sizeof(path->hive), hive));
}

int
re_path_push(re_path_t *path, const char *name)
{
	size_t	used, add;

	if (!path || path->hive[0] == '\0' || !re_name_ok(name)) {
		errno = EINVAL;
		return (-1);
	}
	used = strlen(path->key);
	add = strlen(name);
	if (used != 0) {
		add += 1;
	}
	if (used + add + 1 > sizeof(path->key)) {
		errno = E2BIG;
		return (-1);
	}
	if (used != 0) {
		path->key[used] = '.';
		used++;
	}
	memcpy(path->key + used, name, strlen(name) + 1);
	return (0);
}

int
re_path_pop(re_path_t *path)
{
	size_t	i, len;

	if (!path) {
		errno = EINVAL;
		return (-1);
	}
	len = strlen(path->key);
	if (len == 0) {
		if (path->hive[0] == '\0') {
			return (1);
		}
		path->hive[0] = '\0';
		return (0);
	}
	for (i = len; i > 0; i--) {
		if (path->key[i - 1] == '.') {
			path->key[i - 1] = '\0';
			return (0);
		}
	}
	path->key[0] = '\0';
	return (0);
}

int
re_path_text(const re_path_t *path, char *out, size_t size)
{
	size_t	hive_len, key_len;

	if (!path || !out || size == 0) {
		errno = EINVAL;
		return (-1);
	}
	out[0] = '\0';
	if (path->hive[0] == '\0') {
		return (re_copy(out, size, "(hives)"));
	}
	hive_len = strlen(path->hive);
	key_len = strlen(path->key);
	if (hive_len + (key_len != 0 ? key_len + 1 : 0) + 1 > size) {
		errno = E2BIG;
		return (-1);
	}
	memcpy(out, path->hive, hive_len);
	out[hive_len] = '\0';
	if (key_len != 0) {
		out[hive_len] = '.';
		memcpy(out + hive_len + 1, path->key, key_len + 1);
	}
	return (0);
}

int
re_path_parse(re_path_t *path, const char *text)
{
	char	norm[RE_PATH_MAX];
	char	*split;
	size_t	i, pos;

	if (!path || !text) {
		errno = EINVAL;
		return (-1);
	}
	pos = 0;
	for (i = 0; text[i] != '\0'; i++) {
		if (pos + 1 >= sizeof(norm)) {
			errno = E2BIG;
			return (-1);
		}
		if (re_is_sep((unsigned char)text[i])) {
			if (pos == 0 || norm[pos - 1] == '.') {
				continue;
			}
			norm[pos++] = '.';
			continue;
		}
		norm[pos++] = text[i];
	}
	while (pos > 0 && norm[pos - 1] == '.') {
		pos--;
	}
	norm[pos] = '\0';
	if (pos == 0) {
		re_path_reset(path);
		return (0);
	}
	split = strchr(norm, '.');
	if (!split) {
		return (re_path_set_hive(path, norm));
	}
	*split = '\0';
	if (re_path_set_hive(path, norm) != 0) {
		return (-1);
	}
	return (re_copy(path->key, sizeof(path->key), split + 1));
}

const char *
re_path_leaf(const char *key)
{
	const char	*last;

	if (!key || key[0] == '\0') {
		return ("");
	}
	last = strrchr(key, '.');
	if (!last) {
		return (key);
	}
	return (last + 1);
}
