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
$define %func config_copy_path as procedure with args const char *
$define %func config_file_exists as function with args const char *
$define %func config_read_autoupdate as procedure with args void
$define %func config_schedule_next as procedure with args void
$define %func config_init_from_data as procedure with args const char *, u32
$define %func config_init_from_file as procedure with args const char *
$define %func config_attach_file as function with args const char *
$define %func config_reload_from_file as function with args void
$define %func config_flush_to_file as function with args void
$define %func config_poll as procedure with args void
$define %func config_save_to_file as function with args const char *
$define %func config_get as function with args const char *, const char *
$define %func config_get_bool as function with args const char *, const char *, int
$define %func config_get_int as function with args const char *, const char *, int
$define %func config_get_string as function with args const char *, const char *, const char *
$define %func config_set as procedure with args const char *, const char *, const char *
$define %func config_foreach_section as procedure with args const char *, config_section_cb, void *
$define %func config_foreach_in_section as procedure with args const char *, config_kv_cb, void *
$define %func config_is_initialized as function with args void
$define %func config_free as procedure with args void

*/

/* !SPACE!

$space %internal config_strncmp, config_section_prefix
$space %internal config_section_seen, config_value_is_true
$space %internal config_copy_path, config_file_exists
$space %internal config_read_autoupdate, config_schedule_next
$space %export config_init_from_data, config_init_from_file
$space %export config_attach_file, config_reload_from_file
$space %export config_flush_to_file, config_poll
$space %export config_save_to_file, config_get, config_get_bool
$space %export config_get_int, config_get_string, config_set
$space %export config_foreach_section, config_foreach_in_section
$space %export config_is_initialized
$space %export config_free

*/

#include <kernel/other/config.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/timer.h>
#include <mlibc/toml.h>
#include <mlibc/mlibc.h>

#define	CONFIG_PATH_MAX		256

static toml_doc_t	*g_config_doc;
static char		g_config_path[CONFIG_PATH_MAX];
static int		g_config_fs_ready;
static int		g_config_dirty;
static int		g_config_syncing;
static int		g_config_autoupdate_enabled;
static int		g_config_autosave;
static u32		g_config_autoupdate_seconds;
static u64		g_config_next_tick;

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

static void
config_copy_path(const char *path)
{
	int	i;

	memset(g_config_path, 0, sizeof(g_config_path));
	if (!path) {
		return;
	}
	for (i = 0; i < CONFIG_PATH_MAX - 1 && path[i] != '\0'; i++) {
		g_config_path[i] = path[i];
	}
	g_config_path[i] = '\0';
}

static int
config_file_exists(const char *path)
{
	vnode_t	*vn;

	if (!path || !vfs_is_initialized()) {
		return (0);
	}
	vn = NULL;
	if (vfs_resolve(path, &vn) != 0 || !vn) {
		return (0);
	}
	vnode_release(vn);
	return (1);
}

static void
config_read_autoupdate(void)
{
	int	seconds;

	g_config_autoupdate_enabled =
	    config_get_bool("autoupdate", "enabled", 0);
	g_config_autosave = config_get_bool("autoupdate", "autosave", 1);
	seconds = config_get_int("autoupdate", "seconds", 0);
	if (seconds <= 0) {
		g_config_autoupdate_enabled = 0;
		g_config_autoupdate_seconds = 0;
		return;
	}
	g_config_autoupdate_seconds = (u32)seconds;
}

static void
config_schedule_next(void)
{
	u64	ticks;
	u32	freq;

	g_config_next_tick = 0;
	if (!g_config_autoupdate_enabled ||
	    g_config_autoupdate_seconds == 0) {
		return;
	}
	if (!timer_is_initialized()) {
		return;
	}

	freq = timer_get_frequency();
	if (freq == 0) {
		freq = 1;
	}
	ticks = (u64)g_config_autoupdate_seconds * (u64)freq;
	if (ticks == 0) {
		ticks = 1;
	}
	g_config_next_tick = timer_get_ticks() + ticks;
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
		g_config_dirty = 0;
		config_read_autoupdate();
		config_schedule_next();
		return;
	}
	g_config_doc = toml_parse(data, len);
	g_config_dirty = 0;
	config_read_autoupdate();
	config_schedule_next();
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
		g_config_dirty = 0;
		config_read_autoupdate();
		config_schedule_next();
		return;
	}
	g_config_doc = toml_parse_file(path);
	if (!g_config_doc) {
		g_config_doc = toml_new();
	}
	g_config_dirty = 0;
	config_read_autoupdate();
	config_schedule_next();
}

int
config_attach_file(const char *path)
{
	int	ret;

	if (!path || !vfs_is_initialized()) {
		return (-1);
	}

	config_copy_path(path);
	g_config_fs_ready = 1;

	if (config_file_exists(g_config_path)) {
		ret = config_reload_from_file();
	} else {
		ret = config_flush_to_file();
	}

	config_read_autoupdate();
	config_schedule_next();
	return (ret);
}

int
config_reload_from_file(void)
{
	toml_doc_t	*doc, *old;

	if (!g_config_fs_ready || g_config_path[0] == '\0' ||
	    !vfs_is_initialized()) {
		return (-1);
	}

	doc = toml_parse_file(g_config_path);
	if (!doc) {
		return (-1);
	}

	old = g_config_doc;
	g_config_doc = doc;
	if (old) {
		toml_free(old);
	}
	g_config_dirty = 0;
	config_read_autoupdate();
	config_schedule_next();
	return (0);
}

int
config_flush_to_file(void)
{
	int	ret;

	if (!g_config_doc || !g_config_fs_ready ||
	    g_config_path[0] == '\0' || !vfs_is_initialized()) {
		return (-1);
	}

	ret = toml_save(g_config_doc, g_config_path);
	if (ret == 0) {
		g_config_dirty = 0;
	}
	return (ret);
}

void
config_poll(void)
{
	u64	now;

	if (!g_config_fs_ready || g_config_syncing) {
		return;
	}
	config_read_autoupdate();
	if (!g_config_autoupdate_enabled ||
	    g_config_autoupdate_seconds == 0) {
		g_config_next_tick = 0;
		return;
	}
	if (!timer_is_initialized()) {
		return;
	}
	if (g_config_next_tick == 0) {
		config_schedule_next();
		return;
	}

	now = timer_get_ticks();
	if (now < g_config_next_tick) {
		return;
	}

	g_config_syncing = 1;
	if (g_config_dirty) {
		if (g_config_autosave) {
			config_flush_to_file();
		}
	} else {
		config_reload_from_file();
	}
	g_config_syncing = 0;

	config_read_autoupdate();
	config_schedule_next();
}

int
config_save_to_file(const char *path)
{
	int	ret;

	if (!g_config_doc || !path) {
		return (-1);
	}
	ret = toml_save(g_config_doc, path);
	if (ret == 0 && g_config_path[0] != '\0' &&
	    strcmp(path, g_config_path) == 0) {
		g_config_dirty = 0;
	}
	return (ret);
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
	g_config_dirty = 1;
	config_read_autoupdate();
	config_schedule_next();
	if (g_config_fs_ready && g_config_autosave &&
	    !g_config_syncing) {
		config_flush_to_file();
	}
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

void
config_foreach_in_section(const char *section, config_kv_cb cb,
    void *ctx)
{
	toml_entry_t	*e;

	if (!g_config_doc || !section || !cb) {
		return;
	}
	for (e = g_config_doc->entries; e; e = e->next) {
		if (!e->section || !e->key || !e->value) {
			continue;
		}
		if (strcmp(e->section, section) != 0) {
			continue;
		}
		cb(e->key, e->value, ctx);
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
