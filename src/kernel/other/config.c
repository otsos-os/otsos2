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
$define %type toml_doc_t as struct with toml entries
$define %type toml_entry_t as struct with section, key, value, next
$define %type config_section_cb as function pointer with args const char *, void *

$define %func config_strncmp as function with args const char *, const char *, int
$define %func config_section_prefix as function with args const char *, const char *
$define %func config_section_seen as function with args toml_entry_t *, const char *
$define %func config_value_is_true as function with args const char *
$define %func config_init_from_data as procedure with args const char *, u32
$define %func config_init_from_file as procedure with args const char *
$define %func config_save_to_file as function with args const char *
$define %func config_get as function with args const char *, const char *
$define %func config_get_bool as function with args const char *, const char *, int
$define %func config_get_int as function with args const char *, const char *, int
$define %func config_get_string as function with args const char *, const char *, const char *
$define %func config_set as procedure with args const char *, const char *, const char *
$define %func config_foreach_section as procedure with args const char *, config_section_cb, void *
$define %func config_is_initialized as function with args void
$define %func config_free as procedure with args void

*/

/* !SPACE!

$space %internal config_strncmp, config_section_prefix
$space %internal config_section_seen, config_value_is_true
$space %export config_init_from_data, config_init_from_file
$space %export config_save_to_file, config_get, config_get_bool
$space %export config_get_int, config_get_string, config_set
$space %export config_foreach_section, config_is_initialized
$space %export config_free

*/

#include <kernel/other/config.h>
#include <mlibc/toml.h>
#include <mlibc/mlibc.h>

static toml_doc_t	*g_config_doc;

static int
config_strncmp(const char *a, const char *b, int n)
{
	int	i;

	for (i = 0; i < n; i++) {
		if (a[i] != b[i]) {
			return ((int)(u8)a[i] - (int)(u8)b[i]);
		}
		if (a[i] == '\0') {
			return (0);
		}
	}
	return (0);
}

static int
config_section_prefix(const char *section, const char *prefix)
{
	int	plen;

	if (!section || !prefix) {
		return (0);
	}
	plen = strlen(prefix);
	if (config_strncmp(section, prefix, plen) != 0) {
		return (0);
	}
	return (1);
}

static int
config_section_seen(toml_entry_t *start, const char *section)
{
	toml_entry_t	*e;

	for (e = start; e; e = e->next) {
		if (e == start) {
			continue;
		}
		if (e->section && section &&
		    strcmp(e->section, section) == 0) {
			return (1);
		}
	}
	return (0);
}

static int
config_value_is_true(const char *val)
{
	if (!val) {
		return (0);
	}
	if (strcmp(val, "true") == 0) {
		return (1);
	}
	if (strcmp(val, "1") == 0) {
		return (1);
	}
	if (strcmp(val, "yes") == 0) {
		return (1);
	}
	if (strcmp(val, "on") == 0) {
		return (1);
	}
	return (0);
}

void
config_init_from_data(const char *data, u32 len)
{
	if (g_config_doc) {
		toml_free(g_config_doc);
		g_config_doc = NULL;
	}
	if (!data || len == 0) {
		g_config_doc = toml_new();
		return;
	}
	g_config_doc = toml_parse(data, len);
}

void
config_init_from_file(const char *path)
{
	if (g_config_doc) {
		toml_free(g_config_doc);
		g_config_doc = NULL;
	}
	if (!path) {
		g_config_doc = toml_new();
		return;
	}
	g_config_doc = toml_parse_file(path);
	if (!g_config_doc) {
		g_config_doc = toml_new();
	}
}

int
config_save_to_file(const char *path)
{
	if (!g_config_doc || !path) {
		return (-1);
	}
	return (toml_save(g_config_doc, path));
}

const char *
config_get(const char *section, const char *key)
{
	if (!g_config_doc) {
		return (NULL);
	}
	return (toml_get(g_config_doc, section, key));
}

int
config_get_bool(const char *section, const char *key, int default_val)
{
	const char	*val;

	val = config_get(section, key);
	if (!val) {
		return (default_val);
	}
	return (config_value_is_true(val));
}

int
config_get_int(const char *section, const char *key, int default_val)
{
	const char	*val;

	val = config_get(section, key);
	if (!val) {
		return (default_val);
	}
	return (atoi(val));
}

const char *
config_get_string(const char *section, const char *key,
    const char *default_val)
{
	const char	*val;

	val = config_get(section, key);
	if (!val) {
		return (default_val);
	}
	return (val);
}

void
config_set(const char *section, const char *key, const char *value)
{
	if (!g_config_doc) {
		g_config_doc = toml_new();
		if (!g_config_doc) {
			return;
		}
	}
	toml_set(g_config_doc, section, key, value);
}

void
config_foreach_section(const char *prefix, config_section_cb cb,
    void *ctx)
{
	toml_entry_t	*e;

	if (!g_config_doc || !prefix || !cb) {
		return;
	}
	for (e = g_config_doc->entries; e; e = e->next) {
		if (!e->section) {
			continue;
		}
		if (!config_section_prefix(e->section, prefix)) {
			continue;
		}
		if (config_section_seen(e, e->section)) {
			continue;
		}
		if (cb(e->section, ctx) != 0) {
			break;
		}
	}
}

int
config_is_initialized(void)
{
	return (g_config_doc != NULL);
}

void
config_free(void)
{
	if (g_config_doc) {
		toml_free(g_config_doc);
		g_config_doc = NULL;
	}
}
