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
$define %type uint32_t as 32 bit unsigned
$define %type inst_level_t as one open directory of the walk
$define %type inst_ctx_t as installer run state

$define %func inst_path_join as function with args inst_ctx_t *, char *, size_t, const char *, const char *
$define %func inst_level_fill as function with args inst_ctx_t *, inst_level_t *
$define %func inst_walk_push as function with args inst_ctx_t *, inst_level_t **, int *, int *, const char *
$define %func inst_walk as function with args inst_ctx_t *, const char *, inst_walk_fn, void *

*/

/* !SPACE!

$space %internal inst_level_fill, inst_walk_push
$space %export inst_path_join, inst_walk

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inst.h"
#include "walk.h"

#define INST_WALK_PAGE		64
#define INST_WALK_GROW		16

typedef struct inst_level {
	struct api_dirent	page[INST_WALK_PAGE];
	char			path[INST_PATH_MAX];
	uint32_t		offset;
	int			count;
	int			next;
} inst_level_t;

int
inst_path_join(inst_ctx_t *ctx, char *out, size_t size, const char *dir,
    const char *name)
{
	int	len;

	if (dir[0] == '/' && dir[1] == '\0') {
		len = snprintf(out, size, "/%s", name);
	} else {
		len = snprintf(out, size, "%s/%s", dir, name);
	}
	if (len < 0 || (size_t)len + 1 > size) {
		inst_fail(ctx, "path too long: %s/%s", dir, name);
		return (-1);
	}
	return (0);
}


static int
inst_level_fill(inst_ctx_t *ctx, inst_level_t *lvl)
{
	int	got;

	got = fsListdirAt(lvl->path, lvl->offset, lvl->page, INST_WALK_PAGE);
	if (got < 0) {
		inst_fail(ctx, "cannot read %s: %s", lvl->path,
		    strerror(errno));
		return (-1);
	}

	lvl->offset += (uint32_t)got;
	lvl->count = got;
	lvl->next = 0;
	return (got);
}


static int
inst_walk_push(inst_ctx_t *ctx, inst_level_t **stack, int *cap, int *depth,
    const char *path)
{
	inst_level_t	*grown;
	inst_level_t	*lvl;

	if (*depth + 1 > *cap) {
		grown = realloc(*stack, (size_t)(*cap + INST_WALK_GROW) *
		    sizeof(**stack));
		if (grown == NULL) {
			inst_fail(ctx, "out of memory walking %s -- the "
			    "directory tree is deeper than the heap allows",
			    path);
			return (-1);
		}
		*stack = grown;
		*cap += INST_WALK_GROW;
	}

	lvl = &(*stack)[*depth];
	memset(lvl, 0, sizeof(*lvl));
	if (strlen(path) + 1 > sizeof(lvl->path)) {
		inst_fail(ctx, "path too long: %s", path);
		return (-1);
	}
	memcpy(lvl->path, path, strlen(path) + 1);
	if (inst_level_fill(ctx, lvl) < 0) {
		return (-1);
	}
	(*depth)++;
	return (0);
}

int
inst_walk(inst_ctx_t *ctx, const char *root, inst_walk_fn fn, void *arg)
{
	struct api_fs_stat	st;
	inst_level_t		*stack;
	inst_level_t		*lvl;
	const char		*name;
	char			path[INST_PATH_MAX];
	int			cap;
	int			depth;
	int			ret;

	if (ctx == NULL || root == NULL || fn == NULL) {
		return (-1);
	}

	memset(&st, 0, sizeof(st));
	if (fsStat(root, &st) != 0) {
		inst_fail(ctx, "cannot stat %s: %s", root, strerror(errno));
		return (-1);
	}
	ret = fn(ctx, root, &st, arg);
	if (ret != 0) {
		return (ret);
	}
	if (st.type != API_FS_TYPE_DIR) {
		return (0);
	}

	stack = NULL;
	cap = 0;
	depth = 0;
	if (inst_walk_push(ctx, &stack, &cap, &depth, root) != 0) {
		free(stack);
		return (-1);
	}

	while (depth > 0) {
		lvl = &stack[depth - 1];

		if (lvl->next >= lvl->count) {

			if (lvl->count == INST_WALK_PAGE) {
				if (inst_level_fill(ctx, lvl) < 0) {
					free(stack);
					return (-1);
				}
				if (lvl->count > 0) {
					continue;
				}
			}
			depth--;
			continue;
		}

		name = lvl->page[lvl->next].name;
		lvl->next++;
		if (name[0] == '\0' || strcmp(name, ".") == 0 ||
		    strcmp(name, "..") == 0) {
			continue;
		}
		if (inst_path_join(ctx, path, sizeof(path), lvl->path,
		    name) != 0) {
			free(stack);
			return (-1);
		}

		memset(&st, 0, sizeof(st));
		if (fsStat(path, &st) != 0) {
			inst_fail(ctx, "cannot stat %s: %s", path,
			    strerror(errno));
			free(stack);
			return (-1);
		}
		ret = fn(ctx, path, &st, arg);
		if (ret != 0) {
			free(stack);
			return (ret);
		}
		if (st.type != API_FS_TYPE_DIR) {
			continue;
		}


		if (inst_walk_push(ctx, &stack, &cap, &depth, path) != 0) {
			free(stack);
			return (-1);
		}
	}

	free(stack);
	return (0);
}
