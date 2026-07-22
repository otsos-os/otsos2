/* !DEFINES!

$define %type la_zip_options as zip extraction options
$define %func unzip_entry as function with args name, path, size, arg
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal unzip_entry
$space %export main

*/

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

#include <errno.h>
#include <libarchive.h>
#include <stdint.h>
#include <stdio.h>
#include "yabox.h"

static int
unzip_entry(const char *name, const char *path, uint64_t size, void *arg)
{
	(void)path;
	(void)size;
	(void)arg;
	printf("extract %s\n", name);
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	struct la_zip_options	opts;
	const char		*dest;
	int			code;

	(void)envp;
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: unzip archive.zip [directory]\n");
		return (1);
	}
	dest = ".";
	if (argc == 3) {
		dest = argv[2];
	}
	opts.dest_dir = dest;
	opts.on_entry = unzip_entry;
	opts.arg = NULL;
	if (la_zip_extract(argv[1], &opts) < 0) {
		code = errno;
		ybx_error("unzip", argv[1], code);
		return (1);
	}
	return (0);
}
