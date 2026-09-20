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
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type inst_module_t as one module the live root was populated from
$define %type inst_plan_t as the module map and the copy set derived from it
$define %type inst_cand_t as one candidate install target
$define %type inst_slice_t as one placed partition and its published slice
$define %type inst_target_t as the chosen disk after partitioning
$define %type inst_progress_fn as copy progress callback
$define %type inst_ctx_t as one installer run

$define %func inst_fail as procedure with args inst_ctx_t *, const char *, ...
$define %func inst_error as function with args const inst_ctx_t *
$define %func inst_error_clear as procedure with args inst_ctx_t *
$define %func inst_module_by_role as function with args const inst_plan_t *, const char *
$define %func inst_size_label as procedure with args char *, size_t, uint64_t, uint32_t
$define %func inst_sectors as function with args uint64_t, uint32_t
$define %func inst_bytes as function with args uint64_t, uint32_t

*/

/* !SPACE!

$space %export inst_module_t, inst_plan_t, inst_cand_t, inst_slice_t
$space %export inst_target_t, inst_progress_fn, inst_ctx_t
$space %export inst_fail, inst_error, inst_error_clear
$space %export inst_module_by_role, inst_size_label
$space %export inst_sectors, inst_bytes

*/

#ifndef INSTALL_INST_H
#define INSTALL_INST_H

#include <disk.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>

#include "plan.h"

#define INST_PATH_MAX		256
#define INST_NAME_MAX		64
#define INST_ROLE_MAX		32
#define INST_LABEL_MAX		96
#define INST_ERR_MAX		192
#define INST_CANDS_MAX		16


typedef struct inst_module {
	char	name[INST_NAME_MAX];
	char	dest[INST_PATH_MAX];
	char	role[INST_ROLE_MAX];
} inst_module_t;


typedef struct inst_plan {
	inst_module_t	modules[INST_MODULES_MAX];
	char		trees[INST_TREES_MAX][INST_PATH_MAX];
	int		module_count;
	int		tree_count;
} inst_plan_t;


typedef struct inst_cand {
	ldisk_entry_t	entry;
	char		label[INST_LABEL_MAX];
	char		reason[INST_LABEL_MAX];
	int		usable;
} inst_cand_t;

typedef struct inst_slice {
	ldisk_part_t	part;
	ldisk_entry_t	slice;
	int		present;
} inst_slice_t;


typedef struct inst_target {
	ldisk_entry_t	disk;
	inst_slice_t	bios;
	inst_slice_t	esp;
	inst_slice_t	root;
	int		partitioned;
} inst_target_t;


typedef void	(*inst_progress_fn)(const char *label, uint64_t done,
		    uint64_t total, void *ctx);


typedef struct inst_ctx {
	inst_plan_t	plan;
	inst_target_t	target;
	inst_cand_t	cands[INST_CANDS_MAX];
	char		err[INST_ERR_MAX];
	int		cand_count;
	int		selected;
	int		want_uefi;
	int		want_bios;

	uint64_t	esp_bytes;
	uint32_t	root_max_files;
	int		copied;
	int		boot_done;
} inst_ctx_t;

void		inst_fail(inst_ctx_t *ctx, const char *fmt, ...);
const char	*inst_error(const inst_ctx_t *ctx);
void		inst_error_clear(inst_ctx_t *ctx);


const inst_module_t	*inst_module_by_role(const inst_plan_t *plan,
			    const char *role);

void		inst_size_label(char *out, size_t size, uint64_t sectors,
		    uint32_t sector_size);


uint64_t	inst_sectors(uint64_t bytes, uint32_t sector_size);
uint64_t	inst_bytes(uint64_t sectors, uint32_t sector_size);

#endif
