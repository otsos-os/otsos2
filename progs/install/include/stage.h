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
$define %type uint64_t as 64 bit unsigned
$define %type inst_ctx_t as installer run state shared by every stage

$define %func inst_plan_load as function with args inst_ctx_t *
$define %func inst_scan as function with args inst_ctx_t *
$define %func inst_partition as function with args inst_ctx_t *
$define %func inst_format as function with args inst_ctx_t *
$define %func inst_copy as function with args inst_ctx_t *, inst_progress_fn, void *
$define %func inst_copy_total as function with args inst_ctx_t *, uint64_t *
$define %func inst_boot as function with args inst_ctx_t *
$define %func inst_verify as function with args inst_ctx_t *

*/

/* !SPACE!

$space %export inst_plan_load, inst_scan
$space %export inst_partition, inst_format
$space %export inst_copy, inst_copy_total, inst_boot, inst_verify

*/

#ifndef INSTALL_STAGE_H
#define INSTALL_STAGE_H

#include "inst.h"
int	inst_plan_load(inst_ctx_t *ctx);
int	inst_scan(inst_ctx_t *ctx);
int	inst_partition(inst_ctx_t *ctx);
int	inst_format(inst_ctx_t *ctx);
int	inst_copy(inst_ctx_t *ctx, inst_progress_fn progress, void *pctx);
int	inst_copy_total(inst_ctx_t *ctx, uint64_t *total);
int	inst_boot(inst_ctx_t *ctx);
int	inst_verify(inst_ctx_t *ctx);

#endif
