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
$define %type api_reg_hive as native registry hive enumeration entry
$define %type re_hive as one registry hive with resolved access mask
$define %type re_hives as bounded list of discovered registry hives

$define %func re_hive_access as function with args uint32_t
$define %func re_hive_add as function with args re_hives *, name, uint32_t
$define %func re_hive_sort as procedure with args re_hives *
$define %func re_hives_load as function with args re_hives *

*/

/* !SPACE!

$space %internal re_hive_access, re_hive_add, re_hive_sort
$space %export re_hives_load

*/

#include <errno.h>
#include <native.h>
#include <regedit/regedit.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t
re_hive_access(uint32_t mask)
{
	uint32_t	access;

	access = RE_ACCESS_NONE;
	if (mask & API_REG_HIVE_CAN_READ) {
		access |= RE_ACCESS_READ;
	}
	if (mask & (API_REG_HIVE_CAN_ADD | API_REG_HIVE_CAN_EDIT)) {
		access |= RE_ACCESS_WRITE;
	}
	return (access);
}

static int
re_hive_add(re_hives_t *list, const char *name, uint32_t mask)
{
	size_t	len;

	if (!name || name[0] == '\0') {
		return (-1);
	}
	len = strnlen(name, RE_NAME_MAX);
	if (len >= RE_NAME_MAX) {
		return (-1);
	}
	if (list->count >= RE_HIVE_MAX) {
		return (-1);
	}
	memset(&list->items[list->count], 0, sizeof(list->items[0]));
	memcpy(list->items[list->count].name, name, len);
	list->items[list->count].access = re_hive_access(mask);
	list->count++;
	return (0);
}

static void
re_hive_sort(re_hives_t *list)
{
	re_hive_t	tmp;
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

int
re_hives_load(re_hives_t *out)
{
	struct api_reg_hive	hive;
	uint32_t		index;
	int			ret;

	if (!out) {
		errno = EINVAL;
		return (-1);
	}
	memset(out, 0, sizeof(*out));
	for (index = 0; index < RE_HIVE_MAX; index++) {
		memset(&hive, 0, sizeof(hive));
		hive.index = index;
		ret = regEnumHives(&hive);
		if (ret < 0) {
			return (-1);
		}
		if (ret == 0) {
			break;
		}
		if (re_hive_add(out, hive.name, hive.access) != 0) {
			break;
		}
	}
	re_hive_sort(out);
	return (0);
}
