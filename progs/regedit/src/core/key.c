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

$define %type uint32_t as 32 bit unsigned
$define %type api_reg_entry as native registry enumeration entry
$define %type re_path as hive name plus dot separated key path
$define %type re_keys as bounded list of child key names
$define %type re_values as bounded list of registry values

$define %func re_open as function with args re_path *, uint32_t
$define %func re_name_copy as function with args char *, const char *
$define %func re_keys_sort as procedure with args re_keys *
$define %func re_values_sort as procedure with args re_values *
$define %func re_value_preview as procedure with args int, re_value *
$define %func re_enumerate as function with args re_path *, re_keys *, values
$define %func re_keys_load as function with args re_path *, re_keys *
$define %func re_values_load as function with args re_path *, re_values *
$define %func re_key_create as function with args re_path *, const char *
$define %func re_key_delete as function with args re_path *, const char *
$define %func re_value_delete as function with args re_path *, const char *

*/

/* !SPACE!

$space %internal re_open, re_name_copy, re_keys_sort, re_values_sort
$space %internal re_value_preview, re_enumerate
$space %export re_keys_load, re_values_load
$space %export re_key_create, re_key_delete, re_value_delete

*/

#include <errno.h>
#include <native.h>
#include <regedit/regedit.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RE_PREVIEW_DATA		256

static int
re_open(const re_path_t *path, uint32_t flags)
{
	if (!path || path->hive[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	return (regOpen(path->hive, path->key, flags));
}

static void
re_name_copy(char *dst, const char *src)
{
	size_t	len;

	len = strnlen(src, RE_NAME_MAX - 1);
	memcpy(dst, src, len);
	dst[len] = '\0';
}

static void
re_keys_sort(re_keys_t *list)
{
	re_key_t	tmp;
	uint32_t	i, j;

	for (i = 1; i < list->count; i++) {
		memcpy(&tmp, &list->items[i], sizeof(tmp));
		j = i;
		while (j > 0 && strcmp(list->items[j - 1].name,
		    tmp.name) > 0) {
			memcpy(&list->items[j], &list->items[j - 1],
			    sizeof(tmp));
			j--;
		}
		memcpy(&list->items[j], &tmp, sizeof(tmp));
	}
}

static void
re_values_sort(re_values_t *list)
{
	re_value_t	tmp;
	uint32_t	i, j;

	for (i = 1; i < list->count; i++) {
		memcpy(&tmp, &list->items[i], sizeof(tmp));
		j = i;
		while (j > 0 && strcmp(list->items[j - 1].name,
		    tmp.name) > 0) {
			memcpy(&list->items[j], &list->items[j - 1],
			    sizeof(tmp));
			j--;
		}
		memcpy(&list->items[j], &tmp, sizeof(tmp));
	}
}

static void
re_value_preview(int reg, re_value_t *item)
{
	struct api_reg_value	value;
	uint8_t			data[RE_PREVIEW_DATA];
	char			text[RE_TEXT_MAX];
	ssize_t			ret;

	item->readable = 0;
	if (item->size > sizeof(data)) {
		snprintf(item->preview, sizeof(item->preview),
		    "<%u bytes>", (unsigned int)item->size);
		return;
	}
	memset(&value, 0, sizeof(value));
	memset(data, 0, sizeof(data));
	value.name = item->name;
	value.data = data;
	value.size = sizeof(data);
	ret = regGet(reg, &value);
	if (ret < 0) {
		snprintf(item->preview, sizeof(item->preview), "<%s>",
		    re_error(errno));
		return;
	}
	if (re_format(value.type, data, value.bytes, text,
	    sizeof(text)) != 0) {
		snprintf(item->preview, sizeof(item->preview),
		    "<%u bytes>", (unsigned int)value.bytes);
		return;
	}
	item->type = value.type;
	item->size = value.bytes;
	item->readable = 1;
	snprintf(item->preview, sizeof(item->preview), "%s", text);
}

static int
re_enumerate(const re_path_t *path, re_keys_t *keys, re_values_t *values)
{
	struct api_reg_entry	entry;
	uint32_t		index;
	int			reg, ret;

	if (keys) {
		memset(keys, 0, sizeof(*keys));
	}
	if (values) {
		memset(values, 0, sizeof(*values));
	}
	reg = re_open(path, API_REG_OPEN_READ);
	if (reg < 0) {
		return (-1);
	}
	for (index = 0;; index++) {
		memset(&entry, 0, sizeof(entry));
		entry.index = index;
		ret = regEnum(reg, &entry);
		if (ret < 0) {
			regClose(reg);
			return (-1);
		}
		if (ret == 0) {
			break;
		}
		if (entry.kind == API_REG_KIND_KEY && keys) {
			if (keys->count >= RE_KEYS_MAX) {
				keys->truncated = 1;
				continue;
			}
			re_name_copy(keys->items[keys->count].name,
			    entry.name);
			keys->count++;
			continue;
		}
		if (entry.kind == API_REG_KIND_VALUE && values) {
			if (values->count >= RE_VALUES_MAX) {
				values->truncated = 1;
				continue;
			}
			re_name_copy(values->items[values->count].name,
			    entry.name);
			values->items[values->count].type = entry.type;
			values->items[values->count].size = entry.size;
			re_value_preview(reg,
			    &values->items[values->count]);
			values->count++;
		}
	}
	regClose(reg);
	if (keys) {
		re_keys_sort(keys);
	}
	if (values) {
		re_values_sort(values);
	}
	return (0);
}

int
re_keys_load(const re_path_t *path, re_keys_t *out)
{
	if (!out) {
		errno = EINVAL;
		return (-1);
	}
	return (re_enumerate(path, out, NULL));
}

int
re_values_load(const re_path_t *path, re_values_t *out)
{
	if (!out) {
		errno = EINVAL;
		return (-1);
	}
	return (re_enumerate(path, NULL, out));
}

int
re_key_create(const re_path_t *path, const char *name)
{
	int	reg, ret;

	if (!name || name[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	reg = re_open(path, API_REG_OPEN_WRITE);
	if (reg < 0) {
		return (-1);
	}
	ret = regCreateKey(reg, name);
	regClose(reg);
	return (ret);
}

int
re_key_delete(const re_path_t *path, const char *name)
{
	int	reg, ret;

	if (!name || name[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	reg = re_open(path, API_REG_OPEN_WRITE);
	if (reg < 0) {
		return (-1);
	}
	ret = regDeleteKey(reg, name);
	regClose(reg);
	return (ret);
}

int
re_value_delete(const re_path_t *path, const char *name)
{
	int	reg, ret;

	if (!name || name[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	reg = re_open(path, API_REG_OPEN_WRITE);
	if (reg < 0) {
		return (-1);
	}
	ret = regDeleteValue(reg, name);
	regClose(reg);
	return (ret);
}
