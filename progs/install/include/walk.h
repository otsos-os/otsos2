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
$define %type size_t as native object size
$define %type inst_walk_fn as callback invoked once per visited object
$define %type inst_ctx_t as installer run state

$define %func inst_walk as function with args inst_ctx_t *, const char *, inst_walk_fn, void *
$define %func inst_path_join as function with args inst_ctx_t *, char *, size_t, const char *, const char *

*/

/* !SPACE!

$space %export inst_walk_fn, inst_walk, inst_path_join

*/

#ifndef INSTALL_WALK_H
#define INSTALL_WALK_H

#include <native.h>

#include "inst.h"

typedef int	(*inst_walk_fn)(inst_ctx_t *ctx, const char *path,
		    const struct api_fs_stat *st, void *arg);
int	inst_walk(inst_ctx_t *ctx, const char *root, inst_walk_fn fn,
	    void *arg);

int	inst_path_join(inst_ctx_t *ctx, char *out, size_t size,
	    const char *dir, const char *name);

#endif
