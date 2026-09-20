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
$define %type inst_ctx_t as installer run state

$define %func inst_check_role as function with args inst_ctx_t *, ldisk_dev_t *, const char *, void *, uint64_t
$define %func inst_verify as function with args inst_ctx_t *

*/

/* !SPACE!

$space %internal inst_check_role
$space %export inst_verify

*/

#include <disk.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"

#define INST_VERIFY_PROBE	512U

static int
inst_check_role(inst_ctx_t *ctx, ldisk_dev_t *dev, const char *role,
    void *probe, uint64_t probe_size)
{
	const inst_module_t	*mod;
	uint64_t		got;

	mod = inst_module_by_role(&ctx->plan, role);
	if (mod == NULL) {
		inst_fail(ctx, "no module carries the %s role", role);
		return (-1);
	}

	got = 0;
	if (ldisk_read(dev, mod->dest, probe, probe_size, &got) != 0) {
		inst_fail(ctx, "the installed system has no readable %s at %s: "
		    "%s", role, mod->dest, strerror(errno));
		return (-1);
	}
	if (got == 0) {

		inst_fail(ctx, "the installed %s at %s is empty", role,
		    mod->dest);
		return (-1);
	}
	return (0);
}

int
inst_verify(inst_ctx_t *ctx)
{
	ldisk_dev_t	dev;
	void		*probe;
	int		ret;

	if (ctx == NULL) {
		return (-1);
	}
	if (ctx->copied == 0) {
		inst_fail(ctx, "nothing has been copied to the target yet");
		return (-1);
	}
	if (ctx->target.root.present == 0) {
		inst_fail(ctx, "the target has no root partition");
		return (-1);
	}

	probe = malloc(INST_VERIFY_PROBE);
	if (probe == NULL) {
		inst_fail(ctx, "out of memory verifying the install");
		return (-1);
	}
	if (ldisk_open(ctx->target.root.slice.path, &dev) != 0) {
		inst_fail(ctx, "cannot open the root partition: %s",
		    strerror(errno));
		free(probe);
		return (-1);
	}


	ret = inst_check_role(ctx, &dev, INST_ROLE_KERNEL, probe,
	    INST_VERIFY_PROBE);
	if (ret == 0) {
		ret = inst_check_role(ctx, &dev, INST_ROLE_CMSEED, probe,
		    INST_VERIFY_PROBE);
	}

	ldisk_close(&dev);
	free(probe);
	return (ret);
}
