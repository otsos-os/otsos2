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

/* !DEFINES!

$define %type api_dirent as native directory entry
$define %type api_proc_info as native process entry
$define %type api_meminfo as native memory data
$define %type api_kmeminfo as native kernel memory data
$define %type api_cpuinfo as native CPU data
$define %type api_timeinfo as native time data

$define %func main as start with args int, char **, char **
$define %func read_line as function with args char *, int
$define %func parse_line as function with args char *, char **, int
$define %func exec_line as function with args int, char **

*/

/* !SPACE!

$space %export main
$space %internal print, println, printc, print_int, print_u64
$space %internal err_str, print_err_tail, print_err_code
$space %internal read_line, parse_line
$space %internal resolve_path, run_external, exec_builtin, exec_line

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE	512
#define MAX_ARGS	64
#define MAX_PATH	256
#define MAX_DIRENT	128
#define MAX_PROCS	64
#define MAX_CPUS	32
#define MAX_CPU_PIDS	64

static char	**g_envp;
static int	g_kusr_authed;

static void
print(const char *s)
{
	if (s) {
		termPrint(s);
	}
}

static void
println(const char *s)
{
	print(s);
	print("\n");
}

static void
printc(char c)
{
	termWrite(&c, 1);
}

static void
print_int(int v)
{
	char	buf[32];
	snprintf(buf, sizeof(buf), "%d", v);
	print(buf);
}

static void
print_u64(uint64_t v)
{
	char	buf[32];
	snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);
	print(buf);
}

static void
print_hex_u64(uint64_t v)
{
	char	buf[32];
	snprintf(buf, sizeof(buf), "%lx", (unsigned long)v);
	print(buf);
}

static const char *
err_str(int code)
{
	if (code < 0) {
		code = -code;
	}
	switch (code) {
	case 0:
		return ("ok");
	case EPERM:
		return ("operation not permitted");
	case ENOENT:
		return ("no such file or directory");
	case ESRCH:
		return ("no such process");
	case EINTR:
		return ("interrupted");
	case EIO:
		return ("i/o error");
	case 6:
		return ("no device address");
	case E2BIG:
		return ("argument/value too large");
	case ENOEXEC:
		return ("invalid executable image");
	case EBADF:
		return ("bad file handle");
	case ECHILD:
		return ("no child process");
	case EAGAIN:
		return ("resource busy, try again");
	case ENOMEM:
		return ("out of memory");
	case EACCES:
		return ("permission denied");
	case EFAULT:
		return ("bad address");
	case EINVAL:
	case 22:
		return ("invalid argument");
	case EBUSY:
		return ("device or resource busy");
	case EEXIST:
		return ("file exists");
	case EXDEV:
		return ("cross-device link");
	case ENODEV:
		return ("no such device");
	case ENOTDIR:
		return ("not a directory");
	case EISDIR:
		return ("is a directory");
	case 23:
		return ("kernel object table full");
	case EMFILE:
		return ("process handle table full");
	case ENOTTY:
		return ("not a terminal");
	case 26:
		return ("out of memory");
	case EFBIG:
		return ("file too large");
	case ENOSPC:
		return ("no space left on device");
	case ESPIPE:
		return ("not seekable");
	case EROFS:
		return ("read-only filesystem");
	case EPIPE:
		return ("broken pipe");
	case ENOSYS:
		return ("no such syscall");
	case ENOTSUP:
		return ("operation not supported");
	default:
		return ("unknown error");
	}
}

static void
print_err_tail(int code)
{
	print(err_str(code));
	print(" (code ");
	if (code > 0) {
		print_int(-code);
	} else {
		print_int(code);
	}
	print(")\n");
}

static void
print_err_code(const char *prefix, int code)
{
	print(prefix);
	print(": ");
	print_err_tail(code);
}

static int
read_line(char *buf, int max)
{
	ssize_t	n;
	int	code;
	int	pos;
	char	c;

	pos = 0;
	for (;;) {
		n = termReadFlags(&c, 1, TERM_READ_IGNORE_SIGINT);
		if (n == 0) {
			return (-1);
		}
		if (n < 0) {
			code = errno;
			if (code == EAGAIN || code == EINTR) {
				continue;
			}
			return (-1);
		}
		if (c == 0x04) {
			if (pos == 0) {
				return (-1);
			}
			continue;
		}
		if (c == '\r' || c == '\n') {
			printc('\n');
			buf[pos] = '\0';
			return (pos);
		}
		if (c == '\b' || c == 0x7f) {
			if (pos > 0) {
				pos--;
				print("\b \b");
			}
			continue;
		}
		if (c == 0x03) {
			print("^C\n");
			buf[0] = '\0';
			return (0);
		}
		if (c >= 32 && c < 127 && pos < max - 1) {
			buf[pos++] = c;
			printc(c);
		}
	}
}

static int
parse_line(char *line, char **argv, int max_args)
{
	char	*p;
	char	quote;
	int	argc;

	argc = 0;
	p = line;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	while (*p && argc < max_args - 1) {
		if (*p == '"' || *p == '\'') {
			quote = *p++;
			argv[argc++] = p;
			while (*p && *p != quote) {
				p++;
			}
			if (*p == quote) {
				*p++ = '\0';
			}
		} else {
			argv[argc++] = p;
			while (*p && *p != ' ' && *p != '\t') {
				p++;
			}
			if (*p) {
				*p++ = '\0';
			}
		}
		while (*p == ' ' || *p == '\t') {
			p++;
		}
	}
	argv[argc] = NULL;
	return (argc);
}

static int
file_exists(const char *path)
{
	int	fd;
	int	code;

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
resolve_path(const char *cmd, char *out, size_t out_sz)
{
	const char	*prefix;
	size_t		plen, clen;
	int		exists;

	if (cmd[0] == '/' || cmd[0] == '.') {
		clen = strlen(cmd);
		if (clen >= out_sz) {
			errno = E2BIG;
			return (-1);
		}
		memcpy(out, cmd, clen + 1);
		exists = file_exists(out);
		if (exists == 1) {
			return (0);
		}
		if (exists == 0) {
			errno = ENOENT;
		}
		return (-1);
	}

	prefix = "/bin/";
	plen = strlen(prefix);
	clen = strlen(cmd);
	if (plen + clen >= out_sz) {
		errno = E2BIG;
		return (-1);
	}
	memcpy(out, prefix, plen);
	memcpy(out + plen, cmd, clen + 1);
	exists = file_exists(out);
	if (exists == 1) {
		return (0);
	}
	if (exists == 0) {
		errno = ENOENT;
	}
	return (-1);
}

static int
run_external(const char *path, char **argv, char **envp)
{
	int	status;
	int	handle;
	int	pid;
	int	code;

	pid = procSpawn(path, argv, envp);
	if (pid < 0) {
		code = errno;
		print("sh: ");
		print(path);
		print(": ");
		print_err_tail(code);
		return (-1);
	}

	handle = procOpen((uint32_t)pid);
	if (handle < 0) {
		code = errno;
		if (code == ESRCH) {
			return (0);
		}
		print_err_code("sh: procOpen", code);
		return (-1);
	}

	status = 0;
	if (procWaitHandle(handle, &status) < 0) {
		code = errno;
		print_err_code("sh: wait", code);
		procClose(handle);
		return (-1);
	}
	procClose(handle);
	return (0);
}

static void
cmd_drm_list(void)
{
	struct api_drm_driver_entry	entries[8];
	uint32_t			count;
	uint32_t			i;
	int				ret;
	int				code;

	count = 0;
	ret = drmDriverList(entries, 8, &count);
	if (ret < 0) {
		code = errno;
		print_err_code("drm_list", code);
		return;
	}

	println("ID  NAME                 STATUS");
	for (i = 0; i < count; i++) {
		print_int((int)entries[i].id);
		print("  ");
		print(entries[i].name);
		if (entries[i].active) {
			print("  [active]");
		}
		printc('\n');
	}
	if (count == 0) {
		println("(no drivers registered)");
	}
}

static int
parse_nonneg(const char *s)
{
	const char	*p;
	unsigned long	value;

	if (!s || *s == '\0') {
		return (-1);
	}
	for (p = s; *p; p++) {
		if (*p < '0' || *p > '9') {
			return (-1);
		}
	}
	value = strtoul(s, NULL, 10);
	if (value > UINT32_MAX) {
		return (-1);
	}
	return ((int)value);
}

static void
cmd_drm_switch(int argc, char **argv)
{
	int	id;
	int	code;

	if (argc < 2) {
		println("drm_switch: usage: drm_switch <id>");
		println("  use drm_list to see available drivers");
		return;
	}

	id = parse_nonneg(argv[1]);
	if (id < 0) {
		println("drm_switch: invalid id");
		return;
	}

	if (drmDriverSwitch((uint32_t)id) < 0) {
		code = errno;
		if (code == EPERM) {
			println("drm_switch: permission denied (need kusr)");
		} else {
			print_err_code("drm_switch", code);
		}
		return;
	}
	println("drm_switch: ok");
}

static void
cmd_help(void)
{
	println("otsos2 sh - built-in commands:");
	println("  echo [text...]     print text");
	println("  pwd                print working directory");
	println("  cd <path>          change directory");
	println("  ls [path]          list directory");
	println("  ps                 list processes");
	println("  cpus               list CPUs and running PIDs");
	println("  time               show time info");
	println("  mem                show memory info");
	println("  cat <file>         print file contents");
	println("  clear              clear screen");
	println("  color <hex>        set text color (e.g. FF0000)");
	println("  kusr               authenticate as kernel user");
	println("  drm_list           list DRM drivers");
	println("  drm_switch <id>    switch DRM driver (kusr only)");
	println("  env                print environment");
	println("  exit               exit shell");
	println("  help               this help");
	println("External commands: /bin/<name> or absolute paths");
}

static void
cmd_echo(int argc, char **argv)
{
	int	i;

	for (i = 1; i < argc; i++) {
		print(argv[i]);
		if (i + 1 < argc) {
			printc(' ');
		}
	}
	printc('\n');
}

static void
cmd_pwd(void)
{
	char	buf[MAX_PATH];
	int	code;

	if (fsGetcwd(buf, sizeof(buf)) < 0) {
		code = errno;
		print_err_code("pwd", code);
	} else {
		println(buf);
	}
}

static void
cmd_cd(int argc, char **argv)
{
	const char	*path;
	int		code;

	path = argc < 2 ? "/" : argv[1];
	if (fsChdir(path) < 0) {
		code = errno;
		print("cd: ");
		print(path);
		print(": ");
		print(err_str(code));
		print(" (code ");
		print_int(-code);
		print(")\n");
	}
}

static void
cmd_ls(int argc, char **argv)
{
	struct api_dirent	entries[MAX_DIRENT];
	const char		*path;
	int			n, i, code;

	path = argc > 1 ? argv[1] : "";
	n = fsListdir(path, entries, MAX_DIRENT);
	if (n < 0) {
		code = errno;
		print("ls: ");
		if (path[0]) {
			print(path);
			print(": ");
		}
		print(err_str(code));
		print(" (code ");
		print_int(-code);
		print(")\n");
		return;
	}
	for (i = 0; i < n; i++) {
		print(entries[i].name);
		if (entries[i].type == API_FS_TYPE_DIR) {
			printc('/');
		}
		print("  ");
	}
	printc('\n');
}

static void
print_padded(int v, int width)
{
	char	tmp[12];
	int	t;

	t = 0;
	if (v == 0) {
		tmp[t++] = '0';
	} else {
		while (v > 0) {
			tmp[t++] = (char)('0' + (v % 10));
			v /= 10;
		}
	}
	while (t < width) {
		printc('0');
		width--;
	}
	while (t > 0) {
		printc(tmp[--t]);
	}
}

static int
is_leap(int y)
{
	return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

static uint32_t
days_in_month_int(int y, int m)
{
	static const int	days[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	if (m == 2 && is_leap(y)) {
		return (29);
	}
	return ((uint32_t)days[m - 1]);
}

static void
print_datetime(uint64_t sec)
{
	uint64_t	days;
	int		sec_of_day, rem;
	int		hour, min, s;
	int		year, month, day;
	int		ydays, mdays;

	days = sec / 86400UL;
	sec_of_day = (int)(sec % 86400UL);
	hour = sec_of_day / 3600;
	rem = sec_of_day % 3600;
	min = rem / 60;
	s = rem % 60;

	year = 1970;
	for (;;) {
		ydays = is_leap(year) ? 366 : 365;
		if ((int)days < ydays) {
			break;
		}
		days -= (uint64_t)ydays;
		year++;
	}

	month = 1;
	for (;;) {
		mdays = (int)days_in_month_int(year, month);
		if ((int)days < mdays) {
			break;
		}
		days -= (uint64_t)mdays;
		month++;
	}
	day = (int)days + 1;

	print_padded(year, 4);
	printc('-');
	print_padded(month, 2);
	printc('-');
	print_padded(day, 2);
	printc(' ');
	print_padded(hour, 2);
	printc(':');
	print_padded(min, 2);
	printc(':');
	print_padded(s, 2);
}

static void
print_duration(uint64_t sec)
{
	int	d, h, m, s, rem;

	d = (int)(sec / 86400UL);
	rem = (int)(sec % 86400UL);
	h = rem / 3600;
	rem %= 3600;
	m = rem / 60;
	s = rem % 60;

	if (d > 0) {
		print_int(d);
		print("d ");
	}
	print_padded(h, 2);
	printc(':');
	print_padded(m, 2);
	printc(':');
	print_padded(s, 2);
}

static void
cmd_mem(void)
{
	struct api_meminfo	mi;
	struct api_kmeminfo	ki;
	int			code;

	if (sysMemInfo(&mi) < 0) {
		code = errno;
		print_err_code("mem", code);
		return;
	}

	println("=== Memory Info ===");
	print("RAM total      : ");
	print_u64(mi.ram_total_kb);
	println(" KB");
	print("RAM free       : ");
	print_u64(mi.ram_free_kb);
	println(" KB");
	println("");
	print("Pages total    : ");
	print_u64(mi.pages_total);
	printc('\n');
	print("Pages free     : ");
	print_u64(mi.pages_free);
	printc('\n');
	print("Pages active   : ");
	print_u64(mi.pages_active);
	printc('\n');
	print("Pages inactive : ");
	print_u64(mi.pages_inactive);
	printc('\n');
	print("Pages cache    : ");
	print_u64(mi.pages_cache);
	printc('\n');
	print("Pages wired    : ");
	print_u64(mi.pages_wired);
	printc('\n');
	println("");
	print("User mmap base : 0x");
	print_hex_u64(mi.mmap_base);
	printc('\n');
	print("User mmap limit: 0x");
	print_hex_u64(mi.mmap_limit);
	printc('\n');
	print("User heap size : ");
	print_u64(mi.user_heap_size_kb / 1024);
	println(" MB");

	if (!g_kusr_authed) {
		return;
	}
	if (sysKmemInfo(&ki) < 0) {
		code = errno;
		println("");
		if (code == EPERM) {
			println("kmem info: permission denied");
		} else {
			print_err_code("kmem info", code);
		}
		return;
	}
	println("");
	println("=== Kernel Memory (kusr) ===");
	print("Kmem heap total: ");
	print_u64(ki.kmem_heap_total_kb);
	println(" KB");
	print("Kmem heap used : ");
	print_u64(ki.kmem_heap_used_kb);
	println(" KB");
	print("Kmem heap free : ");
	print_u64(ki.kmem_heap_free_kb);
	println(" KB");
	print("Bootmem free   : ");
	print_u64(ki.bootmem_free_kb);
	println(" KB");
}

static const char *
state_name(uint32_t s)
{
	switch (s) {
	case 1:
		return ("EMBRYO");
	case 2:
		return ("RUN");
	case 3:
		return ("ACTIVE");
	case 4:
		return ("SLEEP");
	case 5:
		return ("DEAD");
	default:
		return ("FREE");
	}
}

static void
cmd_ps(void)
{
	struct api_proc_info	procs[MAX_PROCS];
	int			n, i, code;

	n = procList(procs, MAX_PROCS);
	if (n < 0) {
		code = errno;
		print_err_code("ps", code);
		return;
	}
	println("PID\tPPID\tSTATE\tNAME");
	for (i = 0; i < n; i++) {
		print_int((int)procs[i].pid);
		printc('\t');
		print_int((int)procs[i].ppid);
		printc('\t');
		print(state_name(procs[i].state));
		printc('\t');
		println(procs[i].name);
	}
}

static void
cmd_cpus(void)
{
	struct api_cpuinfo	info;
	struct api_cpu_entry	*cpu;
	uint32_t		i, j;
	int			code;

	if (sysCpuInfo(&info) < 0) {
		code = errno;
		print_err_code("cpus", code);
		return;
	}
	print("CPUs detected: ");
	print_int((int)info.cpu_count);
	printc('\n');
	println("CPU\tLAPIC\tONLINE\tCURPID\tTID\tSTATE\tNAME\tPIDS");
	for (i = 0; i < info.entry_count && i < MAX_CPUS; i++) {
		cpu = &info.entries[i];
		print_int((int)cpu->cpu_index);
		printc('\t');
		print_int((int)cpu->lapic_id);
		printc('\t');
		print(cpu->online ? "yes" : "no");
		printc('\t');
		if (cpu->pid) {
			print_int((int)cpu->pid);
		} else {
			print("-");
		}
		printc('\t');
		if (cpu->tid) {
			print_int((int)cpu->tid);
		} else {
			print("-");
		}
		printc('\t');
		print(state_name(cpu->state));
		printc('\t');
		print(cpu->proc_name[0] ? cpu->proc_name : "-");
		printc('\t');
		if (cpu->pid_count == 0) {
			print("-");
		}
		for (j = 0; j < cpu->pid_count && j < MAX_CPU_PIDS; j++) {
			if (j > 0) {
				printc(',');
			}
			print_int((int)cpu->pids[j]);
		}
		printc('\n');
	}
}

static void
cmd_time(void)
{
	struct api_timeinfo	ti;
	int			code;

	if (sysTimeInfo(&ti) < 0) {
		code = errno;
		print_err_code("time", code);
		return;
	}
	println("=== Time Info ===");
	print("Local date     : ");
	print_datetime(ti.local_sec);
	printc('\n');
	print("Wall-clock date: ");
	print_datetime(ti.wall_sec);
	printc('\n');
	print("Timezone offset: ");
	print_int((int)(ti.timezone_offset / 3600));
	print(" hours\n");
	print("Wall-clock sec : ");
	print_u64(ti.wall_sec);
	printc('\n');
	print("Wall-clock ns  : ");
	print_u64(ti.wall_nsec);
	printc('\n');
	print("Uptime         : ");
	print_duration(ti.uptime_sec);
	printc('\n');
	print("Uptime sec     : ");
	print_u64(ti.uptime_sec);
	printc('\n');
	print("Uptime ns      : ");
	print_u64(ti.uptime_nsec);
	printc('\n');
	print("Timer ticks    : ");
	print_u64(ti.ticks);
	printc('\n');
	print("Timer frequency: ");
	print_u64(ti.frequency);
	printc('\n');
	print("Clocksource    : ");
	println(ti.clocksource);
}

static void
cmd_cat(int argc, char **argv)
{
	char	buf[256];
	FILE	*file;
	size_t	n;
	int	code;

	if (argc < 2) {
		println("cat: missing file");
		return;
	}
	file = fopen(argv[1], "r");
	if (!file) {
		code = errno;
		print("cat: ");
		print(argv[1]);
		print(": ");
		print(err_str(code));
		print(" (code ");
		print_int(-code);
		print(")\n");
		return;
	}
	for (;;) {
		n = fread(buf, 1, sizeof(buf), file);
		if (n > 0) {
			termWrite(buf, n);
		}
		if (n < sizeof(buf)) {
			break;
		}
	}
	if (ferror(file)) {
		code = errno;
		print("\ncat: ");
		print(argv[1]);
		print(": read error: ");
		print(err_str(code));
		print(" (code ");
		print_int(-code);
		print(")\n");
	}
	fclose(file);
}

static void
cmd_clear(void)
{
	print("\033[2J\033[H");
}

static void
cmd_env(void)
{
	int	i;

	if (!g_envp) {
		return;
	}
	for (i = 0; g_envp[i]; i++) {
		println(g_envp[i]);
	}
}

static int
hex_digit(char c)
{
	if (c >= '0' && c <= '9') {
		return (c - '0');
	}
	if (c >= 'a' && c <= 'f') {
		return (c - 'a' + 10);
	}
	if (c >= 'A' && c <= 'F') {
		return (c - 'A' + 10);
	}
	return (-1);
}

static void
cmd_color(int argc, char **argv)
{
	const char	*hex;
	char		buf[32];
	int		d[6];
	int		r, g, b, i, pos;

	if (argc < 2) {
		print("\033[0m\033[39m");
		println("color reset");
		return;
	}

	hex = argv[1];
	if (hex[0] == '#') {
		hex++;
	}
	if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
		hex += 2;
	}
	if (strlen(hex) != 6) {
		println("color: need 6 hex digits (e.g. FF0000)");
		return;
	}
	for (i = 0; i < 6; i++) {
		d[i] = hex_digit(hex[i]);
		if (d[i] < 0) {
			println("color: invalid hex");
			return;
		}
	}
	r = (d[0] << 4) | d[1];
	g = (d[2] << 4) | d[3];
	b = (d[4] << 4) | d[5];
	pos = snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm", r, g, b);
	if (pos > 0) {
		termWrite(buf, (size_t)pos);
	}
}

static void
cmd_kusr(void)
{
	char	pass[128];
	ssize_t	n;
	int	pos;
	int	code;
	char	c;

	pos = 0;
	print("kusr password: ");
	for (;;) {
		n = termReadFlags(&c, 1,
		    TERM_READ_IGNORE_SIGINT | TERM_READ_NO_ECHO);
		if (n <= 0) {
			continue;
		}
		if (c == '\r' || c == '\n') {
			printc('\n');
			pass[pos] = '\0';
			break;
		}
		if (c == '\b' || c == 0x7f) {
			if (pos > 0) {
				pos--;
			}
			continue;
		}
		if (c == 0x03) {
			print("^C\n");
			return;
		}
		if (c >= 32 && c < 127 && pos < (int)sizeof(pass) - 1) {
			pass[pos++] = c;
		}
	}

	if (kusrAuth(pass) == 0) {
		memset(pass, 0, sizeof(pass));
		g_kusr_authed = 1;
		println("kusr: authenticated");
		return;
	}
	code = errno;
	memset(pass, 0, sizeof(pass));
	if (code == EPERM) {
		println("kusr: wrong password");
	} else if (code == ENOENT) {
		println("kusr: not configured (no kusr password set)");
	} else {
		print_err_code("kusr", code);
	}
}

static void
show_prompt(void)
{
	char	buf[MAX_PATH];

	if (fsGetcwd(buf, sizeof(buf)) == 0) {
		print(buf);
	}
	print(" $ ");
}

static int
exec_builtin(int argc, char **argv)
{
	const char	*cmd;

	cmd = argv[0];
	if (strcmp(cmd, "help") == 0) {
		cmd_help();
		return (0);
	}
	if (strcmp(cmd, "echo") == 0) {
		cmd_echo(argc, argv);
		return (0);
	}
	if (strcmp(cmd, "pwd") == 0) {
		cmd_pwd();
		return (0);
	}
	if (strcmp(cmd, "cd") == 0) {
		cmd_cd(argc, argv);
		return (0);
	}
	if (strcmp(cmd, "ls") == 0) {
		cmd_ls(argc, argv);
		return (0);
	}
	if (strcmp(cmd, "ps") == 0) {
		cmd_ps();
		return (0);
	}
	if (strcmp(cmd, "cpus") == 0) {
		cmd_cpus();
		return (0);
	}
	if (strcmp(cmd, "time") == 0) {
		cmd_time();
		return (0);
	}
	if (strcmp(cmd, "mem") == 0) {
		cmd_mem();
		return (0);
	}
	if (strcmp(cmd, "cat") == 0) {
		cmd_cat(argc, argv);
		return (0);
	}
	if (strcmp(cmd, "clear") == 0) {
		cmd_clear();
		return (0);
	}
	if (strcmp(cmd, "color") == 0) {
		cmd_color(argc, argv);
		return (0);
	}
	if (strcmp(cmd, "kusr") == 0) {
		cmd_kusr();
		return (0);
	}
	if (strcmp(cmd, "drm_list") == 0) {
		cmd_drm_list();
		return (0);
	}
	if (strcmp(cmd, "drm_switch") == 0) {
		cmd_drm_switch(argc, argv);
		return (0);
	}
	if (strcmp(cmd, "env") == 0) {
		cmd_env();
		return (0);
	}
	if (strcmp(cmd, "exit") == 0) {
		return (1);
	}
	return (-1);
}

static int
exec_line(int argc, char **argv)
{
	char	path[MAX_PATH];
	int	ret;
	int	code;

	if (argc == 0) {
		return (0);
	}

	ret = exec_builtin(argc, argv);
	if (ret >= 0) {
		return (ret);
	}

	if (resolve_path(argv[0], path, sizeof(path)) == 0) {
		run_external(path, argv, g_envp);
		return (0);
	}

	code = errno;
	if (code == ENOENT) {
		print("sh: command not found: ");
		println(argv[0]);
	} else {
		print("sh: ");
		print(argv[0]);
		print(": ");
		print(err_str(code));
		print(" (code ");
		print_int(-code);
		print(")\n");
	}
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	char	line[MAX_LINE];
	char	*pargv[MAX_ARGS];
	int	i, len, pargc, rc;

	personality(0);
	g_envp = envp;

	if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
		strncpy(line, argv[2], sizeof(line) - 1);
		line[sizeof(line) - 1] = '\0';
		for (i = 0; i < MAX_ARGS; i++) {
			pargv[i] = NULL;
		}
		pargc = parse_line(line, pargv, MAX_ARGS);
		if (pargc > 0) {
			rc = exec_line(pargc, pargv);
			return (rc);
		}
		return (0);
	}

	println("otsos2 shell (sh) - type 'help' for commands");

	for (;;) {
		memset(line, 0, sizeof(line));
		for (i = 0; i < MAX_ARGS; i++) {
			pargv[i] = NULL;
		}

		show_prompt();
		len = read_line(line, sizeof(line));
		if (len < 0) {
			return (0);
		}

		pargc = parse_line(line, pargv, MAX_ARGS);
		if (pargc == 0) {
			continue;
		}

		rc = exec_line(pargc, pargv);
		if (rc == 1) {
			return (0);
		}
	}
}
