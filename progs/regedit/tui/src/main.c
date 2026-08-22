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

$define %type api_key_event as native keyboard event
$define %type rt_state as regedit tui global state

$define %func rt_usage as procedure with args void
$define %func rt_start_path as function with args rt_state *, const char *
$define %func rt_run as procedure with args rt_state *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal rt_usage, rt_start_path, rt_run
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <regedit/regedit.h>
#include <stdio.h>
#include <string.h>
#include "tui.h"

static rt_state_t	rt_state;

static void
rt_usage(void)
{
	printf("usage: regedit [tui] [hive[.key]]\n");
	printf("       regedit -h\n");
	printf("modes: tui (default). gui is not built yet.\n");
	printf("path separators: '.', '/' and '\\\\' are accepted.\n");
}

static int
rt_start_path(rt_state_t *st, const char *text)
{
	re_path_reset(&st->path);
	if (!text) {
		return (0);
	}
	errno = 0;
	if (re_path_parse(&st->path, text) != 0) {
		re_path_reset(&st->path);
		rt_status_fmt(st, "%s: %s", text, re_error(errno));
		return (-1);
	}
	return (0);
}

static void
rt_run(rt_state_t *st)
{
	struct api_key_event	ev;

	rt_screen_clear();
	rt_reload(st);
	if (st->path.hive[0] != '\0' && !st->loaded) {
		rt_status_fmt(st, "%s: %s", st->path.hive,
		    re_error(errno != 0 ? errno : ENOENT));
		re_path_reset(&st->path);
		rt_reload(st);
	}
	if (st->status[0] == '\0') {
		rt_status(st, "F1 help, Enter open, Tab pane, Ctrl+Q quit");
	}
	while (!st->quit) {
		rt_draw(st);
		rt_key_read(&ev);
		rt_dispatch(st, &ev);
	}
	rt_screen_reset();
	rt_screen_clear();
}

int
main(int argc, char **argv, char **envp)
{
	const char	*path;
	int		i;

	(void)envp;
	path = NULL;
	for (i = 1; i < argc; i++) {
		if (!argv[i]) {
			continue;
		}
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			rt_usage();
			return (0);
		}
		if (strcmp(argv[i], "gui") == 0) {
			fprintf(stderr, "regedit: gui frontend is not "
			    "built; only tui is available\n");
			return (1);
		}
		if (strcmp(argv[i], "tui") == 0) {
			continue;
		}
		if (path) {
			rt_usage();
			return (1);
		}
		path = argv[i];
	}

	memset(&rt_state, 0, sizeof(rt_state));
	rt_state.rows = RT_ROWS_MIN;
	rt_state.cols = RT_COLS_MIN;
	inputFlush();
	(void)rt_start_path(&rt_state, path);
	rt_run(&rt_state);
	return (0);
}
