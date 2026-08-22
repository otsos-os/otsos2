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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s32 as 32 bit signed
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type cm_key_cb as function pointer with args const char *, void *
$define %type cm_consumer_update_t as function pointer with args u32
$define %type cm_entry as registry key or value enumeration record
$define %type cm_hive as registry hive enumeration record with access mask

$define %func cm_init as function with args void
$define %func cm_is_initialized as function with args void
$define %func cm_mount_path as function with args void
$define %func cm_foreach_key as function with args hive, key, cb, ctx
$define %func cm_key_exists as function with args hive, key
$define %func cm_value_info as function with args hive, key, value, type
$define %func cm_enum_entry as function with args hive, key, index, entry
$define %func cm_enum_hive as function with args index, hive, kusr
$define %func cm_check_access as function with args hive, key, value, op
$define %func cm_register_consumer as function with args id, name, update
$define %func cm_update_consumer as function with args id, flags
$define %func cm_update_consumer_user as function with args id, flags, kusr
$define %func cm_read_value as function with args hive, key, value, buf
$define %func cm_get_bool as function with args hive, key, value, out
$define %func cm_get_i32 as function with args hive, key, value, out
$define %func cm_get_u32 as function with args hive, key, value, out
$define %func cm_get_u64 as function with args hive, key, value, out
$define %func cm_get_ipv4 as function with args hive, key, value, out
$define %func cm_get_string as function with args hive, key, value, out
$define %func cm_create_key as function with args hive, key
$define %func cm_delete_key as function with args hive, key
$define %func cm_set_value as function with args hive, key, value, type
$define %func cm_delete_value as function with args hive, key, value
$define %func cm_set_bool as function with args hive, key, value, val
$define %func cm_set_i32 as function with args hive, key, value, val
$define %func cm_set_u32 as function with args hive, key, value, val
$define %func cm_set_u64 as function with args hive, key, value, val
$define %func cm_set_ipv4 as function with args hive, key, value, val
$define %func cm_set_string as function with args hive, key, value, val
$define %func cm_get_bool_default as function with args hive, key, value, default
$define %func cm_get_i32_default as function with args hive, key, value, default
$define %func cm_get_u32_default as function with args hive, key, value, default
$define %func cm_get_ipv4_default as function with args hive, key, value, default
$define %func cm_get_string_default as function with args hive, key, value, out

*/

/* !SPACE!

$space %export cm_init, cm_is_initialized, cm_mount_path
$space %export cm_foreach_key, cm_key_exists, cm_value_info
$space %export cm_enum_entry, cm_enum_hive, cm_check_access
$space %export cm_register_consumer, cm_update_consumer
$space %export cm_update_consumer_user
$space %export cm_read_value, cm_get_bool, cm_get_i32
$space %export cm_get_u32, cm_get_u64, cm_get_ipv4, cm_get_string
$space %export cm_create_key, cm_delete_key, cm_set_value
$space %export cm_delete_value, cm_set_bool, cm_set_i32
$space %export cm_set_u32, cm_set_u64, cm_set_ipv4, cm_set_string
$space %export cm_get_bool_default, cm_get_i32_default
$space %export cm_get_u32_default, cm_get_ipv4_default
$space %export cm_get_string_default

*/

#ifndef KERNEL_CM_CM_H
#define KERNEL_CM_CM_H

#include <mlibc/mlibc.h>

#define	CM_MOUNT_PATH	"/conf/registry"

#define	CM_TYPE_STRING		1
#define	CM_TYPE_BOOL		2
#define	CM_TYPE_I32		3
#define	CM_TYPE_U32		4
#define	CM_TYPE_U64		5
#define	CM_TYPE_IPV4		6
#define	CM_TYPE_BYTES		7
#define	CM_TYPE_MULTI_STRING	8

#define	CM_ENTRY_KEY		1
#define	CM_ENTRY_VALUE		2

#define	CM_ACCESS_READ		1
#define	CM_ACCESS_ADD		2
#define	CM_ACCESS_EDIT		3
#define	CM_HIVE_CAN_READ	0x1
#define	CM_HIVE_CAN_ADD		0x2
#define	CM_HIVE_CAN_EDIT	0x4

#define	CM_SUBJECT_USER		0
#define	CM_SUBJECT_KUSR		1

#define	CM_CONSUMER_NET		1
#define	CM_CONSUMER_SCHEDULER	2
#define	CM_CONSUMER_KUSR	3
#define	CM_CONSUMER_CONSOLE	4
#define	CM_CONSUMER_INPUT	5

typedef int (*cm_key_cb)(const char *name, void *ctx);
typedef int (*cm_consumer_update_t)(u32 flags);

typedef struct cm_entry {
	u32	kind;
	u32	type;
	u32	size;
	char	name[32];
} cm_entry_t;

typedef struct cm_hive {
	u32	access;
	char	name[32];
} cm_hive_t;

int		cm_init(void);
int		cm_is_initialized(void);
const char	*cm_mount_path(void);
int		cm_foreach_key(const char *hive, const char *key,
		    cm_key_cb cb, void *ctx);
int		cm_key_exists(const char *hive, const char *key);
int		cm_value_info(const char *hive, const char *key,
		    const char *value, u32 *type, u32 *size);
int		cm_enum_entry(const char *hive, const char *key,
		    u32 index, cm_entry_t *entry);
int		cm_enum_hive(u32 index, cm_hive_t *hive, int is_kusr);
int		cm_check_access(const char *hive, const char *key,
		    const char *value, u32 op, int is_kusr);
int		cm_register_consumer(u32 id, const char *name,
		    cm_consumer_update_t update);
int		cm_update_consumer(u32 id, u32 flags);
int		cm_update_consumer_user(u32 id, u32 flags, int is_kusr);
int		cm_read_value(const char *hive, const char *key,
		    const char *value, void *buf, u32 bufsize,
		    u32 *bytes_read);
int		cm_get_bool(const char *hive, const char *key,
		    const char *value, int *out);
int		cm_get_i32(const char *hive, const char *key,
		    const char *value, s32 *out);
int		cm_get_u32(const char *hive, const char *key,
		    const char *value, u32 *out);
int		cm_get_u64(const char *hive, const char *key,
		    const char *value, u64 *out);
int		cm_get_ipv4(const char *hive, const char *key,
		    const char *value, u32 *out);
int		cm_get_string(const char *hive, const char *key,
		    const char *value, char *out, u32 out_size);
int		cm_create_key(const char *hive, const char *key);
int		cm_delete_key(const char *hive, const char *key);
int		cm_set_value(const char *hive, const char *key,
		    const char *value, u32 type, const void *data,
		    u32 size);
int		cm_delete_value(const char *hive, const char *key,
		    const char *value);
int		cm_set_bool(const char *hive, const char *key,
		    const char *value, int val);
int		cm_set_i32(const char *hive, const char *key,
		    const char *value, s32 val);
int		cm_set_u32(const char *hive, const char *key,
		    const char *value, u32 val);
int		cm_set_u64(const char *hive, const char *key,
		    const char *value, u64 val);
int		cm_set_ipv4(const char *hive, const char *key,
		    const char *value, u32 val);
int		cm_set_string(const char *hive, const char *key,
		    const char *value, const char *val);
int		cm_get_bool_default(const char *hive, const char *key,
		    const char *value, int default_val);
s32		cm_get_i32_default(const char *hive, const char *key,
		    const char *value, s32 default_val);
u32		cm_get_u32_default(const char *hive, const char *key,
		    const char *value, u32 default_val);
u32		cm_get_ipv4_default(const char *hive, const char *key,
		    const char *value, u32 default_val);
int		cm_get_string_default(const char *hive, const char *key,
		    const char *value, char *out, u32 out_size,
		    const char *default_val);

#endif
