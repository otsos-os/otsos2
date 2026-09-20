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
$define %type inst_module_t as one module of the module map
$define %type inst_ctx_t as one installer run

$define %func inst_top_level as function with args const char *, char *, size_t
$define %func inst_tree_add as function with args inst_ctx_t *, const char *
$define %func inst_module_read as function with args const char *, inst_module_t *
$define %func inst_plan_load as function with args inst_ctx_t *

*/

/* !SPACE!

$space %internal inst_top_level, inst_tree_add, inst_module_read
$space %export inst_plan_load

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <string.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"


static int
inst_top_level(const char *path, char *out, size_t size)
{
	const char	*slash;
	size_t		len;

	if (path == NULL || path[0] != '/' || path[1] == '\0') {
		return (-1);
	}
	slash = strchr(path + 1, '/');
	if (slash != NULL) {
		len = (size_t)(slash - path);
	} else {

		return (1);
	}
	if (len + 1 > size) {
		return (-1);
	}
	memcpy(out, path, len);
	out[len] = '\0';
	return (0);
}


static int
inst_tree_add(inst_ctx_t *ctx, const char *tree)
{
	int	i;

	for (i = 0; i < ctx->plan.tree_count; i++) {
		if (strcmp(ctx->plan.trees[i], tree) == 0) {
			return (0);
		}
	}
	if (ctx->plan.tree_count >= INST_TREES_MAX) {
		inst_fail(ctx, "the module map spans more than %d top-level "
		    "directories", INST_TREES_MAX);
		return (-1);
	}
	if (strlen(tree) + 1 > sizeof(ctx->plan.trees[0])) {
		inst_fail(ctx, "directory name too long: %s", tree);
		return (-1);
	}
	(void)snprintf(ctx->plan.trees[ctx->plan.tree_count],
	    sizeof(ctx->plan.trees[0]), "%s", tree);
	ctx->plan.tree_count++;
	return (0);
}


static int
inst_module_read(const char *name, inst_module_t *mod)
{
	char	key[INST_NAME_MAX + 32];
	int	reg;
	int	len;

	memset(mod, 0, sizeof(*mod));

	len = snprintf(key, sizeof(key), "%s.%s", INST_REG_MODULES, name);
	if (len < 0 || (size_t)len + 1 > sizeof(key)) {
		return (1);
	}
	reg = regOpen(INST_REG_HIVE, key, API_REG_OPEN_READ);
	if (reg < 0) {
		return (1);
	}
	if (regGetString(reg, INST_REG_DEST, mod->dest,
	    sizeof(mod->dest)) != 0 || mod->dest[0] != '/') {
		(void)regClose(reg);
		return (1);
	}
	if (regGetString(reg, INST_REG_ROLE, mod->role,
	    sizeof(mod->role)) != 0) {
		mod->role[0] = '\0';
	}
	(void)regClose(reg);

	(void)snprintf(mod->name, sizeof(mod->name), "%s", name);
	return (0);
}

int
inst_plan_load(inst_ctx_t *ctx)
{
	struct api_reg_entry	ent;
	inst_module_t		mod;
	char			tree[INST_PATH_MAX];
	uint32_t		index;
	int			reg;
	int			ret;

	if (ctx == NULL) {
		return (-1);
	}
	memset(&ctx->plan, 0, sizeof(ctx->plan));


	reg = regOpen(INST_REG_HIVE, INST_REG_MODULES, API_REG_OPEN_READ);
	if (reg < 0) {
		inst_fail(ctx, "cannot read %s.%s: %s", INST_REG_HIVE,
		    INST_REG_MODULES, strerror(errno));
		return (-1);
	}

	for (index = 0;; index++) {
		memset(&ent, 0, sizeof(ent));
		ent.index = index;
		ret = regEnum(reg, &ent);
		if (ret < 0) {
			inst_fail(ctx, "cannot enumerate %s.%s: %s",
			    INST_REG_HIVE, INST_REG_MODULES, strerror(errno));
			(void)regClose(reg);
			return (-1);
		}
		if (ret == 0) {
			break;
		}
		if (ent.kind != API_REG_KIND_KEY || ent.name[0] == '\0') {
			continue;
		}
		if (ctx->plan.module_count >= INST_MODULES_MAX) {
			inst_fail(ctx, "%s.%s lists more than %d modules",
			    INST_REG_HIVE, INST_REG_MODULES,
			    INST_MODULES_MAX);
			(void)regClose(reg);
			return (-1);
		}

		ret = inst_module_read(ent.name, &mod);
		if (ret < 0) {
			(void)regClose(reg);
			return (-1);
		}
		if (ret > 0) {
			continue;
		}

		ctx->plan.modules[ctx->plan.module_count] = mod;
		ctx->plan.module_count++;

		ret = inst_top_level(mod.dest, tree, sizeof(tree));
		if (ret < 0) {
			inst_fail(ctx, "module %s has an unusable destination: "
			    "%s", mod.name, mod.dest);
			(void)regClose(reg);
			return (-1);
		}
		if (ret == 0 && inst_tree_add(ctx, tree) != 0) {
			(void)regClose(reg);
			return (-1);
		}
	}
	(void)regClose(reg);

	if (ctx->plan.module_count == 0) {
		inst_fail(ctx, "%s.%s declares no modules", INST_REG_HIVE,
		    INST_REG_MODULES);
		return (-1);
	}
	if (ctx->plan.tree_count == 0) {
		inst_fail(ctx, "no module destination names a directory to "
		    "install");
		return (-1);
	}

	if (inst_module_by_role(&ctx->plan, INST_ROLE_KERNEL) == NULL) {
		inst_fail(ctx, "no module carries the %s role",
		    INST_ROLE_KERNEL);
		return (-1);
	}
	if (inst_module_by_role(&ctx->plan, INST_ROLE_CMSEED) == NULL) {
		inst_fail(ctx, "no module carries the %s role",
		    INST_ROLE_CMSEED);
		return (-1);
	}
	return (0);
}
