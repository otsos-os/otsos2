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
$define %type config_section_cb as function pointer with args const char *, void *
$define %type config_kv_cb as function pointer with args const char *, const char *, void *

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

$space %export config_init_from_data, config_init_from_file
$space %export config_attach_file, config_reload_from_file
$space %export config_flush_to_file, config_poll
$space %export config_save_to_file, config_get, config_get_bool
$space %export config_get_int, config_get_string, config_set
$space %export config_foreach_section, config_foreach_in_section
$space %export config_is_initialized
$space %export config_free

*/

#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

#include <mlibc/mlibc.h>

#define CONFIG_PATH_BOOT "/conf/boot/config.toml"

typedef int (*config_section_cb)(const char *section, void *ctx);
typedef void (*config_kv_cb)(const char *key, const char *value, void *ctx);

void		config_init_from_data(const char *data, u32 len);
void		config_init_from_file(const char *path);
int		config_attach_file(const char *path);
int		config_reload_from_file(void);
int		config_flush_to_file(void);
void		config_poll(void);
int		config_save_to_file(const char *path);
const char	*config_get(const char *section, const char *key);
int		config_get_bool(const char *section, const char *key,
		    int default_val);
int		config_get_int(const char *section, const char *key,
		    int default_val);
const char	*config_get_string(const char *section, const char *key,
		    const char *default_val);
void		config_set(const char *section, const char *key,
		    const char *value);
void		config_foreach_section(const char *prefix,
	    config_section_cb cb, void *ctx);
void		config_foreach_in_section(const char *section,
	    config_kv_cb cb, void *ctx);
int		config_is_initialized(void);
void		config_free(void);

#endif
