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

$define %type u32 as 32 bit unsigned
$define %type vfs_back_ops_t as backend operation table

$define %func hivefs_reset as procedure with args void
$define %func hivefs_load_pack as function with args const void *, u32
$define %func hivefs_is_loaded as function with args void
$define %func hivefs_set_store_path as function with args const char *
$define %func hivefs_load_store as function with args const char *
$define %func hivefs_sync as function with args void
$define %func hivefs_create_key as function with args hive, key
$define %func hivefs_delete_key as function with args hive, key
$define %func hivefs_set_value as function with args hive, key, value, type
$define %func hivefs_delete_value as function with args hive, key, value
$define %func hivefs_value_info as function with args hive, key, value, type
$define %func hivefs_access_info as function with args hive, key, value, flags
$define %func hivefs_back_ops as function with args void

*/

/* !SPACE!

$space %export hivefs_reset, hivefs_load_pack
$space %export hivefs_is_loaded, hivefs_set_store_path
$space %export hivefs_load_store, hivefs_sync
$space %export hivefs_create_key, hivefs_delete_key
$space %export hivefs_set_value, hivefs_delete_value
$space %export hivefs_value_info, hivefs_access_info
$space %export hivefs_back_ops

*/

#ifndef KERNEL_DRIVERS_FS_HIVEFS_HIVEFS_H
#define KERNEL_DRIVERS_FS_HIVEFS_HIVEFS_H

#include <kernel/drivers/fs/vfs/back/vfs_back.h>

#define	HIVEFS_TYPE_STRING		1
#define	HIVEFS_TYPE_BOOL		2
#define	HIVEFS_TYPE_I32			3
#define	HIVEFS_TYPE_U32			4
#define	HIVEFS_TYPE_U64			5
#define	HIVEFS_TYPE_IPV4		6
#define	HIVEFS_TYPE_BYTES		7
#define	HIVEFS_TYPE_MULTI_STRING	8

#define	HIVEFS_ACCESS_INHERIT	0
#define	HIVEFS_ACCESS_USER	1
#define	HIVEFS_ACCESS_KUSR	2
#define	HIVEFS_ACCESS_MASK	0x3
#define	HIVEFS_ACCESS_READ_SHIFT	16
#define	HIVEFS_ACCESS_ADD_SHIFT	18
#define	HIVEFS_ACCESS_EDIT_SHIFT	20
#define	HIVEFS_ACCESS_DEFAULT					\
	((HIVEFS_ACCESS_USER << HIVEFS_ACCESS_READ_SHIFT) |	\
	(HIVEFS_ACCESS_KUSR << HIVEFS_ACCESS_ADD_SHIFT) |	\
	(HIVEFS_ACCESS_KUSR << HIVEFS_ACCESS_EDIT_SHIFT))

void				hivefs_reset(void);
int				hivefs_load_pack(const void *data, u32 size);
int				hivefs_is_loaded(void);
int				hivefs_set_store_path(const char *path);
int				hivefs_load_store(const char *path);
int				hivefs_sync(void);
int				hivefs_create_key(const char *hive,
				    const char *key);
int				hivefs_delete_key(const char *hive,
				    const char *key);
int				hivefs_set_value(const char *hive,
				    const char *key, const char *value,
				    u32 type, const void *data, u32 size);
int				hivefs_delete_value(const char *hive,
				    const char *key, const char *value);
int				hivefs_value_info(const char *hive,
				    const char *key, const char *value,
				    u32 *type, u32 *size);
int				hivefs_access_info(const char *hive,
				    const char *key, const char *value,
				    u32 *flags);
const vfs_back_ops_t		*hivefs_back_ops(void);

#endif
