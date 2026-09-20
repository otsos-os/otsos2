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

$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type ldisk_dev_t as an opened block device

$define %func ldisk_dev_ok as function with args const ldisk_dev_t *
$define %func ldisk_copy_info as procedure with args ldisk_info_t *, const dioc_info_t *
$define %func ldisk_str_copy as function with args char *, size_t, const char *
$define %func ldisk_fs_path_ok as function with args const char *

*/

/* !SPACE!

$space %internal ldisk_dev_ok, ldisk_copy_info, ldisk_str_copy
$space %internal ldisk_fs_path_ok

*/

#ifndef LIBDISK_DISK_INT_H
#define LIBDISK_DISK_INT_H

#include <disk.h>
#include <stddef.h>

#define LDISK_IFACE_NAME	DIOC_IFACE_NAME
#define LDISK_NS_ROOT		"/Entity/Interface/Driver"

int	ldisk_dev_ok(const ldisk_dev_t *dev);
void	ldisk_copy_info(ldisk_info_t *out, const dioc_info_t *in);
int	ldisk_str_copy(char *dst, size_t size, const char *src);
int	ldisk_fs_path_ok(const char *path);

#endif
