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

$define %type char as 8 bit signed
$define %type size_t as native object size
$define %type uint64_t as 64 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type inst_ctx_t as one installer run
$define %type inst_plan_t as the module map read from the registry
$define %type inst_module_t as one module of the module map

$define %func inst_fail as procedure with args inst_ctx_t *, const char *, ...
$define %func inst_error as function with args const inst_ctx_t *
$define %func inst_error_clear as procedure with args inst_ctx_t *
$define %func inst_module_by_role as function with args const inst_plan_t *, const char *
$define %func inst_size_label as procedure with args char *, size_t, uint64_t, uint32_t

*/

/* !SPACE!

$space %export inst_fail, inst_error, inst_error_clear
$space %export inst_module_by_role, inst_size_label

*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "inst.h"


void
inst_fail(inst_ctx_t *ctx, const char *fmt, ...)
{
	va_list	ap;

	if (ctx == NULL || fmt == NULL || ctx->err[0] != '\0') {
		return;
	}
	va_start(ap, fmt);
	(void)vsnprintf(ctx->err, sizeof(ctx->err), fmt, ap);
	va_end(ap);
}

const char *
inst_error(const inst_ctx_t *ctx)
{
	if (ctx == NULL || ctx->err[0] == '\0') {
		return ("unknown failure");
	}
	return (ctx->err);
}

void
inst_error_clear(inst_ctx_t *ctx)
{
	if (ctx != NULL) {
		ctx->err[0] = '\0';
	}
}

const inst_module_t *
inst_module_by_role(const inst_plan_t *plan, const char *role)
{
	int	i;

	if (plan == NULL || role == NULL || role[0] == '\0') {
		return (NULL);
	}
	for (i = 0; i < plan->module_count; i++) {
		if (strcmp(plan->modules[i].role, role) == 0) {
			return (&plan->modules[i]);
		}
	}
	return (NULL);
}


void
inst_size_label(char *out, size_t size, uint64_t sectors, uint32_t sector_size)
{
	static const char	*unit[] = { "B", "KiB", "MiB", "GiB", "TiB",
				    "PiB" };
	uint64_t		bytes;
	uint64_t		whole;
	uint64_t		frac;
	unsigned int		idx;

	if (out == NULL || size == 0) {
		return;
	}
	if (sector_size == 0) {

		(void)snprintf(out, size, "unknown size");
		return;
	}

	bytes = sectors * (uint64_t)sector_size;
	idx = 0;
	whole = bytes;
	frac = 0;
	while (whole >= 1024 && idx + 1 < sizeof(unit) / sizeof(unit[0])) {
		frac = whole % 1024;
		whole /= 1024;
		idx++;
	}
	frac = (frac * 10) / 1024;

	if (idx == 0) {
		(void)snprintf(out, size, "%llu %s",
		    (unsigned long long)whole, unit[idx]);
		return;
	}
	(void)snprintf(out, size, "%llu.%llu %s", (unsigned long long)whole,
	    (unsigned long long)frac, unit[idx]);
}
