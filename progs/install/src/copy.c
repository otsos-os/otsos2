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
$define %type uint64_t as 64 bit unsigned
$define %type inst_walk_fn as callback invoked once per source object
$define %type inst_ctx_t as installer run state

$define %func inst_measure_cb as function with args inst_ctx_t *, const char *, const struct api_fs_stat *, void *
$define %func inst_copy_cb as function with args inst_ctx_t *, const char *, const struct api_fs_stat *, void *
$define %func inst_copy_file as function with args inst_ctx_t *, struct inst_copy_state *, const char *, uint64_t
$define %func inst_copy_total as function with args inst_ctx_t *, uint64_t *
$define %func inst_copy as function with args inst_ctx_t *, inst_progress_fn, void *

*/

/* !SPACE!

$space %internal inst_measure_cb, inst_copy_cb, inst_copy_file
$space %export inst_copy_total, inst_copy

*/

#include <disk.h>
#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"
#include "walk.h"

struct inst_copy_state {
	ldisk_dev_t		*dev;
	inst_progress_fn	progress;
	void			*pctx;
	unsigned char		*buf;
	uint64_t		chunk;
	uint64_t		done;
	uint64_t		total;
};

static int
inst_measure_cb(inst_ctx_t *ctx, const char *path,
    const struct api_fs_stat *st, void *arg)
{
	uint64_t	*total;

	(void)ctx;
	(void)path;
	total = arg;
	if (st->type == API_FS_TYPE_REG) {
		*total += st->size;
	}
	return (0);
}


static int
inst_copy_file(inst_ctx_t *ctx, struct inst_copy_state *cs, const char *path,
    uint64_t size)
{
	uint64_t	offset;
	ssize_t		got;
	size_t		want;
	int		fd;

	fd = dataOpen(path, API_OPEN_READ);
	if (fd < 0) {
		inst_fail(ctx, "cannot open %s: %s", path, strerror(errno));
		return (-1);
	}

	offset = 0;
	while (offset < size) {
		want = (size_t)cs->chunk;
		if (size - offset < cs->chunk) {
			want = (size_t)(size - offset);
		}
		got = dataRead(fd, cs->buf, want);
		if (got < 0) {
			inst_fail(ctx, "cannot read %s: %s", path,
			    strerror(errno));
			(void)dataClose(fd);
			return (-1);
		}
		if (got == 0) {

			inst_fail(ctx, "%s ended early at %llu of %llu bytes",
			    path, (unsigned long long)offset,
			    (unsigned long long)size);
			(void)dataClose(fd);
			return (-1);
		}
		if (ldisk_write(cs->dev, LDISK_FS_CHAINFS, path, cs->buf,
		    (uint64_t)got, offset, size) != 0) {
			inst_fail(ctx, "cannot write %s to the target: %s",
			    path, strerror(errno));
			(void)dataClose(fd);
			return (-1);
		}
		offset += (uint64_t)got;
		cs->done += (uint64_t)got;
		if (cs->progress != NULL) {
			cs->progress(path, cs->done, cs->total, cs->pctx);
		}
	}
	(void)dataClose(fd);


	if (size == 0 && ldisk_write(cs->dev, LDISK_FS_CHAINFS, path, cs->buf,
	    0, 0, 0) != 0) {
		inst_fail(ctx, "cannot create %s on the target: %s", path,
		    strerror(errno));
		return (-1);
	}
	return (0);
}

static int
inst_copy_cb(inst_ctx_t *ctx, const char *path, const struct api_fs_stat *st,
    void *arg)
{
	struct inst_copy_state	*cs;

	cs = arg;
	if (st->type == API_FS_TYPE_DIR) {
		if (ldisk_mkdir(cs->dev, LDISK_FS_CHAINFS, path) != 0) {
			inst_fail(ctx, "cannot create directory %s on the "
			    "target: %s", path, strerror(errno));
			return (-1);
		}
		if (cs->progress != NULL) {
			cs->progress(path, cs->done, cs->total, cs->pctx);
		}
		return (0);
	}
	if (st->type != API_FS_TYPE_REG) {

		return (0);
	}
	return (inst_copy_file(ctx, cs, path, st->size));
}

int
inst_copy_total(inst_ctx_t *ctx, uint64_t *out)
{
	uint64_t	total;
	int		i;

	if (ctx == NULL || out == NULL) {
		return (-1);
	}
	*out = 0;
	total = 0;
	for (i = 0; i < ctx->plan.tree_count; i++) {
		if (inst_walk(ctx, ctx->plan.trees[i], inst_measure_cb,
		    &total) != 0) {
			return (-1);
		}
	}
	*out = total;
	return (0);
}

int
inst_copy(inst_ctx_t *ctx, inst_progress_fn progress, void *pctx)
{
	struct inst_copy_state	cs;
	ldisk_dev_t		dev;
	int			i;

	if (ctx == NULL) {
		return (-1);
	}
	if (ctx->target.partitioned == 0 || ctx->target.root.present == 0) {
		inst_fail(ctx, "target root partition is not ready");
		return (-1);
	}
	if (ctx->plan.tree_count == 0) {
		inst_fail(ctx, "nothing to install: the module map produced no "
		    "directories");
		return (-1);
	}

	memset(&cs, 0, sizeof(cs));
	if (inst_copy_total(ctx, &cs.total) != 0) {
		return (-1);
	}


	cs.chunk = ldisk_chunk_max();
	if (cs.chunk > INST_CHUNK_CAP) {
		cs.chunk = INST_CHUNK_CAP;
	}
	if (cs.chunk == 0) {
		inst_fail(ctx, "the kernel reports a zero write chunk");
		return (-1);
	}
	cs.buf = malloc((size_t)cs.chunk);
	if (cs.buf == NULL) {
		inst_fail(ctx, "out of memory allocating a %llu byte copy "
		    "buffer", (unsigned long long)cs.chunk);
		return (-1);
	}

	if (ldisk_open(ctx->target.root.slice.path, &dev) != 0) {
		inst_fail(ctx, "cannot open the root partition: %s",
		    strerror(errno));
		free(cs.buf);
		return (-1);
	}
	cs.dev = &dev;
	cs.progress = progress;
	cs.pctx = pctx;

	for (i = 0; i < ctx->plan.tree_count; i++) {
		if (inst_walk(ctx, ctx->plan.trees[i], inst_copy_cb,
		    &cs) != 0) {
			ldisk_close(&dev);
			free(cs.buf);
			return (-1);
		}
	}
	if (ldisk_flush(&dev) != 0) {
		inst_fail(ctx, "cannot flush the target: %s", strerror(errno));
		ldisk_close(&dev);
		free(cs.buf);
		return (-1);
	}
	ldisk_close(&dev);
	free(cs.buf);

	ctx->copied = 1;
	return (0);
}
