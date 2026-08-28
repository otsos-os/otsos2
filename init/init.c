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

$define %type char as 8 bit signed
$define %type int as 32 bit signed
$define %type long as 64 bit signed
$define %type unsigned long as 64 bit unsigned
$define %type short as 16 bit signed
$define %type unsigned short as 16 bit unsigned
$define %type unsigned int as 32 bit unsigned
$define %type long long as 64 bit signed
$define %type unsigned long long as 64 bit unsigned
$define %type api_input_event as struct mirroring the kernel input event ABI
$define %type kevent as struct with event ident, filter, flags, fflags,
    data, udata, input
$define %type kevent_args as struct with kq_idx, changelist, nchanges, eventlist, nevents, timeout_ms

$define %func _start as start with args void
$define %func syscall1 as function with args long, long
$define %func syscall3 as function with args long, long, long, long
$define %func termWrite as function with args const void *, unsigned long
$define %func termRead as function with args void *, unsigned long
$define %func procSpawnAbi as function with args const char *, char *const *, char *const *, unsigned int
$define %func procWait as function with args int *
$define %func procTryWait as function with args int *
$define %func drain_exit_records as function with args int
$define %func scan_events as function with args struct kevent *, int, int
$define %func kqueue_create as function with args void
$define %func kqueue_close as function with args int
$define %func kevent as function with args int, struct kevent *, int, struct kevent *, int, long long
$define %func strlen as function with args const char *
$define %func print as procedure with args const char *
$define %func print_long as procedure with args long
$define %func trim_newline as procedure with args char *

*/

/* !SPACE!

$space %export _start
$space %internal syscall1, syscall3, termWrite, termRead
$space %internal procSpawnAbi
$space %internal procWait, procTryWait, drain_exit_records, scan_events
$space %internal kqueue_create, kqueue_close, kevent
$space %internal strlen, print, print_long, trim_newline

*/

#define	CALL_TERM_READ		0x100
#define	CALL_TERM_WRITE		0x101
#define	CALL_TERM_POWER		0x111
#define	CALL_PROC_SPAWN		0x402
#define	CALL_PROC_WAIT		0x404
#define	CALL_PROC_EXIT		0x403
#define	CALL_PROC_TRYWAIT	0x414
#define	CALL_EVENT_KQUEUE	0x700
#define	CALL_EVENT_KEVENT	0x701
#define	CALL_EVENT_CLOSE	0x702

#define	API_TERM_POWER_CHANGE	1
#define	TERM_STATE_ACTIVE	0

#define	EVFILT_READ	(-1)
#define	EVFILT_TIMER	(-3)
#define	EVFILT_PROC	(-4)

#define	EV_ADD		0x0001
#define	EV_DELETE	0x0002
#define	EV_ENABLE	0x0004
#define	EV_ONESHOT	0x0010
#define	EV_CLEAR	0x0020
#define	EV_EOF		0x8000

#define	NOTE_EXIT	0x80000000U
#define	NOTE_REAP	0x00000008U
#define	INIT_PID	1
#define	MAX_REAP_BURST	64
#define	API_PROC_SPAWN_ABI_POSIX	0

struct api_input_event {
	unsigned long long	timestamp;
	unsigned long long	seq;
	unsigned int		type;
	unsigned int		device;
	unsigned int		flags;
	unsigned int		lost;
	int			x;
	int			y;
	int			dx;
	int			dy;
	int			dz;
	unsigned int		buttons;
	unsigned int		key;
	unsigned int		raw;
	unsigned int		mods;
	unsigned int		ch;
};

struct kevent {
	unsigned long long	ident;
	short			filter;
	unsigned short		flags;
	unsigned int		fflags;
	long long		data;
	unsigned long long	udata;
	struct api_input_event	input;
};

struct kevent_args {
	int			kq_idx;
	struct kevent		*changelist;
	int			nchanges;
	struct kevent		*eventlist;
	int			nevents;
	long long		timeout_ms;
};

struct term_power_args {
	int			op;
	int			tty;
	int			state;
	int			flags;
};
struct proc_spawn_args {
	unsigned int		size;
	unsigned int		flags;
	unsigned int		abi;
	unsigned int		pad;
	const char		*path;
	char *const		*argv;
	char *const		*envp;
};

static long
syscall1(long num, long arg1)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(arg1)
	    : "rcx", "r11", "memory");
	return (ret);
}

static long
syscall3(long num, long arg1, long arg2, long arg3)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
	    : "rcx", "r11", "memory");
	return (ret);
}

static long
termWrite(const void *buf, unsigned long count)
{
	return (syscall3(CALL_TERM_WRITE, (long)buf, (long)count, 0));
}

static long
termRead(void *buf, unsigned long count)
{
	return (syscall3(CALL_TERM_READ, (long)buf, (long)count, 0));
}

static long
termPower(int op, int tty, int state)
{
	struct term_power_args	args;

	args.op = op;
	args.tty = tty;
	args.state = state;
	args.flags = 0;
	return (syscall1(CALL_TERM_POWER, (long)&args));
}

static long
procSpawnAbi(const char *path, char *const argv[], char *const envp[],
    unsigned int abi)
{
	struct proc_spawn_args	args;
	args.size = sizeof(args);
	args.flags = 0;
	args.abi = abi;
	args.pad = 0;
	args.path = path;
	args.argv = argv;
	args.envp = envp;
	return (syscall1(CALL_PROC_SPAWN, (long)&args));
}

static long
procWait(int *status)
{
	return (syscall1(CALL_PROC_WAIT, (long)status));
}

static long
procTryWait(int *status)
{
	return (syscall1(CALL_PROC_TRYWAIT, (long)status));
}

static int
drain_exit_records(int fg)
{
	int	status, got, hit, i;

	hit = 0;
	for (i = 0; i < MAX_REAP_BURST; i++) {
		status = 0;
		got = (int)procTryWait(&status);
		if (got < 0) {
			break;
		}
		if (fg >= 0 && got == fg) {
			hit = 1;
		}
	}
	return (hit);
}

static int
scan_events(struct kevent *events, int n, int fg)
{
	int	exited, i;

	exited = 0;
	for (i = 0; i < n; i++) {
		if (events[i].filter != EVFILT_PROC) {
			continue;
		}
		if (events[i].fflags & NOTE_REAP) {
			if (drain_exit_records(fg)) {
				exited = 1;
			}
		}
		if (events[i].ident == (unsigned long long)fg &&
		    (events[i].fflags & NOTE_EXIT)) {
			exited = 1;
		}
	}
	return (exited);
}

static int
kqueue_create(void)
{
	return ((int)syscall1(CALL_EVENT_KQUEUE, 0));
}

static int __attribute__((unused))
kqueue_close(int kq)
{
	return ((int)syscall1(CALL_EVENT_CLOSE, (long)kq));
}

static int
kevent(int kq, struct kevent *changes, int nchanges,
    struct kevent *events, int nevents, long long timeout_ms)
{
	struct kevent_args	args;

	args.kq_idx = kq;
	args.changelist = changes;
	args.nchanges = nchanges;
	args.eventlist = events;
	args.nevents = nevents;
	args.timeout_ms = timeout_ms;
	return ((int)syscall3(CALL_EVENT_KEVENT, 0, (long)&args, 0));
}

static unsigned long
strlen(const char *s)
{
	unsigned long	len;

	len = 0;
	while (s[len]) {
		len++;
	}
	return (len);
}

static void
print(const char *s)
{
	termWrite(s, strlen(s));
}

static void
print_long(long value)
{
	char	buf[32];
	long	n;
	int	i, neg;

	i = 0;
	neg = 0;
	if (value < 0) {
		neg = 1;
		value = -value;
	}
	do {
		n = value % 10;
		buf[i++] = (char)('0' + n);
		value /= 10;
	} while (value > 0 && i < (int)sizeof(buf) - 1);
	if (neg && i < (int)sizeof(buf) - 1) {
		buf[i++] = '-';
	}
	while (i > 0) {
		i--;
		termWrite(&buf[i], 1);
	}
}

static void
trim_newline(char *s)
{
	unsigned long	i;

	i = 0;
	while (s[i]) {
		if (s[i] == '\n' || s[i] == '\r') {
			s[i] = 0;
			return;
		}
		i++;
	}
}

void
_start(void)
{
	struct kevent	changes[2];
	struct kevent	events[8];
	char		*argv[2];
	char		path[128];
	long		bytes, pid;
	int		kq, child_pid, n, status;

	/* Wake the system terminal before using it.
	 * The kernel boots with all TTYs suspended. */
	termPower(API_TERM_POWER_CHANGE, 1, TERM_STATE_ACTIVE);

	print("\n");
	print("Hello init (event-driven)\n");

	kq = kqueue_create();
	if (kq < 0) {
		print("init: kqueue_create failed, "
		    "falling back to polling\n");
		kq = -1;
	}

	if (kq >= 0) {
		changes[0].ident = INIT_PID;
		changes[0].filter = EVFILT_PROC;
		changes[0].flags = EV_ADD | EV_CLEAR;
		changes[0].fflags = NOTE_REAP;
		changes[0].data = 0;
		changes[0].udata = 0;
		if (kevent(kq, changes, 1, 0, 0, -1) < 0) {
			print("init: NOTE_REAP registration failed, "
			    "adopted exit records will accumulate\n");
		}
	}

	child_pid = -1;

	while (1) {
		print("Enter program path (relative, e.g. hello): ");
		bytes = termRead(path, 120);
		if (bytes <= 0) {
			continue;
		}
		if (bytes >= 120) {
			bytes = 119;
		}
		path[bytes] = 0;
		trim_newline(path);

		if (path[0] == 0) {
			print("empty path\n");
			continue;
		}

		argv[0] = path;
		argv[1] = 0;

		pid = procSpawnAbi(path, argv, 0, API_PROC_SPAWN_ABI_POSIX);
		if (pid < 0) {
			print("procSpawn failed: ");
			print_long(pid);
			print("\n");
			continue;
		}

		child_pid = (int)pid;

		if (kq >= 0) {
			int child_exited;
			changes[0].ident =
			    (unsigned long long)child_pid;
			changes[0].filter = EVFILT_PROC;
			changes[0].flags = EV_ADD | EV_ONESHOT;
			changes[0].fflags = NOTE_EXIT;
			changes[0].data = 0;
			changes[0].udata =
			    (unsigned long long)child_pid;
			kevent(kq, changes, 1, 0, 0, -1);

			child_exited = 0;
			while (!child_exited) {
				n = kevent(kq, 0, 0, events, 8, -1);
				if (n < 0) {
					break;
				}
				if (scan_events(events, n, child_pid)) {
					child_exited = 1;
				}
			}
		} else {
			status = 0;
			while (procWait(&status) < 0) {
			}
		}

		(void)drain_exit_records(-1);
		child_pid = -1;
	}
}
