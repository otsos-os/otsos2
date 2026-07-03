/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * kbdtest - demonstrates TTY power management and raw keyboard events.
 *
 * Usage: /bin/kbdtest
 *
 * The program suspends the active terminal (TTY 1), reads a few raw
 * keyboard events via kqueue EVFILT_KBD, then resumes the TTY and prints
 * what was received.  While the TTY is suspended, normal text output is
 * suppressed and key presses do not appear on the console.
 */

#define	KBDTEST_TTY		1

#define	CALL_TERM_POWER		0x111
#define	CALL_EVENT_KQUEUE	0x700
#define	CALL_EVENT_KEVENT	0x701
#define	CALL_EVENT_CLOSE	0x702
#define	CALL_TERM_WRITE		0x101
#define	CALL_PROC_EXIT		0x403
#define	CALL_PERSONALITY	0xFFFF

#define	EVFILT_KBD	(-7)

#define	EV_ADD		0x0001
#define	EV_CLEAR	0x0020

#define	API_TERM_POWER_GET	0
#define	API_TERM_POWER_CHANGE	1
#define	API_TERM_POWER_RESET	2

#define	TTY_STATE_ACTIVE	0
#define	TTY_STATE_SUSPENDED	1
#define	TTY_STATE_DISABLED	2

#define	KBD_DATA_SCANCODE(v)	((unsigned short)((unsigned long)(v) & 0xFFFF))
#define	KBD_DATA_RELEASED(v)	(((unsigned long)(v) >> 16) & 1)
#define	KBD_DATA_EXTENDED(v)	(((unsigned long)(v) >> 17) & 1)
#define	KBD_DATA_ASCII(v)	((char)(((unsigned long)(v) >> 24) & 0xFF))

struct api_term_power {
	int	op;
	int	tty;
	int	state;
	int	flags;
};

struct kevent {
	unsigned long long	ident;
	short			filter;
	unsigned short		flags;
	unsigned int		fflags;
	long long		data;
	unsigned long long	udata;
};

struct kevent_args {
	int			kq_idx;
	struct kevent		*changelist;
	int			nchanges;
	struct kevent		*eventlist;
	int			nevents;
	long long		timeout_ms;
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

static unsigned long
strlen_s(const char *s)
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
	syscall3(CALL_TERM_WRITE, (long)s, (long)strlen_s(s), 0);
}

static void
print_int(int v)
{
	char	buf[12];
	int	pos;
	int	neg;
	int	i;

	pos = 0;
	neg = 0;
	if (v < 0) {
		neg = 1;
		v = -v;
	}
	if (v == 0) {
		buf[pos++] = '0';
	} else {
		i = pos;
		while (v > 0) {
			buf[i++] = (char)('0' + (v % 10));
			v /= 10;
		}
		while (i > pos) {
			buf[pos++] = buf[--i];
		}
	}
	if (neg) {
		buf[pos++] = '-';
	}
	buf[pos] = '\0';
	print(buf);
}

static void
print_hex(unsigned short v)
{
	const char	*hex;
	char		buf[5];
	int		i;

	hex = "0123456789ABCDEF";
	for (i = 3; i >= 0; i--) {
		buf[3 - i] = hex[(v >> (i * 4)) & 0xF];
	}
	buf[4] = '\0';
	print(buf);
}

static int
term_power(struct api_term_power *args)
{
	return ((int)syscall1(CALL_TERM_POWER, (long)args));
}

static int
kqueue_create(void)
{
	return ((int)syscall1(CALL_EVENT_KQUEUE, 0));
}

static int
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

static void
proc_exit(int code)
{
	syscall1(CALL_PROC_EXIT, (long)code);
	for (;;) {}
}

static void
print_event(const struct kevent *ev)
{
	unsigned short	sc;
	int		rel;
	int		ext;
	char		ascii;

	sc = KBD_DATA_SCANCODE(ev->data);
	rel = KBD_DATA_RELEASED(ev->data);
	ext = KBD_DATA_EXTENDED(ev->data);
	ascii = KBD_DATA_ASCII(ev->data);

	print("  scancode=0x");
	print_hex(sc);
	print(" released=");
	print_int(rel);
	print(" extended=");
	print_int(ext);
	print(" ascii=");
	if (ascii >= 32 && ascii < 127) {
		char	buf[2];

		buf[0] = ascii;
		buf[1] = '\0';
		print("'");
		print(buf);
		print("'");
	} else {
		print_int((int)(unsigned char)ascii);
	}
	print("\n");
}

void
_start(void)
{
	struct api_term_power	args;
	struct kevent		changes[1];
	struct kevent		events[8];
	int			kq;
	int			n;
	int			i;
	int			ret;

	/* Switch from the default POSIX personality to native OTSOS API. */
	(void)syscall1(CALL_PERSONALITY, 0);

	print("kbdtest: starting\n");

	/* Verify current state of TTY 1. */
	args.op = API_TERM_POWER_GET;
	args.tty = KBDTEST_TTY;
	args.state = 0;
	args.flags = 0;
	ret = term_power(&args);
	if (ret < 0) {
		print("kbdtest: get state failed: ");
		print_int(ret);
		print("\n");
		return;
	}
	print("kbdtest: tty1 state is ");
	print_int(ret);
	print("\n");

	print("kbdtest: suspending TTY 1, press 5 keys (screen will blank)\n");

	/* Suspend TTY 1.  This requires kusr auth. */
	args.op = API_TERM_POWER_CHANGE;
	args.tty = KBDTEST_TTY;
	args.state = TTY_STATE_SUSPENDED;
	args.flags = 0;
	ret = term_power(&args);
	if (ret < 0) {
		print("kbdtest: suspend failed: ");
		print_int(ret);
		print("\n");
		return;
	}

	kq = kqueue_create();
	if (kq < 0) {
		print("kbdtest: kqueue_create failed\n");
		args.op = API_TERM_POWER_CHANGE;
		args.tty = KBDTEST_TTY;
		args.state = TTY_STATE_ACTIVE;
		(void)term_power(&args);
		return;
	}

	changes[0].ident = 0;
	changes[0].filter = EVFILT_KBD;
	changes[0].flags = EV_ADD | EV_CLEAR;
	changes[0].fflags = 0;
	changes[0].data = 0;
	changes[0].udata = 0;
	kevent(kq, changes, 1, 0, 0, -1);

	/* Read 5 key events. */
	for (i = 0; i < 5; ) {
		n = kevent(kq, 0, 0, events, 8, -1);
		if (n < 0) {
			print("kbdtest: kevent failed\n");
			break;
		}
		for (int j = 0; j < n && i < 5; j++, i++) {
			print_event(&events[j]);
		}
	}

	kqueue_close(kq);

	/* Resume TTY 1. */
	args.op = API_TERM_POWER_CHANGE;
	args.tty = KBDTEST_TTY;
	args.state = TTY_STATE_ACTIVE;
	args.flags = 0;
	ret = term_power(&args);
	if (ret < 0) {
		print("kbdtest: resume failed: ");
		print_int(ret);
		print("\n");
	}

	print("kbdtest: done, tty1 resumed\n");
	proc_exit(0);
}
