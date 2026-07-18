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

/*
 * POSIX device and TTY test (replaces the old keyboard test).
 * Runs in the default POSIX personality and exercises the new
 * devfs/termios/winsize/session/pty syscalls.
 */

/* Linux x86_64 syscall numbers. */
#define	SYS_read		0
#define	SYS_write		1
#define	SYS_open		2
#define	SYS_close		3
#define	SYS_ioctl		16
#define	SYS_getpid		39
#define	SYS_getppid		110
#define	SYS_setpgid		109
#define	SYS_getpgrp		111
#define	SYS_setsid		112
#define	SYS_getpgid		121
#define	SYS_getsid		124
#define	SYS_exit		60

/* open(2) flags. */
#define	O_RDONLY		0
#define	O_WRONLY		1
#define	O_RDWR			2
#define	O_NOCTTY		0x00000100
#define	O_NONBLOCK		0x00000800
#define	O_CLOEXEC		0x00080000

/* ioctl requests. */
#define	TIOCGWINSZ		0x5413
#define	TIOCSWINSZ		0x5414
#define	TCGETS			0x5401
#define	TCSETS			0x5402
#define	TIOCGPGRP		0x540F
#define	TIOCSPGRP		0x5410
#define	TIOCGSID		0x5429
#define	TIOCGPTN		0x80045430

/* termios bits. */
#define	NCCS			32
#define	VINTR			0
#define	VQUIT			1
#define	VERASE			2
#define	VKILL			3
#define	VEOF			4
#define	VTIME			5
#define	VMIN			6
#define	VSUSP			10
#define	VEOL			11
#define	VEOL2			16
#define	B38400			0000015
#define	ISIG			0000001
#define	ICANON			0000002
#define	ECHO			0000010
#define	ECHOE			0000020

typedef unsigned int		tcflag_t;
typedef unsigned char		cc_t;
typedef unsigned int		speed_t;

struct winsize {
	unsigned short	ws_row;
	unsigned short	ws_col;
	unsigned short	ws_xpixel;
	unsigned short	ws_ypixel;
};

struct termios {
	tcflag_t	c_iflag;
	tcflag_t	c_oflag;
	tcflag_t	c_cflag;
	tcflag_t	c_lflag;
	cc_t		c_line;
	cc_t		c_cc[NCCS];
	speed_t		c_ispeed;
	speed_t		c_ospeed;
};

#define	STDIN_FILENO		0
#define	STDOUT_FILENO		1
#define	STDERR_FILENO		2

static long
syscall0(long num)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num)
	    : "rcx", "r11", "memory");
	return (ret);
}

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
syscall2(long num, long arg1, long arg2)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(arg1), "S"(arg2)
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
syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6)
{
	long	ret;
	register long	r10 __asm__("r10") = a4;
	register long	r8 __asm__("r8") = a5;
	register long	r9 __asm__("r9") = a6;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8),
	    "r"(r9)
	    : "rcx", "r11", "memory");
	return (ret);
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
	(void)syscall3(SYS_write, STDOUT_FILENO, (long)s, (long)strlen(s));
}

static void
print_int(int v)
{
	char	buf[16];
	int	pos;
	int	neg;
	int	i;
	int	t;

	pos = 0;
	neg = 0;
	if (v < 0) {
		neg = 1;
		if (v == -2147483648) {
			v = -2147483647;
		}
		v = -v;
	}
	if (v == 0) {
		buf[pos++] = '0';
	} else {
		t = v;
		while (t > 0) {
			buf[pos++] = (char)('0' + (t % 10));
			t /= 10;
		}
		for (i = 0; i < pos / 2; i++) {
			char	tmp;

			tmp = buf[i];
			buf[i] = buf[pos - 1 - i];
			buf[pos - 1 - i] = tmp;
		}
	}
	if (neg) {
		buf[pos++] = '-';
	}
	buf[pos] = '\0';
	print(buf);
}

static void
print_hex(unsigned long v)
{
	const char	*hex;
	char		buf[17];
	int		i;
	int		digit;
	int		started;

	hex = "0123456789ABCDEF";
	started = 0;
	for (i = 15; i >= 0; i--) {
		digit = (int)((v >> (i * 4)) & 0xF);
		if (digit || started || i == 0) {
			buf[started++] = hex[digit];
		}
	}
	buf[started] = '\0';
	print(buf);
}

static void
print_ok(const char *name)
{
	print("[OK] ");
	print(name);
	print("\n");
}

static void
print_fail(const char *name, long err)
{
	print("[FAIL] ");
	print(name);
	print(" err=");
	print_int((int)err);
	print("\n");
}

static int
fail(const char *name, long ret)
{
	if (ret < 0) {
		print_fail(name, ret);
		return (1);
	}
	print_ok(name);
	return (0);
}

static int
open_file(const char *path, int flags)
{
	return ((int)syscall3(SYS_open, (long)path, flags, 0));
}

static void
close_fd(int fd)
{
	(void)syscall1(SYS_close, fd);
}

static int
test_session(void)
{
	long	pid;
	long	pgrp1;
	long	sid1;
	long	pgrp2;
	long	sid2;
	int	failures;

	failures = 0;
	pid = syscall0(SYS_getpid);
	if (pid < 0) {
		fail("getpid", pid);
		failures++;
	}
	pgrp1 = syscall0(SYS_getpgrp);
	sid1 = syscall0(SYS_getsid);
	if (fail("getpgrp", pgrp1)) {
		failures++;
	}
	if (fail("getsid", sid1)) {
		failures++;
	}
	pgrp2 = syscall0(SYS_setsid);
	if (fail("setsid", pgrp2)) {
		failures++;
	}
	sid2 = syscall0(SYS_getsid);
	if (fail("getsid-after-setsid", sid2)) {
		failures++;
	}
	if (pgrp2 != sid2) {
		print_fail("setsid-sid-match", sid2 - pgrp2);
		failures++;
	}
	return (failures);
}

static int
test_dev_devices(void)
{
	int	fd;
	int	failures;
	char	buf[16];
	long	n;

	failures = 0;

	fd = open_file("/dev/null", O_WRONLY);
	if (fail("/dev/null open", fd)) {
		failures++;
	} else {
		n = syscall3(SYS_write, fd, (long)"hello", 5);
		if (n != 5) {
			fail("/dev/null write", n);
			failures++;
		} else {
			print_ok("/dev/null write");
		}
		close_fd(fd);
	}

	fd = open_file("/dev/zero", O_RDONLY);
	if (fail("/dev/zero open", fd)) {
		failures++;
	} else {
		n = syscall3(SYS_read, fd, (long)buf, 8);
		if (n != 8) {
			fail("/dev/zero read", n);
			failures++;
		} else {
			int	i;
			int	ok;

			ok = 1;
			for (i = 0; i < 8; i++) {
				if (buf[i] != 0) {
					ok = 0;
				}
			}
			if (ok) {
				print_ok("/dev/zero read");
			} else {
				print_fail("/dev/zero value", 0);
				failures++;
			}
		}
		close_fd(fd);
	}

	fd = open_file("/dev/random", O_RDONLY);
	if (fail("/dev/random open", fd)) {
		failures++;
	} else {
		n = syscall3(SYS_read, fd, (long)buf, 4);
		if (n != 4) {
			fail("/dev/random read", n);
			failures++;
		} else {
			print_ok("/dev/random read");
		}
		close_fd(fd);
	}

	fd = open_file("/dev/urandom", O_RDONLY);
	if (fail("/dev/urandom open", fd)) {
		failures++;
	} else {
		n = syscall3(SYS_read, fd, (long)buf, 4);
		if (n != 4) {
			fail("/dev/urandom read", n);
			failures++;
		} else {
			print_ok("/dev/urandom read");
		}
		close_fd(fd);
	}

	fd = open_file("/dev/tty", O_RDWR);
	if (fail("/dev/tty open", fd)) {
		failures++;
	} else {
		close_fd(fd);
	}

	fd = open_file("/dev/console", O_WRONLY);
	if (fail("/dev/console open", fd)) {
		failures++;
	} else {
		close_fd(fd);
	}

	return (failures);
}

static int
test_termios_winsize(void)
{
	int		fd;
	int		failures;
	struct winsize	ws;
	struct termios	t;
	long		ret;
	tcflag_t	old_lflag;

	failures = 0;
	fd = open_file("/dev/tty", O_RDWR);
	if (fd < 0) {
		print_fail("termios tty open", fd);
		return (1);
	}

	ret = syscall3(SYS_ioctl, fd, TIOCGWINSZ, (long)&ws);
	if (fail("TIOCGWINSZ", ret)) {
		failures++;
	} else {
		print("      rows=");
		print_int((int)ws.ws_row);
		print(" cols=");
		print_int((int)ws.ws_col);
		print("\n");
	}

	ret = syscall3(SYS_ioctl, fd, TIOCGPGRP, (long)&ret);
	if (fail("TIOCGPGRP", ret)) {
		failures++;
	}

	ret = syscall3(SYS_ioctl, fd, TIOCGSID, (long)&ret);
	if (fail("TIOCGSID", ret)) {
		failures++;
	}

	ret = syscall3(SYS_ioctl, fd, TCGETS, (long)&t);
	if (fail("TCGETS", ret)) {
		failures++;
	} else {
		print("      lflag=0x");
		print_hex(t.c_lflag);
		print("\n");
		old_lflag = t.c_lflag;
		t.c_lflag &= ~(ICANON | ECHO);
		ret = syscall3(SYS_ioctl, fd, TCSETS, (long)&t);
		if (fail("TCSETS raw", ret)) {
			failures++;
		}
		t.c_lflag = old_lflag;
		ret = syscall3(SYS_ioctl, fd, TCSETS, (long)&t);
		if (fail("TCSETS restore", ret)) {
			failures++;
		}
	}

	ws.ws_row = 30;
	ws.ws_col = 100;
	ret = syscall3(SYS_ioctl, fd, TIOCSWINSZ, (long)&ws);
	if (fail("TIOCSWINSZ", ret)) {
		failures++;
	}
	ret = syscall3(SYS_ioctl, fd, TIOCGWINSZ, (long)&ws);
	if (fail("TIOCGWINSZ after set", ret)) {
		failures++;
	} else {
		if (ws.ws_row != 30 || ws.ws_col != 100) {
			print_fail("winsize mismatch", 0);
			failures++;
		}
	}

	close_fd(fd);
	return (failures);
}

static int
test_pty(void)
{
	int	master;
	int	slave;
	int	slave_num;
	int	failures;
	long	ret;
	char	path[32];
	char	msg[] = "hello pty";
	char	buf[32];
	int	i;

	failures = 0;
	master = open_file("/dev/ptmx", O_RDWR | O_NOCTTY);
	if (fail("/dev/ptmx open", master)) {
		failures++;
		return (failures);
	}

	ret = syscall3(SYS_ioctl, master, TIOCGPTN, (long)&slave_num);
	if (fail("TIOCGPTN", ret)) {
		failures++;
		close_fd(master);
		return (failures);
	}
	print("      slave number=");
	print_int(slave_num);
	print("\n");

	i = 0;
	path[i++] = '/';
	path[i++] = 'd';
	path[i++] = 'e';
	path[i++] = 'v';
	path[i++] = '/';
	path[i++] = 'p';
	path[i++] = 't';
	path[i++] = 's';
	path[i++] = '/';
	if (slave_num >= 10) {
		path[i++] = (char)('0' + (slave_num / 10));
	}
	path[i++] = (char)('0' + (slave_num % 10));
	path[i] = '\0';

	slave = open_file(path, O_RDWR | O_NOCTTY);
	if (fail("pty slave open", slave)) {
		failures++;
		close_fd(master);
		return (failures);
	}

	ret = syscall3(SYS_write, master, (long)msg, (long)sizeof(msg) - 1);
	if (fail("pty master write", ret)) {
		failures++;
	}

	ret = syscall3(SYS_read, slave, (long)buf, sizeof(buf) - 1);
	if (fail("pty slave read", ret)) {
		failures++;
	} else if (ret > 0) {
		buf[ret] = '\0';
		print("      read: ");
		print(buf);
		print("\n");
	}

	ret = syscall3(SYS_write, slave, (long)msg, (long)sizeof(msg) - 1);
	if (fail("pty slave write", ret)) {
		failures++;
	}

	ret = syscall3(SYS_read, master, (long)buf, sizeof(buf) - 1);
	if (fail("pty master read", ret)) {
		failures++;
	} else if (ret > 0) {
		buf[ret] = '\0';
		print("      read: ");
		print(buf);
		print("\n");
	}

	close_fd(slave);
	close_fd(master);
	return (failures);
}

void
_start(void)
{
	int	failures;

	print("\n=== POSIX devfs/termios/pty test ===\n");

	failures = 0;
	failures += test_session();
	failures += test_dev_devices();
	failures += test_termios_winsize();
	failures += test_pty();

	print("\n");
	if (failures == 0) {
		print("ALL TESTS PASSED\n");
	} else {
		print("FAILURES: ");
		print_int(failures);
		print("\n");
	}
	(void)syscall1(SYS_exit, 0);
	for (;;) {}
}
