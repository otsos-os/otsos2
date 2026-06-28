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
$define %type u64 as 64 bit unsigned
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type devfs_device_t as struct with name, type, read_fn, write_fn, stat_fn
$define %type vnode_t as struct with type, refcount, size, mode, name, ops, data

$define %func devfs_init as procedure with args void
$define %func devfs_lookup as function with args const char *
$define %func devfs_root_readdir as function with args vnode_t *, u32, char *, int *

*/

/* !SPACE!

$space %export devfs_init, devfs_lookup, devfs_root_readdir

*/

#ifndef DEVFS_H
#define DEVFS_H

#include <kernel/drivers/fs/vfs/vfs.h>

#define DEVFS_DEV_NULL	1
#define DEVFS_DEV_ZERO	2
#define DEVFS_DEV_TTY	3
#define DEVFS_DEV_CONSOLE 4
#define DEVFS_DEV_FB0	5
#define DEVFS_DEV_RANDOM 6
#define DEVFS_DEV_URANDOM 7

void		devfs_init(void);
vnode_t		*devfs_lookup(const char *path);
int		devfs_root_readdir(vnode_t *vn, u32 index, char *name,
		    int *type);

#endif
