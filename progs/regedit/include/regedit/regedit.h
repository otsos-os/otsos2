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
$define %type uint64_t as 64 bit unsigned
$define %type size_t as native object size
$define %type re_hive as one discovered registry hive with read probe result
$define %type re_hives as bounded list of discovered registry hives
$define %type re_key as one child key name of the current registry key
$define %type re_keys as bounded list of child key names
$define %type re_value as one registry value with type, size and preview text
$define %type re_values as bounded list of registry values
$define %type re_path as hive name plus dot separated key path

$define %func re_path_reset as procedure with args re_path *
$define %func re_path_set_hive as function with args re_path *, const char *
$define %func re_path_push as function with args re_path *, const char *
$define %func re_path_pop as function with args re_path *
$define %func re_path_text as function with args re_path *, char *, size_t
$define %func re_path_parse as function with args re_path *, const char *
$define %func re_path_leaf as function with args const char *
$define %func re_hives_load as function with args re_hives *
$define %func re_keys_load as function with args re_path *, re_keys *
$define %func re_values_load as function with args re_path *, re_values *
$define %func re_value_read as function with args re_path *, name, out
$define %func re_value_write as function with args re_path *, name, type, text
$define %func re_key_create as function with args re_path *, const char *
$define %func re_key_delete as function with args re_path *, const char *
$define %func re_value_delete as function with args re_path *, const char *
$define %func re_consumer_update as function with args uint32_t
$define %func re_consumer_name as function with args uint32_t
$define %func re_consumer_id as function with args const char *
$define %func re_type_name as function with args uint32_t
$define %func re_type_id as function with args const char *
$define %func re_type_hint as function with args uint32_t
$define %func re_format as function with args type, data, size, char *, size_t
$define %func re_parse as function with args type, text, void *, size_t, out
$define %func re_error as function with args int

*/

/* !SPACE!

$space %export re_hive_t, re_hives_t, re_key_t, re_keys_t
$space %export re_value_t, re_values_t, re_path_t
$space %export re_path_reset, re_path_set_hive, re_path_push, re_path_pop
$space %export re_path_text, re_path_parse, re_path_leaf
$space %export re_hives_load, re_keys_load, re_values_load
$space %export re_value_read, re_value_write
$space %export re_key_create, re_key_delete, re_value_delete
$space %export re_consumer_update, re_consumer_name, re_consumer_id
$space %export re_type_name, re_type_id, re_type_hint
$space %export re_format, re_parse, re_error

*/

#ifndef PROGS_REGEDIT_REGEDIT_H
#define PROGS_REGEDIT_REGEDIT_H

#include <stddef.h>
#include <stdint.h>

#define RE_NAME_MAX		32
#define RE_KEY_MAX		192
#define RE_PATH_MAX		256
#define RE_HIVE_MAX		16
#define RE_KEYS_MAX		128
#define RE_VALUES_MAX		128
#define RE_DATA_MAX		4096
#define RE_TEXT_MAX		1024
#define RE_PREVIEW_MAX		96

#define RE_ACCESS_NONE		0
#define RE_ACCESS_READ		1
#define RE_ACCESS_WRITE		2

typedef struct re_hive {
	uint32_t	access;
	char		name[RE_NAME_MAX];
} re_hive_t;

typedef struct re_hives {
	uint32_t	count;
	re_hive_t	items[RE_HIVE_MAX];
} re_hives_t;

typedef struct re_key {
	char	name[RE_NAME_MAX];
} re_key_t;

typedef struct re_keys {
	uint32_t	count;
	uint32_t	truncated;
	re_key_t	items[RE_KEYS_MAX];
} re_keys_t;

typedef struct re_value {
	uint32_t	type;
	uint32_t	size;
	uint32_t	readable;
	char		name[RE_NAME_MAX];
	char		preview[RE_PREVIEW_MAX];
} re_value_t;

typedef struct re_values {
	uint32_t	count;
	uint32_t	truncated;
	re_value_t	items[RE_VALUES_MAX];
} re_values_t;

typedef struct re_path {
	char	hive[RE_NAME_MAX];
	char	key[RE_KEY_MAX];
} re_path_t;

void		re_path_reset(re_path_t *path);
int		re_path_set_hive(re_path_t *path, const char *hive);
int		re_path_push(re_path_t *path, const char *name);
int		re_path_pop(re_path_t *path);
int		re_path_text(const re_path_t *path, char *out, size_t size);
int		re_path_parse(re_path_t *path, const char *text);
const char	*re_path_leaf(const char *key);

int		re_hives_load(re_hives_t *out);
int		re_keys_load(const re_path_t *path, re_keys_t *out);
int		re_values_load(const re_path_t *path, re_values_t *out);

int		re_value_read(const re_path_t *path, const char *name,
		    uint32_t *type, void *data, size_t size, uint32_t *bytes);
int		re_value_write(const re_path_t *path, const char *name,
		    uint32_t type, const char *text);
int		re_key_create(const re_path_t *path, const char *name);
int		re_key_delete(const re_path_t *path, const char *name);
int		re_value_delete(const re_path_t *path, const char *name);

int		re_consumer_update(uint32_t consumer);
const char	*re_consumer_name(uint32_t consumer);
uint32_t	re_consumer_id(const char *name);

const char	*re_type_name(uint32_t type);
uint32_t	re_type_id(const char *name);
const char	*re_type_hint(uint32_t type);

int		re_format(uint32_t type, const void *data, uint32_t size,
		    char *out, size_t out_size);
int		re_parse(uint32_t type, const char *text, void *out,
		    size_t out_size, uint32_t *bytes);
const char	*re_error(int code);

#endif
