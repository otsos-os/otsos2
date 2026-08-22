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

$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type size_t as native object size
$define %type api_reg_value as native registry value IO descriptor
$define %type re_path as hive name plus dot separated key path
$define %type re_consumer_map as consumer name to identifier row

$define %func re_value_read as function with args re_path *, name, out
$define %func re_value_write as function with args re_path *, name, type, text
$define %func re_consumer_update as function with args uint32_t
$define %func re_consumer_name as function with args uint32_t
$define %func re_consumer_id as function with args const char *
$define %func re_error as function with args int

*/

/* !SPACE!

$space %export re_value_read, re_value_write
$space %export re_consumer_update, re_consumer_name, re_consumer_id
$space %export re_error

*/

#include <errno.h>
#include <native.h>
#include <regedit/regedit.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct re_consumer_map {
	const char	*name;
	uint32_t	id;
} re_consumer_map_t;

static const re_consumer_map_t	re_consumers[] = {
	{ "net", API_REG_CONSUMER_NET },
	{ "scheduler", API_REG_CONSUMER_SCHEDULER },
	{ "kusr", API_REG_CONSUMER_KUSR },
	{ "console", API_REG_CONSUMER_CONSOLE },
	{ "input", API_REG_CONSUMER_INPUT }
};

int
re_value_read(const re_path_t *path, const char *name, uint32_t *type,
    void *data, size_t size, uint32_t *bytes)
{
	struct api_reg_value	value;
	ssize_t			ret;
	int			reg;

	if (!path || !name || name[0] == '\0' || !data || size == 0 ||
	    !type || !bytes) {
		errno = EINVAL;
		return (-1);
	}
	*type = 0;
	*bytes = 0;
	reg = regOpen(path->hive, path->key, API_REG_OPEN_READ);
	if (reg < 0) {
		return (-1);
	}
	memset(&value, 0, sizeof(value));
	value.name = name;
	value.data = data;
	value.size = (uint32_t)size;
	ret = regGet(reg, &value);
	regClose(reg);
	if (ret < 0) {
		return (-1);
	}
	*type = value.type;
	*bytes = value.bytes;
	return (0);
}

int
re_value_write(const re_path_t *path, const char *name, uint32_t type,
    const char *text)
{
	struct api_reg_value	value;
	uint8_t			data[RE_DATA_MAX];
	uint32_t		bytes;
	int			reg, ret;

	if (!path || !name || name[0] == '\0' || !text) {
		errno = EINVAL;
		return (-1);
	}
	memset(data, 0, sizeof(data));
	bytes = 0;
	if (re_parse(type, text, data, sizeof(data), &bytes) != 0) {
		return (-1);
	}
	reg = regOpen(path->hive, path->key, API_REG_OPEN_WRITE);
	if (reg < 0) {
		return (-1);
	}
	memset(&value, 0, sizeof(value));
	value.name = name;
	value.data = data;
	value.size = bytes;
	value.type = type;
	ret = regSet(reg, &value);
	regClose(reg);
	return (ret);
}

int
re_consumer_update(uint32_t consumer)
{
	if (consumer == 0) {
		errno = EINVAL;
		return (-1);
	}
	return (regUpd(consumer));
}

const char *
re_consumer_name(uint32_t consumer)
{
	size_t	i;

	for (i = 0; i < sizeof(re_consumers) /
	    sizeof(re_consumers[0]); i++) {
		if (re_consumers[i].id == consumer) {
			return (re_consumers[i].name);
		}
	}
	return ("unknown");
}

uint32_t
re_consumer_id(const char *name)
{
	size_t	i;

	if (!name) {
		return (0);
	}
	for (i = 0; i < sizeof(re_consumers) /
	    sizeof(re_consumers[0]); i++) {
		if (strcmp(re_consumers[i].name, name) == 0) {
			return (re_consumers[i].id);
		}
	}
	return (0);
}

const char *
re_error(int code)
{
	switch (code) {
	case 0:
		return ("ok");
	case EPERM:
		return ("operation not permitted");
	case ENOENT:
		return ("not found");
	case EIO:
		return ("io error");
	case E2BIG:
		return ("too large");
	case EBADF:
		return ("bad handle");
	case EAGAIN:
		return ("try again");
	case ENOMEM:
		return ("out of memory");
	case EACCES:
		return ("access denied");
	case EFAULT:
		return ("bad address");
	case EINVAL:
		return ("invalid value");
	case EBUSY:
		return ("busy");
	case EEXIST:
		return ("already exists");
	case ENOTDIR:
		return ("not a key");
	case EISDIR:
		return ("is a key");
	case ENOSPC:
		return ("no space left");
	case EROFS:
		return ("read only");
	case ENOSYS:
		return ("not implemented");
	case ENOTEMPTY:
		return ("key not empty");
	case EOVERFLOW:
		return ("value out of range");
	case ENOTSUP:
		return ("not supported");
	default:
		return ("registry error");
	}
}
