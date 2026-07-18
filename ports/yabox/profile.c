/* !DEFINES!

$define %type profile_opts_t as parsed profile command settings
$define %type profile_manifest_t as trace names from kernel
$define %type profile_stats_t as aggregated trace counters
$define %type api_trace_record as one kernel trace record
$define %func profile_usage as procedure with args void
$define %func profile_opts_init as procedure with args profile_opts_t *
$define %func profile_manifest_load as function with args profile_manifest_t *
$define %func profile_parse_argv as function with args options, manifest, argv
$define %func profile_run as function with args options, manifest, envp
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal profile_usage, profile_opts_init, profile_manifest_load
$space %internal profile_parse_argv, profile_run
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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yabox.h"

#define	PROFILE_ABI_NATIVE	0
#define	PROFILE_ABI_POSIX	1
#define	PROFILE_READ_BATCH	128
#define	PROFILE_RAW_LIMIT	128
#define	PROFILE_TOKEN_MAX	64
#define	PROFILE_SYSCALL_MAX	64

typedef struct profile_opts {
	uint64_t	source_mask;
	uint64_t	event_mask[API_TRACE_EVENT_WORDS];
	const char	*output;
	int		argc;
	char		**argv;
	uint32_t	raw_limit;
	int		abi;
	int		system;
	int		kernel_stacks;
	int		cpu;
	int		tid;
	int		source_selected;
	int		event_selected;
	int		show_summary;
	int		show_events;
	int		show_sources;
	int		show_pmu;
	int		show_syscalls;
	int		show_raw;
	int		show_stacks;
	int		csv;
	int		quiet;
	int		list_events;
	int		list_sources;
	int		list_pmu;
} profile_opts_t;

typedef struct profile_manifest {
	struct api_trace_event		events[API_TRACE_MAX_EVENTS];
	struct api_trace_source		sources[API_TRACE_SOURCE_COUNT];
	struct api_trace_pmu_counter	pmu[API_TRACE_MAX_PMU_COUNTERS];
	uint32_t			event_count;
	uint32_t			source_count;
	uint32_t			pmu_count;
} profile_manifest_t;

typedef struct profile_syscall_stat {
	uint64_t	nr;
	uint64_t	count;
	uint64_t	cycles;
	uint64_t	errors;
} profile_syscall_stat_t;

typedef struct profile_stats {
	uint64_t	event_count[API_TRACE_MAX_EVENTS];
	uint64_t	source_count[API_TRACE_SOURCE_COUNT];
	uint64_t	pmu_sample[API_TRACE_PMU_COUNTER_COUNT];
	uint64_t	pmu_counter[API_TRACE_PMU_COUNTER_COUNT];
	profile_syscall_stat_t syscalls[PROFILE_SYSCALL_MAX];
	uint64_t	records;
	uint64_t	lost;
	uint64_t	raw_printed;
	uint64_t	raw_seen;
	uint64_t	min_tsc;
	uint64_t	max_tsc;
	uint64_t	syscall_count;
	uint64_t	syscall_cycles;
	uint64_t	syscall_errors;
	uint64_t	syscall_max_cycles;
	uint64_t	syscall_max_nr;
	uint64_t	sched_switches;
	uint64_t	runtime_cycles;
	uint64_t	irq_count;
	uint64_t	exception_count;
	uint64_t	kqueue_count;
	uint64_t	pmu_sample_count;
	uint64_t	pmu_counter_count;
	int		have_tsc;
	int		pid;
	int		wait_pid;
	int		exit_status;
} profile_stats_t;

static void
profile_usage(void)
{
	fprintf(stderr,
	    "usage: profile [options] program [args...]\n"
	    "       profile --list-events|--list-sources|--list-pmu\n"
	    "\n"
	    "run mode:\n"
	    "  --native              spawn with native ABI (default)\n"
	    "  --posix               spawn with POSIX ABI\n"
	    "  --abi native|posix    explicit ABI\n"
	    "  --system              trace all processes while program runs\n"
	    "  --cpu N               keep records from one CPU\n"
	    "  --tid N               keep records from one thread id\n"
	    "  --kernel-stacks       request kernel stack capture permission\n"
	    "\n"
	    "filters:\n"
	    "  -e, --event LIST      trace only events by id/name/provider:name\n"
	    "  --no-event LIST       disable events from current selection\n"
	    "  --source LIST         trace only sources by id/name\n"
	    "  --no-source LIST      disable sources from current selection\n"
	    "  --no-pmu              disable PMU sample/counter events\n"
	    "  --no-syscalls         disable syscall source and summary\n"
	    "\n"
	    "output:\n"
	    "  --summary|--no-summary\n"
	    "  --events|--no-events\n"
	    "  --sources             print source counters\n"
	    "  --pmu|--no-pmu-output print or hide PMU totals\n"
	    "  --syscalls|--no-syscall-output\n"
	    "  --full                summary, events, sources, PMU, syscalls\n"
	    "  --raw                 dump trace records too\n"
	    "  --csv                 raw CSV only\n"
	    "  --stacks              include captured stack slots in raw dump\n"
	    "  --raw-limit N         raw rows to print; 0 means all\n"
	    "  -o, --output FILE     write report to file\n"
	    "  -q, --quiet           suppress header lines\n"
	    "\n"
	    "lists accept comma-separated values and can be repeated.\n");
}

static void
profile_mask_all(uint64_t *mask)
{
	int	i;

	for (i = 0; i < API_TRACE_EVENT_WORDS; i++) {
		mask[i] = ~0ULL;
	}
}

static void
profile_mask_set(uint64_t *mask, uint32_t event, int enabled)
{
	uint64_t	bit;
	uint32_t	word;

	if (event >= API_TRACE_MAX_EVENTS) {
		return;
	}
	word = event / 64;
	bit = 1ULL << (event % 64);
	if (enabled) {
		mask[word] |= bit;
	} else {
		mask[word] &= ~bit;
	}
}

static int
profile_parse_u32_any(const char *text, uint32_t *out)
{
	char		*end;
	unsigned long	value;

	if (!text || text[0] == '\0' || !out) {
		errno = EINVAL;
		return (-1);
	}
	errno = 0;
	value = strtoul(text, &end, 0);
	if (errno != 0 || !end || *end != '\0' || value > 0xffffffffUL) {
		errno = EINVAL;
		return (-1);
	}
	*out = (uint32_t)value;
	return (0);
}

static int
profile_file_exists(const char *path)
{
	int	fd, code;

	fd = dataOpen(path, API_OPEN_READ);
	if (fd >= 0) {
		dataClose(fd);
		return (1);
	}
	code = errno;
	if (code == ENOENT) {
		return (0);
	}
	errno = code;
	return (-1);
}

static int
profile_resolve_path(const char *cmd, char *out, size_t out_size)
{
	const char	*prefix;
	size_t		plen, clen;
	int		exists;

	if (!cmd || cmd[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	if (cmd[0] == '/' || cmd[0] == '.') {
		return (ybx_copy_path(out, out_size, cmd));
	}

	prefix = "/bin/";
	plen = strlen(prefix);
	clen = strlen(cmd);
	if (plen + clen + 1 > out_size) {
		errno = E2BIG;
		return (-1);
	}
	memcpy(out, prefix, plen);
	memcpy(out + plen, cmd, clen + 1);
	exists = profile_file_exists(out);
	if (exists == 1) {
		return (0);
	}
	if (exists == 0) {
		errno = ENOENT;
	}
	return (-1);
}

static const char *
profile_need_arg(int argc, char **argv, int *index, const char *name)
{
	if (*index + 1 >= argc) {
		fprintf(stderr, "profile: %s needs an argument\n", name);
		return (NULL);
	}
	(*index)++;
	return (argv[*index]);
}

static const struct api_trace_event *
profile_event_by_id(const profile_manifest_t *m, uint32_t id)
{
	uint32_t	i;

	for (i = 0; i < m->event_count; i++) {
		if (m->events[i].id == id) {
			return (&m->events[i]);
		}
	}
	return (NULL);
}

static const struct api_trace_source *
profile_source_by_id(const profile_manifest_t *m, uint32_t id)
{
	uint32_t	i;

	for (i = 0; i < m->source_count; i++) {
		if (m->sources[i].id == id) {
			return (&m->sources[i]);
		}
	}
	return (NULL);
}

static const char *
profile_source_name(const profile_manifest_t *m, uint32_t id)
{
	const struct api_trace_source	*src;

	src = profile_source_by_id(m, id);
	if (!src) {
		return ("?");
	}
	return (src->name);
}

static int
profile_match_event_token(const struct api_trace_event *ev,
    const char *token)
{
	const char	*colon;
	size_t		len;

	colon = strchr(token, ':');
	if (!colon) {
		return (strcmp(ev->name, token) == 0);
	}
	len = (size_t)(colon - token);
	if (strlen(ev->provider) != len) {
		return (0);
	}
	if (strncmp(ev->provider, token, len) != 0) {
		return (0);
	}
	return (strcmp(ev->name, colon + 1) == 0);
}

static int
profile_source_id(const profile_manifest_t *m, const char *token,
    uint32_t *id)
{
	uint32_t	value, i;

	if (profile_parse_u32_any(token, &value) == 0) {
		if (value >= API_TRACE_SOURCE_COUNT) {
			errno = EINVAL;
			return (-1);
		}
		*id = value;
		return (0);
	}
	for (i = 0; i < m->source_count; i++) {
		if (strcmp(m->sources[i].name, token) == 0) {
			*id = m->sources[i].id;
			return (0);
		}
	}
	errno = EINVAL;
	return (-1);
}

static int
profile_event_id(const profile_manifest_t *m, const char *token,
    uint32_t *id)
{
	uint32_t	value, i;

	if (profile_parse_u32_any(token, &value) == 0) {
		if (value >= API_TRACE_MAX_EVENTS) {
			errno = EINVAL;
			return (-1);
		}
		*id = value;
		return (0);
	}
	for (i = 0; i < m->event_count; i++) {
		if (profile_match_event_token(&m->events[i], token)) {
			*id = m->events[i].id;
			return (0);
		}
	}
	errno = EINVAL;
	return (-1);
}

static int
profile_source_list(profile_opts_t *opts, const profile_manifest_t *m,
    const char *list, int enabled)
{
	char		token[PROFILE_TOKEN_MAX];
	const char	*start, *p;
	uint32_t	id;
	size_t		len;

	if (enabled && !opts->source_selected) {
		opts->source_mask = 0;
		opts->source_selected = 1;
	}
	start = list;
	p = list;
	for (;;) {
		if (*p == ',' || *p == '\0') {
			len = (size_t)(p - start);
			if (len == 0 || len >= sizeof(token)) {
				errno = EINVAL;
				return (-1);
			}
			memcpy(token, start, len);
			token[len] = '\0';
			if (profile_source_id(m, token, &id) != 0) {
				return (-1);
			}
			if (enabled) {
				opts->source_mask |= 1ULL << id;
			} else {
				opts->source_mask &= ~(1ULL << id);
			}
			if (*p == '\0') {
				break;
			}
			start = p + 1;
		}
		p++;
	}
	return (0);
}

static int
profile_event_list(profile_opts_t *opts, const profile_manifest_t *m,
    const char *list, int enabled)
{
	char		token[PROFILE_TOKEN_MAX];
	const char	*start, *p;
	uint32_t	id;
	size_t		len;

	if (enabled && !opts->event_selected) {
		memset(opts->event_mask, 0, sizeof(opts->event_mask));
		opts->event_selected = 1;
	}
	start = list;
	p = list;
	for (;;) {
		if (*p == ',' || *p == '\0') {
			len = (size_t)(p - start);
			if (len == 0 || len >= sizeof(token)) {
				errno = EINVAL;
				return (-1);
			}
			memcpy(token, start, len);
			token[len] = '\0';
			if (profile_event_id(m, token, &id) != 0) {
				return (-1);
			}
			profile_mask_set(opts->event_mask, id, enabled);
			if (*p == '\0') {
				break;
			}
			start = p + 1;
		}
		p++;
	}
	return (0);
}

static void
profile_disable_pmu(profile_opts_t *opts)
{
	opts->source_mask &= ~(1ULL << API_TRACE_SOURCE_PMU);
	profile_mask_set(opts->event_mask, API_TRACE_EV_PROFILE_SAMPLE, 0);
	profile_mask_set(opts->event_mask, API_TRACE_EV_PMU_COUNTERS, 0);
	opts->show_pmu = 0;
}

static void
profile_disable_syscalls(profile_opts_t *opts)
{
	opts->source_mask &= ~(1ULL << API_TRACE_SOURCE_SYSCALL);
	profile_mask_set(opts->event_mask, API_TRACE_EV_SYSCALL_ENTER, 0);
	profile_mask_set(opts->event_mask, API_TRACE_EV_SYSCALL_EXIT, 0);
	opts->show_syscalls = 0;
}

static void
profile_opts_init(profile_opts_t *opts)
{
	memset(opts, 0, sizeof(*opts));
	opts->source_mask = API_TRACE_SOURCE_MASK_ALL;
	profile_mask_all(opts->event_mask);
	opts->raw_limit = PROFILE_RAW_LIMIT;
	opts->abi = PROFILE_ABI_NATIVE;
	opts->cpu = -1;
	opts->tid = -1;
	opts->show_summary = 1;
	opts->show_events = 1;
	opts->show_pmu = 1;
	opts->show_syscalls = 1;
}

static int
profile_manifest_load(profile_manifest_t *m)
{
	struct api_trace_events	evq;
	struct api_trace_sources	srq;
	struct api_trace_pmu	pmq;

	memset(m, 0, sizeof(*m));
	memset(&evq, 0, sizeof(evq));
	evq.events = m->events;
	evq.max_events = API_TRACE_MAX_EVENTS;
	if (traceInfo(API_TRACE_INFO_EVENTS, &evq) < 0) {
		return (-1);
	}
	m->event_count = evq.count;
	if (m->event_count > API_TRACE_MAX_EVENTS) {
		m->event_count = API_TRACE_MAX_EVENTS;
	}

	memset(&srq, 0, sizeof(srq));
	srq.sources = m->sources;
	srq.max_sources = API_TRACE_SOURCE_COUNT;
	if (traceInfo(API_TRACE_INFO_SOURCES, &srq) < 0) {
		return (-1);
	}
	m->source_count = srq.count;
	if (m->source_count > API_TRACE_SOURCE_COUNT) {
		m->source_count = API_TRACE_SOURCE_COUNT;
	}

	memset(&pmq, 0, sizeof(pmq));
	pmq.counters = m->pmu;
	pmq.max_counters = API_TRACE_MAX_PMU_COUNTERS;
	if (traceInfo(API_TRACE_INFO_PMU, &pmq) < 0) {
		return (-1);
	}
	m->pmu_count = pmq.count;
	if (m->pmu_count > API_TRACE_MAX_PMU_COUNTERS) {
		m->pmu_count = API_TRACE_MAX_PMU_COUNTERS;
	}
	return (0);
}

static int
profile_parse_abi(profile_opts_t *opts, const char *value)
{
	if (strcmp(value, "native") == 0) {
		opts->abi = PROFILE_ABI_NATIVE;
		return (0);
	}
	if (strcmp(value, "posix") == 0) {
		opts->abi = PROFILE_ABI_POSIX;
		return (0);
	}
	errno = EINVAL;
	return (-1);
}

static int
profile_parse_number_option(const char *value, int *out)
{
	uint32_t	n;

	if (profile_parse_u32_any(value, &n) != 0) {
		return (-1);
	}
	*out = (int)n;
	return (0);
}

static int
profile_parse_argv(profile_opts_t *opts, const profile_manifest_t *m,
    int argc, char **argv)
{
	const char	*value;
	int		i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}
		if (argv[i][0] != '-' || argv[i][1] == '\0') {
			break;
		}

		value = NULL;
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			profile_usage();
			return (1);
		} else if (strcmp(argv[i], "--list-events") == 0) {
			opts->list_events = 1;
		} else if (strcmp(argv[i], "--list-sources") == 0) {
			opts->list_sources = 1;
		} else if (strcmp(argv[i], "--list-pmu") == 0) {
			opts->list_pmu = 1;
		} else if (strcmp(argv[i], "--native") == 0) {
			opts->abi = PROFILE_ABI_NATIVE;
		} else if (strcmp(argv[i], "--posix") == 0) {
			opts->abi = PROFILE_ABI_POSIX;
		} else if (strncmp(argv[i], "--abi=", 6) == 0) {
			value = argv[i] + 6;
			if (profile_parse_abi(opts, value) != 0) {
				ybx_error("profile", argv[i], errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--abi") == 0) {
			value = profile_need_arg(argc, argv, &i, "--abi");
			if (!value) {
				return (-1);
			}
			if (profile_parse_abi(opts, value) != 0) {
				ybx_error("profile", value, errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--system") == 0) {
			opts->system = 1;
		} else if (strcmp(argv[i], "--kernel-stacks") == 0) {
			opts->kernel_stacks = 1;
		} else if (strncmp(argv[i], "--cpu=", 6) == 0) {
			if (profile_parse_number_option(argv[i] + 6,
			    &opts->cpu) != 0) {
				ybx_error("profile", argv[i], errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--cpu") == 0) {
			value = profile_need_arg(argc, argv, &i, "--cpu");
			if (!value) {
				return (-1);
			}
			if (profile_parse_number_option(value, &opts->cpu) != 0) {
				ybx_error("profile", value, errno);
				return (-1);
			}
		} else if (strncmp(argv[i], "--tid=", 6) == 0) {
			if (profile_parse_number_option(argv[i] + 6,
			    &opts->tid) != 0) {
				ybx_error("profile", argv[i], errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--tid") == 0) {
			value = profile_need_arg(argc, argv, &i, "--tid");
			if (!value) {
				return (-1);
			}
			if (profile_parse_number_option(value, &opts->tid) != 0) {
				ybx_error("profile", value, errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "-e") == 0 ||
		    strcmp(argv[i], "--event") == 0) {
			value = profile_need_arg(argc, argv, &i, argv[i]);
			if (!value) {
				return (-1);
			}
			if (profile_event_list(opts, m, value, 1) != 0) {
				ybx_error("profile", value, errno);
				return (-1);
			}
		} else if (strncmp(argv[i], "--event=", 8) == 0) {
			if (profile_event_list(opts, m, argv[i] + 8, 1) != 0) {
				ybx_error("profile", argv[i] + 8, errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--no-event") == 0) {
			value = profile_need_arg(argc, argv, &i, "--no-event");
			if (!value) {
				return (-1);
			}
			if (profile_event_list(opts, m, value, 0) != 0) {
				ybx_error("profile", value, errno);
				return (-1);
			}
		} else if (strncmp(argv[i], "--no-event=", 11) == 0) {
			if (profile_event_list(opts, m, argv[i] + 11, 0) != 0) {
				ybx_error("profile", argv[i] + 11, errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--source") == 0) {
			value = profile_need_arg(argc, argv, &i, "--source");
			if (!value) {
				return (-1);
			}
			if (profile_source_list(opts, m, value, 1) != 0) {
				ybx_error("profile", value, errno);
				return (-1);
			}
		} else if (strncmp(argv[i], "--source=", 9) == 0) {
			if (profile_source_list(opts, m, argv[i] + 9, 1) != 0) {
				ybx_error("profile", argv[i] + 9, errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--no-source") == 0) {
			value = profile_need_arg(argc, argv, &i, "--no-source");
			if (!value) {
				return (-1);
			}
			if (profile_source_list(opts, m, value, 0) != 0) {
				ybx_error("profile", value, errno);
				return (-1);
			}
		} else if (strncmp(argv[i], "--no-source=", 12) == 0) {
			if (profile_source_list(opts, m, argv[i] + 12, 0) != 0) {
				ybx_error("profile", argv[i] + 12, errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--no-pmu") == 0) {
			profile_disable_pmu(opts);
		} else if (strcmp(argv[i], "--no-syscalls") == 0) {
			profile_disable_syscalls(opts);
		} else if (strcmp(argv[i], "--summary") == 0) {
			opts->show_summary = 1;
		} else if (strcmp(argv[i], "--no-summary") == 0) {
			opts->show_summary = 0;
		} else if (strcmp(argv[i], "--events") == 0) {
			opts->show_events = 1;
		} else if (strcmp(argv[i], "--no-events") == 0) {
			opts->show_events = 0;
		} else if (strcmp(argv[i], "--sources") == 0) {
			opts->show_sources = 1;
		} else if (strcmp(argv[i], "--pmu") == 0) {
			opts->show_pmu = 1;
		} else if (strcmp(argv[i], "--no-pmu-output") == 0) {
			opts->show_pmu = 0;
		} else if (strcmp(argv[i], "--syscalls") == 0) {
			opts->show_syscalls = 1;
		} else if (strcmp(argv[i], "--no-syscall-output") == 0) {
			opts->show_syscalls = 0;
		} else if (strcmp(argv[i], "--full") == 0) {
			opts->show_summary = 1;
			opts->show_events = 1;
			opts->show_sources = 1;
			opts->show_pmu = 1;
			opts->show_syscalls = 1;
		} else if (strcmp(argv[i], "--raw") == 0) {
			opts->show_raw = 1;
		} else if (strcmp(argv[i], "--csv") == 0) {
			opts->show_raw = 1;
			opts->csv = 1;
			opts->show_summary = 0;
			opts->show_events = 0;
			opts->show_sources = 0;
			opts->show_pmu = 0;
			opts->show_syscalls = 0;
		} else if (strcmp(argv[i], "--stacks") == 0) {
			opts->show_stacks = 1;
		} else if (strncmp(argv[i], "--raw-limit=", 12) == 0) {
			if (profile_parse_u32_any(argv[i] + 12,
			    &opts->raw_limit) != 0) {
				ybx_error("profile", argv[i], errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "--raw-limit") == 0) {
			value = profile_need_arg(argc, argv, &i, "--raw-limit");
			if (!value) {
				return (-1);
			}
			if (profile_parse_u32_any(value, &opts->raw_limit) != 0) {
				ybx_error("profile", value, errno);
				return (-1);
			}
		} else if (strcmp(argv[i], "-o") == 0 ||
		    strcmp(argv[i], "--output") == 0) {
			opts->output = profile_need_arg(argc, argv, &i, argv[i]);
			if (!opts->output) {
				return (-1);
			}
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			opts->output = argv[i] + 9;
		} else if (strcmp(argv[i], "-q") == 0 ||
		    strcmp(argv[i], "--quiet") == 0) {
			opts->quiet = 1;
		} else {
			fprintf(stderr, "profile: unknown option %s\n", argv[i]);
			return (-1);
		}
	}

	if (opts->list_events || opts->list_sources || opts->list_pmu) {
		return (0);
	}
	if (i >= argc) {
		profile_usage();
		return (-1);
	}
	opts->argv = &argv[i];
	opts->argc = argc - i;
	return (0);
}

static void
profile_print_event_label(FILE *out, const profile_manifest_t *m,
    uint32_t id)
{
	const struct api_trace_event	*ev;

	ev = profile_event_by_id(m, id);
	if (!ev) {
		fprintf(out, "event%u", id);
		return;
	}
	fprintf(out, "%s:%s", ev->provider, ev->name);
}

static void
profile_list_events(FILE *out, const profile_manifest_t *m)
{
	const struct api_trace_event	*ev;
	const struct api_trace_field	*f;
	uint32_t			i, j;

	for (i = 0; i < m->event_count; i++) {
		ev = &m->events[i];
		fprintf(out, "%u\t%s\t%s:%s\t%s",
		    ev->id, profile_source_name(m, ev->source),
		    ev->provider, ev->name,
		    ev->enabled ? "on" : "off");
		for (j = 0; j < ev->field_count; j++) {
			f = &ev->fields[j];
			fprintf(out, "\t%s", f->name);
		}
		fprintf(out, "\n");
	}
}

static void
profile_list_sources(FILE *out, const profile_manifest_t *m)
{
	uint32_t	i;

	for (i = 0; i < m->source_count; i++) {
		fprintf(out, "%u\t%s\t%s\n", m->sources[i].id,
		    m->sources[i].name,
		    m->sources[i].enabled ? "on" : "off");
	}
}

static void
profile_list_pmu(FILE *out, const profile_manifest_t *m)
{
	uint32_t	i;

	for (i = 0; i < m->pmu_count; i++) {
		fprintf(out, "%u\t%s\t%s\n", m->pmu[i].id,
		    m->pmu[i].name,
		    m->pmu[i].enabled ? "on" : "off");
	}
}

static void
profile_syscall_add(profile_stats_t *stats, uint64_t nr,
    uint64_t cycles, int error)
{
	profile_syscall_stat_t	*slot;
	int			i, empty;

	empty = -1;
	for (i = 0; i < PROFILE_SYSCALL_MAX; i++) {
		slot = &stats->syscalls[i];
		if (slot->count == 0 && empty < 0) {
			empty = i;
			continue;
		}
		if (slot->count != 0 && slot->nr == nr) {
			slot->count++;
			slot->cycles += cycles;
			if (error) {
				slot->errors++;
			}
			return;
		}
	}
	if (empty < 0) {
		empty = PROFILE_SYSCALL_MAX - 1;
		nr = ~0ULL;
	}
	slot = &stats->syscalls[empty];
	if (slot->count == 0) {
		slot->nr = nr;
	}
	slot->count++;
	slot->cycles += cycles;
	if (error) {
		slot->errors++;
	}
}

static void
profile_print_raw(FILE *out, const profile_opts_t *opts,
    const profile_manifest_t *m, const struct api_trace_record *rec)
{
	uint32_t	i;

	if (opts->csv) {
		fprintf(out,
		    "%llu,%llu,%llu,%u,%llu,%llu,%u,%u,"
		    "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
		    (unsigned long long)rec->seq,
		    (unsigned long long)rec->tsc,
		    (unsigned long long)rec->ticks,
		    rec->cpu,
		    (unsigned long long)rec->pid,
		    (unsigned long long)rec->tid,
		    rec->source, rec->event,
		    (unsigned long long)rec->args[0],
		    (unsigned long long)rec->args[1],
		    (unsigned long long)rec->args[2],
		    (unsigned long long)rec->args[3],
		    (unsigned long long)rec->args[4],
		    (unsigned long long)rec->args[5],
		    (unsigned long long)rec->ip,
		    (unsigned long long)rec->sp,
		    (unsigned long long)rec->bp);
		return;
	}

	fprintf(out, "%llu cpu=%u pid=%llu tid=%llu ",
	    (unsigned long long)rec->seq, rec->cpu,
	    (unsigned long long)rec->pid,
	    (unsigned long long)rec->tid);
	fprintf(out, "%s ", profile_source_name(m, rec->source));
	profile_print_event_label(out, m, rec->event);
	fprintf(out,
	    " tsc=%llu ip=0x%llx args=%llu,%llu,%llu,%llu,%llu,%llu\n",
	    (unsigned long long)rec->tsc,
	    (unsigned long long)rec->ip,
	    (unsigned long long)rec->args[0],
	    (unsigned long long)rec->args[1],
	    (unsigned long long)rec->args[2],
	    (unsigned long long)rec->args[3],
	    (unsigned long long)rec->args[4],
	    (unsigned long long)rec->args[5]);
	if (opts->show_stacks && rec->stack_count > 0) {
		fprintf(out, "  stack:");
		for (i = 0; i < rec->stack_count &&
		    i < API_TRACE_RECORD_STACK; i++) {
			fprintf(out, " 0x%llx",
			    (unsigned long long)rec->stack[i]);
		}
		fprintf(out, "\n");
	}
}

static void
profile_record(profile_stats_t *stats, const struct api_trace_record *rec)
{
	uint64_t	cycles, ret;
	int		error;
	uint32_t	i;

	stats->records++;
	if (!stats->have_tsc) {
		stats->min_tsc = rec->tsc;
		stats->max_tsc = rec->tsc;
		stats->have_tsc = 1;
	} else {
		if (rec->tsc < stats->min_tsc) {
			stats->min_tsc = rec->tsc;
		}
		if (rec->tsc > stats->max_tsc) {
			stats->max_tsc = rec->tsc;
		}
	}
	if (rec->event < API_TRACE_MAX_EVENTS) {
		stats->event_count[rec->event]++;
	}
	if (rec->source < API_TRACE_SOURCE_COUNT) {
		stats->source_count[rec->source]++;
	}

	switch (rec->event) {
	case API_TRACE_EV_PROFILE_SAMPLE:
		stats->pmu_sample_count++;
		for (i = 0; i < API_TRACE_PMU_COUNTER_COUNT; i++) {
			stats->pmu_sample[i] += rec->args[i];
		}
		break;
	case API_TRACE_EV_PMU_COUNTERS:
		stats->pmu_counter_count++;
		for (i = 0; i < API_TRACE_PMU_COUNTER_COUNT; i++) {
			stats->pmu_counter[i] += rec->args[i];
		}
		break;
	case API_TRACE_EV_SYSCALL_EXIT:
		cycles = rec->args[2];
		ret = rec->args[1];
		error = ((int64_t)ret < 0);
		stats->syscall_count++;
		stats->syscall_cycles += cycles;
		if (cycles > stats->syscall_max_cycles) {
			stats->syscall_max_cycles = cycles;
			stats->syscall_max_nr = rec->args[0];
		}
		if (error) {
			stats->syscall_errors++;
		}
		profile_syscall_add(stats, rec->args[0], cycles, error);
		break;
	case API_TRACE_EV_SCHED_SWITCH:
		stats->sched_switches++;
		if (stats->pid > 0 && rec->args[0] == (uint64_t)stats->pid) {
			stats->runtime_cycles += rec->args[5];
		}
		break;
	case API_TRACE_EV_IRQ_ENTER:
	case API_TRACE_EV_IRQ_EXIT:
		stats->irq_count++;
		break;
	case API_TRACE_EV_EXCEPTION:
		stats->exception_count++;
		break;
	case API_TRACE_EV_EVENT_KQUEUE_CREATE:
	case API_TRACE_EV_EVENT_KQUEUE_DESTROY:
	case API_TRACE_EV_EVENT_KNOTE_READY:
	case API_TRACE_EV_EVENT_KEVENT_WAIT:
	case API_TRACE_EV_EVENT_KEVENT_RETURN:
	case API_TRACE_EV_EVENT_TIMER_TICK:
		stats->kqueue_count++;
		break;
	default:
		break;
	}
}

static int
profile_read_all(int trace, const profile_opts_t *opts,
    const profile_manifest_t *m, profile_stats_t *stats, FILE *out)
{
	struct api_trace_record	records[PROFILE_READ_BATCH];
	struct api_trace_read	args;
	ssize_t			n;
	int			i;

	if (opts->csv && opts->show_raw) {
		fprintf(out,
		    "seq,tsc,ticks,cpu,pid,tid,source,event,"
		    "arg0,arg1,arg2,arg3,arg4,arg5,ip,sp,bp\n");
	}
	for (;;) {
		memset(&args, 0, sizeof(args));
		args.records = records;
		args.max_records = PROFILE_READ_BATCH;
		n = traceRead(trace, &args);
		if (n < 0) {
			ybx_error("profile", "traceRead", errno);
			return (-1);
		}
		stats->lost = args.lost_records;
		if (n == 0) {
			break;
		}
		for (i = 0; i < n; i++) {
			stats->raw_seen++;
			if (opts->show_raw &&
			    (opts->raw_limit == 0 ||
			    stats->raw_printed < opts->raw_limit)) {
				profile_print_raw(out, opts, m, &records[i]);
				stats->raw_printed++;
			}
			profile_record(stats, &records[i]);
		}
	}
	return (0);
}

static const char *
profile_pmu_name(const profile_manifest_t *m, uint32_t id)
{
	uint32_t	i;

	for (i = 0; i < m->pmu_count; i++) {
		if (m->pmu[i].id == id) {
			return (m->pmu[i].name);
		}
	}
	return ("?");
}

static void
profile_print_pmu_set(FILE *out, const profile_manifest_t *m,
    const char *title, const uint64_t *values, uint64_t count)
{
	uint32_t	i;

	if (count == 0) {
		return;
	}
	fprintf(out, "%s (%llu samples)\n", title,
	    (unsigned long long)count);
	for (i = 0; i < API_TRACE_PMU_COUNTER_COUNT; i++) {
		fprintf(out, "  %-22s %llu\n", profile_pmu_name(m, i),
		    (unsigned long long)values[i]);
	}
}

static void
profile_print_summary(FILE *out, const profile_opts_t *opts,
    const profile_stats_t *stats)
{
	uint64_t	elapsed;

	if (!opts->show_summary) {
		return;
	}
	elapsed = 0;
	if (stats->have_tsc && stats->max_tsc >= stats->min_tsc) {
		elapsed = stats->max_tsc - stats->min_tsc;
	}
	fprintf(out, "summary\n");
	fprintf(out, "  pid              %d\n", stats->pid);
	fprintf(out, "  wait_pid         %d\n", stats->wait_pid);
	fprintf(out, "  exit_status      %d\n", stats->exit_status);
	fprintf(out, "  records          %llu\n",
	    (unsigned long long)stats->records);
	fprintf(out, "  lost             %llu\n",
	    (unsigned long long)stats->lost);
	fprintf(out, "  tsc_elapsed      %llu\n",
	    (unsigned long long)elapsed);
	fprintf(out, "  sched_switches   %llu\n",
	    (unsigned long long)stats->sched_switches);
	fprintf(out, "  runtime_cycles   %llu\n",
	    (unsigned long long)stats->runtime_cycles);
	fprintf(out, "  irq_events       %llu\n",
	    (unsigned long long)stats->irq_count);
	fprintf(out, "  exceptions       %llu\n",
	    (unsigned long long)stats->exception_count);
	fprintf(out, "  event_events     %llu\n",
	    (unsigned long long)stats->kqueue_count);
}

static void
profile_print_events(FILE *out, const profile_opts_t *opts,
    const profile_manifest_t *m, const profile_stats_t *stats)
{
	uint32_t	i, id;

	if (!opts->show_events) {
		return;
	}
	fprintf(out, "events\n");
	for (i = 0; i < m->event_count; i++) {
		id = m->events[i].id;
		if (id >= API_TRACE_MAX_EVENTS || stats->event_count[id] == 0) {
			continue;
		}
		fprintf(out, "  ");
		profile_print_event_label(out, m, id);
		fprintf(out, " %llu\n",
		    (unsigned long long)stats->event_count[id]);
	}
}

static void
profile_print_sources(FILE *out, const profile_opts_t *opts,
    const profile_manifest_t *m, const profile_stats_t *stats)
{
	uint32_t	i, id;

	if (!opts->show_sources) {
		return;
	}
	fprintf(out, "sources\n");
	for (i = 0; i < m->source_count; i++) {
		id = m->sources[i].id;
		if (id >= API_TRACE_SOURCE_COUNT || stats->source_count[id] == 0) {
			continue;
		}
		fprintf(out, "  %-12s %llu\n", m->sources[i].name,
		    (unsigned long long)stats->source_count[id]);
	}
}

static void
profile_print_syscalls(FILE *out, const profile_opts_t *opts,
    const profile_stats_t *stats)
{
	uint64_t	best_count;
	int		used[PROFILE_SYSCALL_MAX];
	int		i, j, best;

	if (!opts->show_syscalls || stats->syscall_count == 0) {
		return;
	}
	memset(used, 0, sizeof(used));
	fprintf(out, "syscalls\n");
	fprintf(out, "  count            %llu\n",
	    (unsigned long long)stats->syscall_count);
	fprintf(out, "  errors           %llu\n",
	    (unsigned long long)stats->syscall_errors);
	fprintf(out, "  cycles           %llu\n",
	    (unsigned long long)stats->syscall_cycles);
	fprintf(out, "  max              nr=0x%llx cycles=%llu\n",
	    (unsigned long long)stats->syscall_max_nr,
	    (unsigned long long)stats->syscall_max_cycles);
	fprintf(out, "  top\n");
	for (i = 0; i < PROFILE_SYSCALL_MAX; i++) {
		best = -1;
		best_count = 0;
		for (j = 0; j < PROFILE_SYSCALL_MAX; j++) {
			if (used[j] || stats->syscalls[j].count == 0) {
				continue;
			}
			if (stats->syscalls[j].count > best_count) {
				best_count = stats->syscalls[j].count;
				best = j;
			}
		}
		if (best < 0) {
			break;
		}
		used[best] = 1;
		fprintf(out, "    nr=0x%llx count=%llu cycles=%llu errors=%llu\n",
		    (unsigned long long)stats->syscalls[best].nr,
		    (unsigned long long)stats->syscalls[best].count,
		    (unsigned long long)stats->syscalls[best].cycles,
		    (unsigned long long)stats->syscalls[best].errors);
	}
}

static void
profile_print_report(FILE *out, const profile_opts_t *opts,
    const profile_manifest_t *m, const profile_stats_t *stats)
{
	if (!opts->quiet && !opts->csv) {
		fprintf(out, "profile: %s pid=%d status=%d\n",
		    opts->argv[0], stats->pid, stats->exit_status);
	}
	profile_print_summary(out, opts, stats);
	profile_print_events(out, opts, m, stats);
	profile_print_sources(out, opts, m, stats);
	if (opts->show_pmu) {
		profile_print_pmu_set(out, m, "pmu profile",
		    stats->pmu_sample, stats->pmu_sample_count);
		profile_print_pmu_set(out, m, "pmu counters",
		    stats->pmu_counter, stats->pmu_counter_count);
	}
	profile_print_syscalls(out, opts, stats);
}

static int
profile_apply_filter(int trace, const profile_opts_t *opts, int pid)
{
	struct api_trace_filter	filter;
	int			i;

	memset(&filter, 0, sizeof(filter));
	filter.source_mask = opts->source_mask;
	for (i = 0; i < API_TRACE_EVENT_WORDS; i++) {
		filter.event_mask[i] = opts->event_mask[i];
	}
	filter.pid = -1;
	filter.tid = -1;
	filter.cpu = -1;
	if (!opts->system) {
		filter.flags |= API_TRACE_FILTER_HAS_PID;
		filter.pid = pid;
	}
	if (opts->tid >= 0) {
		filter.flags |= API_TRACE_FILTER_HAS_TID;
		filter.tid = opts->tid;
	}
	if (opts->cpu >= 0) {
		filter.flags |= API_TRACE_FILTER_HAS_CPU;
		filter.cpu = opts->cpu;
	}
	if (traceCtl(trace, API_TRACE_OP_SET_FILTER, &filter) < 0) {
		ybx_error("profile", "trace filter", errno);
		return (-1);
	}
	return (0);
}

static int
profile_wait_child(int pid, int *status)
{
	int	got, code;

	for (;;) {
		got = procWait(status);
		if (got == pid) {
			return (got);
		}
		if (got > 0) {
			continue;
		}
		code = errno;
		ybx_error("profile", "wait", code);
		return (-1);
	}
}

static int
profile_spawn(const profile_opts_t *opts, const char *path, char **envp)
{
	if (opts->abi == PROFILE_ABI_POSIX) {
		return (procSpawn(path, opts->argv, envp));
	}
	return (procSpawnNative(path, opts->argv, envp));
}

static int
profile_run(profile_opts_t *opts, const profile_manifest_t *m, char **envp)
{
	profile_stats_t	stats;
	char		path[YBX_PATH_MAX];
	FILE		*out;
	uint32_t	flags;
	int		trace, pid, status, ret;

	out = stdout;
	if (opts->output) {
		out = fopen(opts->output, "w");
		if (!out) {
			ybx_error("profile", opts->output, errno);
			return (1);
		}
	}

	if (profile_resolve_path(opts->argv[0], path, sizeof(path)) != 0) {
		ybx_error("profile", opts->argv[0], errno);
		if (out != stdout) {
			fclose(out);
		}
		return (1);
	}

	flags = 0;
	if (opts->system) {
		flags |= API_TRACE_OPEN_SYSTEM;
	}
	if (opts->kernel_stacks) {
		flags |= API_TRACE_OPEN_KERNEL_STACK;
	}

	trace = traceOpen(flags);
	if (trace < 0) {
		ybx_error("profile", "traceOpen", errno);
		if (out != stdout) {
			fclose(out);
		}
		return (1);
	}
	traceCtl(trace, API_TRACE_OP_STOP, NULL);
	traceCtl(trace, API_TRACE_OP_FLUSH, NULL);

	ret = 0;
	status = 0;
	pid = -1;
	memset(&stats, 0, sizeof(stats));

	if (opts->system && profile_apply_filter(trace, opts, 0) != 0) {
		ret = 1;
		goto done;
	}
	if (opts->system && traceCtl(trace, API_TRACE_OP_START, NULL) < 0) {
		ybx_error("profile", "trace start", errno);
		ret = 1;
		goto done;
	}
	if (opts->system) {
		traceMark(1, 0, 0, 0, 0, 0);
	}

	pid = profile_spawn(opts, path, envp);
	if (pid < 0) {
		ybx_error("profile", path, errno);
		ret = 1;
		goto done;
	}

	if (!opts->system) {
		if (profile_apply_filter(trace, opts, pid) != 0) {
			procKill((uint32_t)pid, 15);
			ret = 1;
			goto done;
		}
		if (traceCtl(trace, API_TRACE_OP_START, NULL) < 0) {
			ybx_error("profile", "trace start", errno);
			procKill((uint32_t)pid, 15);
			ret = 1;
			goto done;
		}
		traceMark(1, (uint64_t)pid, 0, 0, 0, 0);
	}

	stats.pid = pid;
	stats.wait_pid = profile_wait_child(pid, &status);
	stats.exit_status = status;
	traceMark(2, (uint64_t)pid, (uint64_t)status, 0, 0, 0);
	traceCtl(trace, API_TRACE_OP_STOP, NULL);
	if (profile_read_all(trace, opts, m, &stats, out) != 0) {
		ret = 1;
		goto done;
	}
	profile_print_report(out, opts, m, &stats);

done:
	traceCtl(trace, API_TRACE_OP_STOP, NULL);
	traceClose(trace);
	if (out != stdout && fclose(out) != 0) {
		ybx_error("profile", opts->output, errno);
		ret = 1;
	}
	return (ret);
}

int
main(int argc, char **argv, char **envp)
{
	profile_manifest_t	manifest;
	profile_opts_t		opts;
	int			rc, i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0 ||
		    argv[i][0] != '-' || argv[i][1] == '\0') {
			break;
		}
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			profile_usage();
			return (0);
		}
	}
	if (profile_manifest_load(&manifest) != 0) {
		ybx_error("profile", "trace manifest", errno);
		return (1);
	}
	profile_opts_init(&opts);
	rc = profile_parse_argv(&opts, &manifest, argc, argv);
	if (rc != 0) {
		return (rc < 0 ? 1 : 0);
	}
	if (opts.list_events || opts.list_sources || opts.list_pmu) {
		if (opts.list_events) {
			profile_list_events(stdout, &manifest);
		}
		if (opts.list_sources) {
			profile_list_sources(stdout, &manifest);
		}
		if (opts.list_pmu) {
			profile_list_pmu(stdout, &manifest);
		}
		return (0);
	}
	return (profile_run(&opts, &manifest, envp));
}
