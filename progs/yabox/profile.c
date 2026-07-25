/* !DEFINES!

$define %type profile_opts_t as parsed profile command settings
$define %type profile_manifest_t as trace provider and probe catalog
$define %type profile_stats_t as profile run counters
$define %type api_trace_probe as kernel probe metadata
$define %type api_trace_program as loadable trace program
$define %func profile_usage as procedure with args void
$define %func profile_opts_init as procedure with args profile_opts_t *
$define %func profile_manifest_load as function with args profile_manifest_t *
$define %func profile_parse_argv as function with args options, manifest, argc, argv
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
#define	PROFILE_TOKEN_MAX	96

#define	PROFILE_ACT_COUNT_BASE	0x10000U
#define	PROFILE_ACT_SUM_BASE	0x20000U
#define	PROFILE_ACT_RAW_BASE	0x30000U

typedef struct profile_opts {
	const char	*output;
	int		argc;
	char		**argv;
	uint8_t		probes[API_TRACE_MAX_PROBES];
	uint32_t	raw_limit;
	int		abi;
	int		system;
	int		kernel_stacks;
	int		cpu;
	int		tid;
	int		probe_selected;
	int		show_summary;
	int		show_aggs;
	int		show_raw;
	int		show_stacks;
	int		csv;
	int		quiet;
	int		list_probes;
	int		list_providers;
	int		list_pmu;
} profile_opts_t;

typedef struct profile_manifest {
	struct api_trace_provider	providers[API_TRACE_MAX_PROVIDERS];
	struct api_trace_probe		probes[API_TRACE_MAX_PROBES];
	struct api_trace_pmu_counter	pmu[API_TRACE_MAX_PMU_COUNTERS];
	uint32_t			provider_count;
	uint32_t			probe_count;
	uint32_t			pmu_count;
} profile_manifest_t;

typedef struct profile_stats {
	uint64_t	records_read;
	uint64_t	records_lost;
	uint64_t	raw_printed;
	uint64_t	raw_seen;
	uint64_t	min_tsc;
	uint64_t	max_tsc;
	uint32_t	agg_count;
	int		have_tsc;
	int		pid;
	int		wait_pid;
	int		exit_status;
} profile_stats_t;

static profile_manifest_t		g_profile_manifest;
static struct api_trace_program	g_profile_programs[API_TRACE_MAX_PROGRAMS];
static struct api_trace_record	g_profile_records[PROFILE_READ_BATCH];
static struct api_trace_agg	g_profile_aggs[API_TRACE_MAX_AGGREGATIONS];

static void
profile_usage(void)
{
	fprintf(stderr,
	    "usage: profile [options] program [args...]\n"
	    "       profile --list-probes|--list-providers|--list-pmu\n"
	    "\n"
	    "run mode:\n"
	    "  --native              spawn with native ABI (default)\n"
	    "  --posix               spawn with POSIX ABI\n"
	    "  --abi native|posix    explicit ABI\n"
	    "  --system              keep all process activity while program runs\n"
	    "  --cpu N               predicate on one CPU\n"
	    "  --tid N               predicate on one thread id\n"
	    "  --kernel-stacks       request kernel stack capture permission\n"
	    "\n"
	    "probe selection:\n"
	    "  -p, --probe LIST      enable probe ids or provider:module:function:name\n"
	    "  --no-probe LIST       remove probes from current selection\n"
	    "  --provider LIST       enable all probes from providers\n"
	    "  --no-provider LIST    remove providers from current selection\n"
	    "\n"
	    "output:\n"
	    "  --summary|--no-summary\n"
	    "  --aggs|--no-aggs      print kernel aggregation buckets\n"
	    "  --raw                 add record actions and dump records\n"
	    "  --csv                 raw CSV output\n"
	    "  --stacks              use stack actions for raw records\n"
	    "  --raw-limit N         raw rows to print; 0 means all\n"
	    "  -o, --output FILE     write report to file\n"
	    "  -q, --quiet           suppress header lines\n");
}

static void
profile_opts_init(profile_opts_t *opts)
{
	memset(opts, 0, sizeof(*opts));
	opts->abi = PROFILE_ABI_NATIVE;
	opts->cpu = -1;
	opts->tid = -1;
	opts->raw_limit = PROFILE_RAW_LIMIT;
	opts->show_summary = 1;
	opts->show_aggs = 1;
}

static int
profile_parse_u32_any(const char *text, uint32_t *out)
{
	char		*end;
	unsigned long	value;

	if (text == NULL || text[0] == '\0' || out == NULL) {
		errno = EINVAL;
		return (-1);
	}
	errno = 0;
	value = strtoul(text, &end, 0);
	if (errno != 0 || *end != '\0') {
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

	if (cmd == NULL || cmd[0] == '\0') {
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
		errno = EINVAL;
		return (NULL);
	}
	(*index)++;
	return (argv[*index]);
}

static const struct api_trace_probe *
profile_probe_by_id(const profile_manifest_t *m, uint32_t id)
{
	uint32_t	i;

	for (i = 0; i < m->probe_count; i++) {
		if (m->probes[i].id == id) {
			return (&m->probes[i]);
		}
	}
	return (NULL);
}

static int
profile_part_match(const char *value, const char *part)
{
	if (part == NULL || part[0] == '\0') {
		return (1);
	}
	return (strcmp(value, part) == 0);
}

static int
profile_probe_matches_token(const struct api_trace_probe *probe,
    const char *token)
{
	char		buf[PROFILE_TOKEN_MAX];
	char		*parts[4];
	size_t		len;
	uint32_t	id;
	int		i, part, colon;

	if (profile_parse_u32_any(token, &id) == 0) {
		return (probe->id == id);
	}
	len = strlen(token);
	if (len == 0 || len >= sizeof(buf)) {
		errno = E2BIG;
		return (0);
	}
	memcpy(buf, token, len + 1);
	colon = 0;
	for (i = 0; i < 4; i++) {
		parts[i] = "";
	}
	parts[0] = buf;
	part = 0;
	for (i = 0; buf[i] != '\0'; i++) {
		if (buf[i] != ':') {
			continue;
		}
		colon = 1;
		buf[i] = '\0';
		part++;
		if (part >= 4) {
			return (0);
		}
		parts[part] = &buf[i + 1];
	}
	if (!colon) {
		if (strcmp(probe->provider_name, token) == 0) {
			return (1);
		}
		if (strcmp(probe->module, token) == 0) {
			return (1);
		}
		if (strcmp(probe->function, token) == 0) {
			return (1);
		}
		return (strcmp(probe->name, token) == 0);
	}
	if (!profile_part_match(probe->provider_name, parts[0])) {
		return (0);
	}
	if (!profile_part_match(probe->module, parts[1])) {
		return (0);
	}
	if (!profile_part_match(probe->function, parts[2])) {
		return (0);
	}
	if (!profile_part_match(probe->name, parts[3])) {
		return (0);
	}
	return (1);
}

static int
profile_provider_matches(const struct api_trace_probe *probe,
    const char *token)
{
	uint32_t	id;

	if (profile_parse_u32_any(token, &id) == 0) {
		return (probe->provider == id);
	}
	return (strcmp(probe->provider_name, token) == 0);
}

static int
profile_apply_token(profile_opts_t *opts, const profile_manifest_t *m,
    const char *token, int enable, int provider)
{
	const struct api_trace_probe	*probe;
	uint32_t			i;
	int				matched;

	matched = 0;
	for (i = 0; i < m->probe_count; i++) {
		probe = &m->probes[i];
		if (provider) {
			if (!profile_provider_matches(probe, token)) {
				continue;
			}
		} else if (!profile_probe_matches_token(probe, token)) {
			continue;
		}
		if (probe->id < API_TRACE_MAX_PROBES) {
			opts->probes[probe->id] = enable ? 1 : 0;
			matched = 1;
		}
	}
	if (!matched) {
		errno = ENOENT;
		return (-1);
	}
	opts->probe_selected = 1;
	return (0);
}

static int
profile_apply_list(profile_opts_t *opts, const profile_manifest_t *m,
    const char *list, int enable, int provider)
{
	char	token[PROFILE_TOKEN_MAX];
	size_t	pos;
	uint32_t p;
	int	i;

	if (!enable && !opts->probe_selected) {
		for (p = 0; p < m->probe_count; p++) {
			if (m->probes[p].id < API_TRACE_MAX_PROBES) {
				opts->probes[m->probes[p].id] = 1;
			}
		}
		opts->probe_selected = 1;
	}
	pos = 0;
	for (i = 0;; i++) {
		if (list[i] == ',' || list[i] == '\0') {
			if (pos == 0) {
				errno = EINVAL;
				return (-1);
			}
			token[pos] = '\0';
			if (profile_apply_token(opts, m, token, enable,
			    provider) != 0) {
				return (-1);
			}
			pos = 0;
			if (list[i] == '\0') {
				break;
			}
			continue;
		}
		if (pos + 1 >= sizeof(token)) {
			errno = E2BIG;
			return (-1);
		}
		token[pos++] = list[i];
	}
	return (0);
}

static int
profile_manifest_load(profile_manifest_t *m)
{
	struct api_trace_providers	pvq;
	struct api_trace_probes		pbq;
	struct api_trace_pmu		pmq;

	memset(m, 0, sizeof(*m));
	memset(&pvq, 0, sizeof(pvq));
	pvq.providers = m->providers;
	pvq.max_providers = API_TRACE_MAX_PROVIDERS;
	if (traceInfo(API_TRACE_INFO_PROVIDERS, &pvq) < 0) {
		return (-1);
	}
	m->provider_count = pvq.count;
	if (m->provider_count > API_TRACE_MAX_PROVIDERS) {
		m->provider_count = API_TRACE_MAX_PROVIDERS;
	}
	memset(&pbq, 0, sizeof(pbq));
	pbq.probes = m->probes;
	pbq.max_probes = API_TRACE_MAX_PROBES;
	if (traceInfo(API_TRACE_INFO_PROBES, &pbq) < 0) {
		return (-1);
	}
	m->probe_count = pbq.count;
	if (m->probe_count > API_TRACE_MAX_PROBES) {
		m->probe_count = API_TRACE_MAX_PROBES;
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
		if (strcmp(argv[i], "--help") == 0) {
			profile_usage();
			return (1);
		} else if (strcmp(argv[i], "--list-probes") == 0) {
			opts->list_probes = 1;
		} else if (strcmp(argv[i], "--list-providers") == 0) {
			opts->list_providers = 1;
		} else if (strcmp(argv[i], "--list-pmu") == 0) {
			opts->list_pmu = 1;
		} else if (strcmp(argv[i], "--native") == 0) {
			opts->abi = PROFILE_ABI_NATIVE;
		} else if (strcmp(argv[i], "--posix") == 0) {
			opts->abi = PROFILE_ABI_POSIX;
		} else if (strcmp(argv[i], "--abi") == 0) {
			value = profile_need_arg(argc, argv, &i, "--abi");
			if (value == NULL || profile_parse_abi(opts, value) != 0) {
				return (-1);
			}
		} else if (strncmp(argv[i], "--abi=", 6) == 0) {
			if (profile_parse_abi(opts, argv[i] + 6) != 0) {
				return (-1);
			}
		} else if (strcmp(argv[i], "--system") == 0) {
			opts->system = 1;
		} else if (strcmp(argv[i], "--kernel-stacks") == 0) {
			opts->kernel_stacks = 1;
		} else if (strcmp(argv[i], "--cpu") == 0) {
			value = profile_need_arg(argc, argv, &i, "--cpu");
			if (value == NULL ||
			    profile_parse_number_option(value,
			    &opts->cpu) != 0) {
				return (-1);
			}
		} else if (strncmp(argv[i], "--cpu=", 6) == 0) {
			if (profile_parse_number_option(argv[i] + 6,
			    &opts->cpu) != 0) {
				return (-1);
			}
		} else if (strcmp(argv[i], "--tid") == 0) {
			value = profile_need_arg(argc, argv, &i, "--tid");
			if (value == NULL ||
			    profile_parse_number_option(value,
			    &opts->tid) != 0) {
				return (-1);
			}
		} else if (strcmp(argv[i], "-p") == 0 ||
		    strcmp(argv[i], "--probe") == 0) {
			value = profile_need_arg(argc, argv, &i, argv[i]);
			if (value == NULL ||
			    profile_apply_list(opts, m, value, 1, 0) != 0) {
				return (-1);
			}
		} else if (strncmp(argv[i], "--probe=", 8) == 0) {
			if (profile_apply_list(opts, m, argv[i] + 8,
			    1, 0) != 0) {
				return (-1);
			}
		} else if (strcmp(argv[i], "--no-probe") == 0) {
			value = profile_need_arg(argc, argv, &i, "--no-probe");
			if (value == NULL ||
			    profile_apply_list(opts, m, value, 0, 0) != 0) {
				return (-1);
			}
		} else if (strcmp(argv[i], "--provider") == 0) {
			value = profile_need_arg(argc, argv, &i, "--provider");
			if (value == NULL ||
			    profile_apply_list(opts, m, value, 1, 1) != 0) {
				return (-1);
			}
		} else if (strncmp(argv[i], "--provider=", 11) == 0) {
			if (profile_apply_list(opts, m, argv[i] + 11,
			    1, 1) != 0) {
				return (-1);
			}
		} else if (strcmp(argv[i], "--no-provider") == 0) {
			value = profile_need_arg(argc, argv, &i,
			    "--no-provider");
			if (value == NULL ||
			    profile_apply_list(opts, m, value, 0, 1) != 0) {
				return (-1);
			}
		} else if (strcmp(argv[i], "--summary") == 0) {
			opts->show_summary = 1;
		} else if (strcmp(argv[i], "--no-summary") == 0) {
			opts->show_summary = 0;
		} else if (strcmp(argv[i], "--aggs") == 0) {
			opts->show_aggs = 1;
		} else if (strcmp(argv[i], "--no-aggs") == 0) {
			opts->show_aggs = 0;
		} else if (strcmp(argv[i], "--raw") == 0) {
			opts->show_raw = 1;
		} else if (strcmp(argv[i], "--csv") == 0) {
			opts->csv = 1;
			opts->show_raw = 1;
			opts->show_summary = 0;
			opts->show_aggs = 0;
		} else if (strcmp(argv[i], "--stacks") == 0) {
			opts->show_stacks = 1;
			opts->show_raw = 1;
		} else if (strcmp(argv[i], "--raw-limit") == 0) {
			value = profile_need_arg(argc, argv, &i, "--raw-limit");
			if (value == NULL ||
			    profile_parse_u32_any(value,
			    &opts->raw_limit) != 0) {
				return (-1);
			}
		} else if (strncmp(argv[i], "--raw-limit=", 12) == 0) {
			if (profile_parse_u32_any(argv[i] + 12,
			    &opts->raw_limit) != 0) {
				return (-1);
			}
		} else if (strcmp(argv[i], "-o") == 0 ||
		    strcmp(argv[i], "--output") == 0) {
			value = profile_need_arg(argc, argv, &i, argv[i]);
			if (value == NULL) {
				return (-1);
			}
			opts->output = value;
		} else if (strcmp(argv[i], "-q") == 0 ||
		    strcmp(argv[i], "--quiet") == 0) {
			opts->quiet = 1;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "profile: unknown option %s\n", argv[i]);
			errno = EINVAL;
			return (-1);
		} else {
			opts->argc = argc - i;
			opts->argv = &argv[i];
			break;
		}
	}
	if (opts->list_probes || opts->list_providers || opts->list_pmu) {
		return (0);
	}
	if (opts->argv == NULL || opts->argc == 0) {
		profile_usage();
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

static int
profile_arg_summable(uint32_t type)
{
	switch (type) {
	case API_TRACE_ARG_U64:
	case API_TRACE_ARG_CYCLES:
	case API_TRACE_ARG_BYTES:
		return (1);
	default:
		return (0);
	}
}

static int
profile_program_add_pred(struct api_trace_program *program,
    uint32_t field, uint32_t op, uint64_t value)
{
	struct api_trace_predicate	*pred;

	if (program->predicate_count >= API_TRACE_MAX_PREDICATES) {
		errno = E2BIG;
		return (-1);
	}
	pred = &program->predicates[program->predicate_count++];
	memset(pred, 0, sizeof(*pred));
	pred->field = field;
	pred->op = op;
	pred->value = value;
	return (0);
}

static int
profile_program_add_action(struct api_trace_program *program,
    uint32_t kind, uint32_t arg, uint32_t key, uint32_t id)
{
	struct api_trace_action	*action;

	if (program->action_count >= API_TRACE_MAX_ACTIONS) {
		errno = E2BIG;
		return (-1);
	}
	action = &program->actions[program->action_count++];
	memset(action, 0, sizeof(*action));
	action->kind = kind;
	action->arg = arg;
	action->key = key;
	action->id = id;
	return (0);
}

static int
profile_probe_selected(const profile_opts_t *opts, uint32_t probe_id)
{
	if (probe_id >= API_TRACE_MAX_PROBES) {
		return (0);
	}
	if (!opts->probe_selected) {
		return (1);
	}
	return (opts->probes[probe_id] != 0);
}

static int
profile_build_programs(const profile_opts_t *opts,
    const profile_manifest_t *m, int pid, uint32_t *out_count)
{
	const struct api_trace_probe	*probe;
	struct api_trace_program		*program;
	uint32_t			count, i, a;

	count = 0;
	memset(g_profile_programs, 0, sizeof(g_profile_programs));
	for (i = 0; i < m->probe_count; i++) {
		probe = &m->probes[i];
		if (!profile_probe_selected(opts, probe->id)) {
			continue;
		}
		if (count >= API_TRACE_MAX_PROGRAMS) {
			errno = E2BIG;
			return (-1);
		}
		program = &g_profile_programs[count];
		memset(program, 0, sizeof(*program));
		program->probe_id = probe->id;
		if (pid >= 0) {
			if (profile_program_add_pred(program,
			    API_TRACE_FIELD_PID, API_TRACE_PRED_EQ,
			    (uint64_t)pid) != 0) {
				return (-1);
			}
		}
		if (opts->cpu >= 0) {
			if (profile_program_add_pred(program,
			    API_TRACE_FIELD_CPU, API_TRACE_PRED_EQ,
			    (uint64_t)opts->cpu) != 0) {
				return (-1);
			}
		}
		if (opts->tid >= 0) {
			if (profile_program_add_pred(program,
			    API_TRACE_FIELD_TID, API_TRACE_PRED_EQ,
			    (uint64_t)opts->tid) != 0) {
				return (-1);
			}
		}
		if (profile_program_add_action(program, API_TRACE_ACT_COUNT,
		    0, API_TRACE_FIELD_NONE,
		    PROFILE_ACT_COUNT_BASE + probe->id) != 0) {
			return (-1);
		}
		for (a = 0; a < probe->argc && a < API_TRACE_MAX_ARGS; a++) {
			if (!profile_arg_summable(probe->args[a].type)) {
				continue;
			}
			if (profile_program_add_action(program,
			    API_TRACE_ACT_SUM, a, API_TRACE_FIELD_NONE,
			    PROFILE_ACT_SUM_BASE +
			    probe->id * API_TRACE_MAX_ARGS + a) != 0) {
				return (-1);
			}
		}
		if (opts->show_raw) {
			if (profile_program_add_action(program,
			    opts->show_stacks ? API_TRACE_ACT_STACK :
			    API_TRACE_ACT_RECORD, 0, API_TRACE_FIELD_NONE,
			    PROFILE_ACT_RAW_BASE + probe->id) != 0) {
				return (-1);
			}
		}
		count++;
	}
	*out_count = count;
	return (0);
}

static int
profile_load_programs(int trace, uint32_t count)
{
	struct api_trace_load	load;

	memset(&load, 0, sizeof(load));
	load.programs = g_profile_programs;
	load.program_count = count;
	if (traceCtl(trace, API_TRACE_OP_LOAD, &load) < 0) {
		return (-1);
	}
	return (0);
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

static void
profile_print_probe_tuple(FILE *out, const struct api_trace_probe *probe)
{
	fprintf(out, "%s:%s:%s:%s", probe->provider_name, probe->module,
	    probe->function, probe->name);
}

static void
profile_list_providers(FILE *out, const profile_manifest_t *m)
{
	uint32_t	i;

	fprintf(out, "id\tenabled\tprobes\tname\n");
	for (i = 0; i < m->provider_count; i++) {
		fprintf(out, "%u\t%s\t%u\t%s\n", m->providers[i].id,
		    m->providers[i].enabled ? "yes" : "no",
		    m->providers[i].probe_count, m->providers[i].name);
	}
}

static void
profile_list_probes(FILE *out, const profile_manifest_t *m)
{
	const struct api_trace_probe	*probe;
	const struct api_trace_arg	*arg;
	uint32_t			i, a;

	fprintf(out, "id\tenabled\tprobe\targs\n");
	for (i = 0; i < m->probe_count; i++) {
		probe = &m->probes[i];
		fprintf(out, "%u\t%s\t", probe->id,
		    probe->enabled ? "yes" : "no");
		profile_print_probe_tuple(out, probe);
		fprintf(out, "\t");
		for (a = 0; a < probe->argc && a < API_TRACE_MAX_ARGS; a++) {
			arg = &probe->args[a];
			if (a != 0) {
				fprintf(out, ",");
			}
			fprintf(out, "%s", arg->name);
		}
		fprintf(out, "\n");
	}
}

static void
profile_list_pmu(FILE *out, const profile_manifest_t *m)
{
	uint32_t	i;

	fprintf(out, "id\tenabled\tname\n");
	for (i = 0; i < m->pmu_count; i++) {
		fprintf(out, "%u\t%s\t%s\n", m->pmu[i].id,
		    m->pmu[i].enabled ? "yes" : "no", m->pmu[i].name);
	}
}

static void
profile_note_record(profile_stats_t *stats,
    const struct api_trace_record *rec)
{
	stats->raw_seen++;
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
}

static void
profile_print_raw(FILE *out, const profile_opts_t *opts,
    const profile_manifest_t *m, const struct api_trace_record *rec)
{
	const struct api_trace_probe	*probe;
	uint32_t			i;

	probe = profile_probe_by_id(m, (uint32_t)rec->probe_id);
	if (opts->csv) {
		fprintf(out,
		    "%llu,%llu,%llu,%u,%llu,%llu,%llu,%s,"
		    "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,"
		    "%llu,%llu,%llu\n",
		    (unsigned long long)rec->seq,
		    (unsigned long long)rec->tsc,
		    (unsigned long long)rec->ticks,
		    rec->cpu,
		    (unsigned long long)rec->pid,
		    (unsigned long long)rec->tid,
		    (unsigned long long)rec->probe_id,
		    probe ? probe->name : "unknown",
		    (unsigned long long)rec->args[0],
		    (unsigned long long)rec->args[1],
		    (unsigned long long)rec->args[2],
		    (unsigned long long)rec->args[3],
		    (unsigned long long)rec->args[4],
		    (unsigned long long)rec->args[5],
		    (unsigned long long)rec->args[6],
		    (unsigned long long)rec->args[7],
		    (unsigned long long)rec->ip,
		    (unsigned long long)rec->sp,
		    (unsigned long long)rec->bp);
		return;
	}
	fprintf(out, "%llu cpu=%u pid=%llu tid=%llu probe=%llu ",
	    (unsigned long long)rec->seq, rec->cpu,
	    (unsigned long long)rec->pid,
	    (unsigned long long)rec->tid,
	    (unsigned long long)rec->probe_id);
	if (probe != NULL) {
		profile_print_probe_tuple(out, probe);
	} else {
		fprintf(out, "unknown");
	}
	fprintf(out, " tsc=%llu args=", (unsigned long long)rec->tsc);
	for (i = 0; i < rec->argc && i < API_TRACE_MAX_ARGS; i++) {
		if (i != 0) {
			fprintf(out, ",");
		}
		fprintf(out, "%llu", (unsigned long long)rec->args[i]);
	}
	fprintf(out, " ip=0x%llx\n", (unsigned long long)rec->ip);
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

static int
profile_read_records(int trace, const profile_opts_t *opts,
    const profile_manifest_t *m, profile_stats_t *stats, FILE *out)
{
	struct api_trace_read	args;
	ssize_t			n;
	int			i;

	if (opts->csv && opts->show_raw) {
		fprintf(out,
		    "seq,tsc,ticks,cpu,pid,tid,probe,probe_name,"
		    "arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,ip,sp,bp\n");
	}
	for (;;) {
		memset(&args, 0, sizeof(args));
		args.records = g_profile_records;
		args.max_records = PROFILE_READ_BATCH;
		n = traceRead(trace, &args);
		if (n < 0) {
			ybx_error("profile", "traceRead", errno);
			return (-1);
		}
		stats->records_read = args.records_total;
		stats->records_lost = args.records_lost;
		if (n == 0) {
			break;
		}
		for (i = 0; i < n; i++) {
			profile_note_record(stats, &g_profile_records[i]);
			if (opts->show_raw &&
			    (opts->raw_limit == 0 ||
			    stats->raw_printed < opts->raw_limit)) {
				profile_print_raw(out, opts, m,
				    &g_profile_records[i]);
				stats->raw_printed++;
			}
		}
	}
	return (0);
}

static int
profile_read_aggs(int trace, profile_stats_t *stats)
{
	struct api_trace_aggs	args;
	ssize_t			n;

	memset(&args, 0, sizeof(args));
	args.trace = trace;
	args.aggs = g_profile_aggs;
	args.max_aggs = API_TRACE_MAX_AGGREGATIONS;
	n = traceInfo(API_TRACE_INFO_AGGS, &args);
	if (n < 0) {
		ybx_error("profile", "trace aggs", errno);
		return (-1);
	}
	stats->agg_count = args.count;
	return (0);
}

static const char *
profile_action_name(uint32_t kind)
{
	switch (kind) {
	case API_TRACE_ACT_COUNT:
		return ("count");
	case API_TRACE_ACT_SUM:
		return ("sum");
	case API_TRACE_ACT_MIN:
		return ("min");
	case API_TRACE_ACT_MAX:
		return ("max");
	case API_TRACE_ACT_QUANTIZE:
		return ("quantize");
	case API_TRACE_ACT_LQUANTIZE:
		return ("lquantize");
	default:
		return ("action");
	}
}

static const char *
profile_agg_arg_name(const profile_manifest_t *m,
    const struct api_trace_agg *agg)
{
	const struct api_trace_probe	*probe;

	probe = profile_probe_by_id(m, agg->probe_id);
	if (probe == NULL || agg->arg >= probe->argc ||
	    agg->arg >= API_TRACE_MAX_ARGS) {
		return ("");
	}
	return (probe->args[agg->arg].name);
}

static void
profile_print_summary(FILE *out, const profile_opts_t *opts,
    const profile_stats_t *stats)
{
	if (!opts->show_summary) {
		return;
	}
	fprintf(out, "summary:\n");
	fprintf(out, "  pid              %d\n", stats->pid);
	fprintf(out, "  wait_pid         %d\n", stats->wait_pid);
	fprintf(out, "  exit_status      %d\n", stats->exit_status);
	fprintf(out, "  records_read     %llu\n",
	    (unsigned long long)stats->records_read);
	fprintf(out, "  records_lost     %llu\n",
	    (unsigned long long)stats->records_lost);
	fprintf(out, "  aggregations     %u\n", stats->agg_count);
	if (stats->have_tsc) {
		fprintf(out, "  tsc_span         %llu\n",
		    (unsigned long long)(stats->max_tsc - stats->min_tsc));
	}
}

static void
profile_print_aggs(FILE *out, const profile_opts_t *opts,
    const profile_manifest_t *m, const profile_stats_t *stats)
{
	const struct api_trace_probe	*probe;
	const struct api_trace_agg	*agg;
	uint32_t			i, k;
	int				have_key;

	if (!opts->show_aggs) {
		return;
	}
	fprintf(out, "aggregations:\n");
	for (i = 0; i < stats->agg_count &&
	    i < API_TRACE_MAX_AGGREGATIONS; i++) {
		agg = &g_profile_aggs[i];
		probe = profile_probe_by_id(m, agg->probe_id);
		fprintf(out, "  ");
		if (probe != NULL) {
			profile_print_probe_tuple(out, probe);
		} else {
			fprintf(out, "probe-%u", agg->probe_id);
		}
		fprintf(out, " %-10s", profile_action_name(agg->kind));
		if (agg->kind != API_TRACE_ACT_COUNT) {
			fprintf(out, " %-16s", profile_agg_arg_name(m, agg));
		} else {
			fprintf(out, " %-16s", "");
		}
		have_key = 0;
		for (k = 0; k < 4; k++) {
			if (agg->key[k] != 0) {
				have_key = 1;
			}
		}
		if (have_key) {
			fprintf(out, " key=[");
			for (k = 0; k < 4; k++) {
				if (k != 0) {
					fprintf(out, ",");
				}
				fprintf(out, "%llu",
				    (unsigned long long)agg->key[k]);
			}
			fprintf(out, "]");
		}
		fprintf(out, " value=%llu count=%llu\n",
		    (unsigned long long)agg->value,
		    (unsigned long long)agg->count);
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
	if (opts->csv) {
		return;
	}
	profile_print_summary(out, opts, stats);
	profile_print_aggs(out, opts, m, stats);
}

static int
profile_run(profile_opts_t *opts, const profile_manifest_t *m, char **envp)
{
	profile_stats_t	stats;
	char		path[YBX_PATH_MAX];
	FILE		*out;
	uint32_t	flags, clear_flags, program_count;
	int		trace, pid, status, ret;

	out = stdout;
	if (opts->output != NULL) {
		out = fopen(opts->output, "w");
		if (out == NULL) {
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
	flags = API_TRACE_OPEN_PRIVILEGED;
	if (opts->kernel_stacks || opts->show_stacks) {
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
	ret = 0;
	pid = -1;
	status = 0;
	memset(&stats, 0, sizeof(stats));
	clear_flags = API_TRACE_CLEAR_ALL;
	if (traceCtl(trace, API_TRACE_OP_CLEAR, &clear_flags) < 0) {
		ybx_error("profile", "trace clear", errno);
		ret = 1;
		goto done;
	}
	if (opts->system) {
		if (profile_build_programs(opts, m, -1,
		    &program_count) != 0 ||
		    profile_load_programs(trace, program_count) != 0) {
			ybx_error("profile", "trace load", errno);
			ret = 1;
			goto done;
		}
		if (traceCtl(trace, API_TRACE_OP_START, NULL) < 0) {
			ybx_error("profile", "trace start", errno);
			ret = 1;
			goto done;
		}
		traceMark(1, 0, 0, 0, 0, 0);
	}
	pid = profile_spawn(opts, path, envp);
	if (pid < 0) {
		ybx_error("profile", path, errno);
		ret = 1;
		goto done;
	}
	if (!opts->system) {
		clear_flags = API_TRACE_CLEAR_ALL;
		traceCtl(trace, API_TRACE_OP_CLEAR, &clear_flags);
		if (profile_build_programs(opts, m, pid,
		    &program_count) != 0 ||
		    profile_load_programs(trace, program_count) != 0) {
			ybx_error("profile", "trace load", errno);
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
	if (profile_read_records(trace, opts, m, &stats, out) != 0) {
		ret = 1;
		goto done;
	}
	if (profile_read_aggs(trace, &stats) != 0) {
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
	profile_opts_t	opts;
	int		ret;

	profile_opts_init(&opts);
	if (profile_manifest_load(&g_profile_manifest) != 0) {
		ybx_error("profile", "trace manifest", errno);
		return (1);
	}
	ret = profile_parse_argv(&opts, &g_profile_manifest, argc, argv);
	if (ret != 0) {
		return (ret < 0 ? 1 : 0);
	}
	if (opts.list_providers) {
		profile_list_providers(stdout, &g_profile_manifest);
	}
	if (opts.list_probes) {
		profile_list_probes(stdout, &g_profile_manifest);
	}
	if (opts.list_pmu) {
		profile_list_pmu(stdout, &g_profile_manifest);
	}
	if (opts.list_providers || opts.list_probes || opts.list_pmu) {
		return (0);
	}
	return (profile_run(&opts, &g_profile_manifest, envp));
}
