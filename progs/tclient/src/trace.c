/* !DEFINES!

$define %type tclient_trace as command-line verbosity and MTProto log sink
$define %func tclient_parse_args as function with args int, char **, out state
$define %func tclient_usage as procedure
$define %func tclient_trace_install as procedure with args state
$define %func tclient_trace as procedure with args state, format

*/

/* !SPACE!

$space %internal trace_sink, trace_level_name
$space %export tclient_parse_args, tclient_usage, tclient_trace_install
$space %export tclient_trace

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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */



#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "tclient.h"

static const char *
trace_level_name(int level)
{
	switch (level) {
	case MTP_LOG_ERROR:
		return ("error");
	case MTP_LOG_INFO:
		return ("info");
	case MTP_LOG_DEBUG:
		return ("debug");
	case MTP_LOG_TRACE:
		return ("trace");
	default:
		return ("log");
	}
}


static void
trace_sink(void *ctx, int level, const char *line)
{
	(void)ctx;
	fprintf(stderr, "tclient: mtproto %s: %s\n", trace_level_name(level),
	    line != NULL ? line : "");
	fflush(stderr);
}

void
tclient_usage(void)
{
	printf("usage: tclient [-v|-vv|-vvv]\n");
	printf("  -v    connection, handshake and request progress\n");
	printf("  -vv   adds per-message detail and every rejected check\n");
	printf("  -vvv  adds nonces and frame bytes\n");
}


int
tclient_parse_args(int argc, char **argv, tclient_state_t *st)
{
	int	i, j;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			return (1);
		}
		if (argv[i][0] == '-' && argv[i][1] == 'v') {
			j = 1;
			while (argv[i][j] == 'v') {
				j++;
			}

			if (argv[i][j] == '\0') {
				st->verbose += j - 1;
				continue;
			}
		}
		return (-1);
	}
	return (0);
}

void
tclient_trace_install(tclient_state_t *st)
{
	int	level;

	if (st->verbose <= 0) {
		return;
	}
	if (st->verbose == 1) {
		level = MTP_LOG_INFO;
	} else if (st->verbose == 2) {
		level = MTP_LOG_DEBUG;
	} else {
		level = MTP_LOG_TRACE;
	}
	mtpLogSet(level, trace_sink, NULL);

	trace_sink(NULL, MTP_LOG_INFO, "logging enabled");
}

void
tclient_trace(const tclient_state_t *st, const char *fmt, ...)
{
	va_list	ap;

	if (st->verbose <= 0) {
		return;
	}
	fprintf(stderr, "tclient: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	fflush(stderr);
}
