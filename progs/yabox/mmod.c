/* !DEFINES!

$define %type api_kofo_info as native KOFO module metadata
$define %func mmod_usage as procedure with args void
$define %func mmod_state_name as function with args uint32_t
$define %func mmod_load as function with args const char *
$define %func mmod_info as function with args uint32_t
$define %func mmod_unload as function with args uint32_t
$define %func mmod_reload as function with args uint32_t, const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal mmod_usage, mmod_state_name
$space %internal mmod_load, mmod_info, mmod_unload, mmod_reload
$space %export main

*/

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
 * LIABLE FOR ANY DIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "yabox.h"

static void
mmod_usage(void)
{
	fprintf(stderr,
	    "usage: mmod load path\n"
	    "       mmod info id\n"
	    "       mmod unload id\n"
	    "       mmod reload id path\n");
}

static const char *
mmod_state_name(uint32_t state)
{
	switch (state) {
	case API_KOFO_STATE_EMPTY:
		return ("empty");
	case API_KOFO_STATE_LOADING:
		return ("loading");
	case API_KOFO_STATE_LOADED:
		return ("loaded");
	case API_KOFO_STATE_UNLOADING:
		return ("unloading");
	default:
		return ("unknown");
	}
}

static int
mmod_load(const char *path)
{
	int	id;

	id = kofoLoad(path, 0);
	if (id < 0) {
		ybx_error("mmod", path, errno);
		return (1);
	}
	printf("loaded %s id=%u\n", path, (uint32_t)id);
	return (0);
}

static int
mmod_info(uint32_t id)
{
	struct api_kofo_info	info;

	memset(&info, 0, sizeof(info));
	if (kofoInfo(id, &info) < 0) {
		ybx_error("mmod", "info", errno);
		return (1);
	}
	printf("id: %u\n", info.id);
	printf("name: %s\n", info.name);
	printf("version: %s\n", info.version);
	printf("state: %s\n", mmod_state_name(info.state));
	printf("path: %s\n", info.path);
	printf("image: 0x%llx size=%llu\n",
	    (unsigned long long)info.image_base,
	    (unsigned long long)info.image_size);
	printf("sections=%u symbols=%u imports=%u relocs=%u drivers=%u\n",
	    info.section_count, info.symbol_count, info.import_count,
	    info.reloc_count, info.driver_count);
	return (0);
}

static int
mmod_unload(uint32_t id)
{
	if (kofoUnload(id, 0) < 0) {
		ybx_error("mmod", "unload", errno);
		return (1);
	}
	printf("unloaded id=%u\n", id);
	return (0);
}

static int
mmod_reload(uint32_t id, const char *path)
{
	if (kofoUnload(id, 0) < 0) {
		ybx_error("mmod", "reload unload", errno);
		return (1);
	}
	return (mmod_load(path));
}

int
main(int argc, char **argv, char **envp)
{
	uint32_t	id;

	(void)envp;
	if (argc < 2) {
		mmod_usage();
		return (1);
	}
	if (strcmp(argv[1], "load") == 0) {
		if (argc != 3) {
			mmod_usage();
			return (1);
		}
		return (mmod_load(argv[2]));
	}
	if (strcmp(argv[1], "info") == 0) {
		if (argc != 3 || ybx_parse_u32(argv[2], &id) < 0) {
			mmod_usage();
			return (1);
		}
		return (mmod_info(id));
	}
	if (strcmp(argv[1], "unload") == 0) {
		if (argc != 3 || ybx_parse_u32(argv[2], &id) < 0) {
			mmod_usage();
			return (1);
		}
		return (mmod_unload(id));
	}
	if (strcmp(argv[1], "reload") == 0) {
		if (argc != 4 || ybx_parse_u32(argv[2], &id) < 0) {
			mmod_usage();
			return (1);
		}
		return (mmod_reload(id, argv[3]));
	}
	mmod_usage();
	return (1);
}
