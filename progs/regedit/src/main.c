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

$define %type re_mode as requested frontend selection

$define %func re_usage as procedure with args void
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal re_usage
$space %export main

*/

#include <regedit/frontend.h>
#include <stdio.h>
#include <string.h>

#define RE_MODE_AUTO	0
#define RE_MODE_GUI	1
#define RE_MODE_TUI	2

static void
re_usage(void)
{
	printf("usage: regedit [gui|tui] [hive[.key]]\n");
	printf("       regedit -h\n");
	printf("modes: default picks gui when a compositor answers on "
	    "%s,\n", "system.gui.swm");
	printf("       otherwise it falls back to tui. 'gui' and 'tui' "
	    "force one.\n");
	printf("path separators: '.', '/' and '\\\\' are accepted.\n");
}

int
main(int argc, char **argv, char **envp)
{
	const char	*path;
	int		mode, ret, i;

	(void)envp;
	path = NULL;
	mode = RE_MODE_AUTO;
	for (i = 1; i < argc; i++) {
		if (!argv[i]) {
			continue;
		}
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			re_usage();
			return (RG_OK);
		}
		if (strcmp(argv[i], "gui") == 0) {
			mode = RE_MODE_GUI;
			continue;
		}
		if (strcmp(argv[i], "tui") == 0) {
			mode = RE_MODE_TUI;
			continue;
		}
		if (path) {
			re_usage();
			return (RG_FAILED);
		}
		path = argv[i];
	}

	if (mode == RE_MODE_TUI) {
		return (rt_main(path));
	}

	ret = rg_main(path);
	if (ret != RG_UNAVAILABLE) {
		return (ret);
	}

	if (mode == RE_MODE_GUI) {
		fprintf(stderr, "regedit: no compositor on system.gui.swm; "
		    "start swm or run 'regedit tui'\n");
		return (RG_FAILED);
	}
	return (rt_main(path));
}
