/* !DEFINES!

$define %type int as native registry handle
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type api_reg_value as native registry value IO descriptor
$define %type api_reg_entry as native registry enumeration entry
$define %type api_reg_hive as native registry hive enumeration entry

$define %func regGetFixed as function with args int, const char *, uint32_t
$define %func regSetFixed as function with args int, const char *, uint32_t
$define %func regOpen as function with args const char *, const char *, uint32_t
$define %func regClose as function with args int
$define %func regGet as function with args int, api_reg_value *
$define %func regSet as function with args int, api_reg_value *
$define %func regCreateKey as function with args int, const char *
$define %func regDeleteKey as function with args int, const char *
$define %func regDeleteValue as function with args int, const char *
$define %func regEnum as function with args int, api_reg_entry *
$define %func regEnumHives as function with args api_reg_hive *
$define %func regUpd as function with args uint32_t
$define %func regGetBool as function with args int, const char *, int *
$define %func regSetBool as function with args int, const char *, int
$define %func regGetU32 as function with args int, const char *, uint32_t *
$define %func regSetU32 as function with args int, const char *, uint32_t
$define %func regGetIpv4 as function with args int, const char *, uint32_t *
$define %func regSetIpv4 as function with args int, const char *, uint32_t
$define %func regGetString as function with args int, const char *, char *
$define %func regSetString as function with args int, const char *, const char *

*/

/* !SPACE!

$space %internal regGetFixed, regSetFixed
$space %export regOpen, regClose, regGet, regSet, regCreateKey
$space %export regDeleteKey, regDeleteValue, regEnum, regEnumHives
$space %export regUpd
$space %export regGetBool, regSetBool, regGetU32, regSetU32
$space %export regGetIpv4, regSetIpv4, regGetString, regSetString

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

static int
regGetFixed(int reg, const char *name, uint32_t type, void *data,
    uint32_t size)
{
	struct api_reg_value	value;
	ssize_t			ret;

	memset(&value, 0, sizeof(value));
	value.name = name;
	value.data = data;
	value.size = size;

	ret = regGet(reg, &value);
	if (ret < 0) {
		return (-1);
	}
	if (value.type != type || value.bytes != size ||
	    ret != (ssize_t)size) {
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

static int
regSetFixed(int reg, const char *name, uint32_t type, const void *data,
    uint32_t size)
{
	struct api_reg_value	value;

	memset(&value, 0, sizeof(value));
	value.name = name;
	value.data = (void *)data;
	value.size = size;
	value.type = type;
	return (regSet(reg, &value));
}

int
regOpen(const char *hive, const char *key, uint32_t flags)
{
	return (__sysret_int(__syscall3(CALL_REG_OPEN, (long)hive,
	    (long)key, (long)flags)));
}

int
regClose(int reg)
{
	return (__sysret_int(__syscall1(CALL_REG_CLOSE, (long)reg)));
}

ssize_t
regGet(int reg, struct api_reg_value *value)
{
	return (__sysret(__syscall2(CALL_REG_GET, (long)reg,
	    (long)value)));
}

int
regSet(int reg, const struct api_reg_value *value)
{
	return (__sysret_int(__syscall2(CALL_REG_SET, (long)reg,
	    (long)value)));
}

int
regCreateKey(int reg, const char *name)
{
	return (__sysret_int(__syscall2(CALL_REG_CREATE_KEY, (long)reg,
	    (long)name)));
}

int
regDeleteKey(int reg, const char *name)
{
	return (__sysret_int(__syscall2(CALL_REG_DELETE_KEY, (long)reg,
	    (long)name)));
}

int
regDeleteValue(int reg, const char *name)
{
	return (__sysret_int(__syscall2(CALL_REG_DELETE_VALUE, (long)reg,
	    (long)name)));
}

int
regEnum(int reg, struct api_reg_entry *entry)
{
	return (__sysret_int(__syscall2(CALL_REG_ENUM, (long)reg,
	    (long)entry)));
}

int
regEnumHives(struct api_reg_hive *hive)
{
	return (__sysret_int(__syscall1(CALL_REG_ENUM_HIVES,
	    (long)hive)));
}

int
regUpd(uint32_t consumer)
{
	return (__sysret_int(__syscall1(CALL_REG_UPD, (long)consumer)));
}

int
regGetBool(int reg, const char *name, int *out)
{
	uint8_t	buf[1];
	int	ret;

	if (!out) {
		errno = EINVAL;
		return (-1);
	}
	ret = regGetFixed(reg, name, API_REG_TYPE_BOOL, buf, sizeof(buf));
	if (ret != 0) {
		return (ret);
	}
	*out = buf[0] ? 1 : 0;
	return (0);
}

int
regSetBool(int reg, const char *name, int value)
{
	uint8_t	buf[1];

	buf[0] = value ? 1 : 0;
	return (regSetFixed(reg, name, API_REG_TYPE_BOOL, buf,
	    sizeof(buf)));
}

int
regGetU32(int reg, const char *name, uint32_t *out)
{
	uint8_t	buf[4];
	int	ret;

	if (!out) {
		errno = EINVAL;
		return (-1);
	}
	ret = regGetFixed(reg, name, API_REG_TYPE_U32, buf, sizeof(buf));
	if (ret != 0) {
		return (ret);
	}
	*out = ((uint32_t)buf[0]) | ((uint32_t)buf[1] << 8) |
	    ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
	return (0);
}

int
regSetU32(int reg, const char *name, uint32_t value)
{
	uint8_t	buf[4];

	buf[0] = (uint8_t)(value & 0xFF);
	buf[1] = (uint8_t)((value >> 8) & 0xFF);
	buf[2] = (uint8_t)((value >> 16) & 0xFF);
	buf[3] = (uint8_t)((value >> 24) & 0xFF);
	return (regSetFixed(reg, name, API_REG_TYPE_U32, buf,
	    sizeof(buf)));
}

int
regGetIpv4(int reg, const char *name, uint32_t *out)
{
	uint8_t	buf[4];
	int	ret;

	if (!out) {
		errno = EINVAL;
		return (-1);
	}
	ret = regGetFixed(reg, name, API_REG_TYPE_IPV4, buf, sizeof(buf));
	if (ret != 0) {
		return (ret);
	}
	*out = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
	    ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
	return (0);
}

int
regSetIpv4(int reg, const char *name, uint32_t value)
{
	uint8_t	buf[4];

	buf[0] = (uint8_t)((value >> 24) & 0xFF);
	buf[1] = (uint8_t)((value >> 16) & 0xFF);
	buf[2] = (uint8_t)((value >> 8) & 0xFF);
	buf[3] = (uint8_t)(value & 0xFF);
	return (regSetFixed(reg, name, API_REG_TYPE_IPV4, buf,
	    sizeof(buf)));
}

int
regGetString(int reg, const char *name, char *buf, size_t size)
{
	struct api_reg_value	value;
	ssize_t			ret;

	if (!buf || size == 0 || !__count_ok(size)) {
		errno = EINVAL;
		return (-1);
	}

	memset(&value, 0, sizeof(value));
	value.name = name;
	value.data = buf;
	value.size = (uint32_t)(size - 1);

	ret = regGet(reg, &value);
	if (ret < 0) {
		buf[0] = '\0';
		return (-1);
	}
	if (value.type != API_REG_TYPE_STRING ||
	    value.bytes >= (uint32_t)size) {
		buf[0] = '\0';
		errno = EINVAL;
		return (-1);
	}
	buf[value.bytes] = '\0';
	return (0);
}

int
regSetString(int reg, const char *name, const char *value)
{
	if (!value) {
		errno = EINVAL;
		return (-1);
	}
	if (strlen(value) > UINT32_MAX) {
		errno = EINVAL;
		return (-1);
	}
	return (regSetFixed(reg, name, API_REG_TYPE_STRING, value,
	    (uint32_t)strlen(value)));
}
