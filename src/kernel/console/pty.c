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

#include <kernel/console/pty.h>
#include <kernel/entity/entity.h>
#include <kernel/drivers/fs/devfs/devfs.h>
#include <kernel/api/posix/posix.h>
#include <kernel/event/event.h>
#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

#define	PTY_COUNT	32
#define	PTY_BUF_SIZE	1024

typedef struct pty_pair {
	int		id;
	u64		entity;
	int		open_master;
	int		open_slave;
	int		locked;
	struct termios	term;
	struct winsize	ws;
	u32		session;
	u32		foreground_pgrp;
	char		to_slave[PTY_BUF_SIZE];
	int		to_slave_head;
	int		to_slave_tail;
	int		to_slave_count;
	char		to_master[PTY_BUF_SIZE];
	int		to_master_head;
	int		to_master_tail;
	int		to_master_count;
} pty_pair_t;

static pty_pair_t	pty_pairs[PTY_COUNT];
static int		pty_initialized = 0;

static void	pty_default_termios(struct termios *t);
static pty_pair_t *pty_alloc(void);
static void	pty_signal_pgrp(pty_pair_t *p, int sig);
static int	pty_to_slave_put(pty_pair_t *p, char c);
static int	pty_to_slave_get(pty_pair_t *p, char *c);
static int	pty_to_master_put(pty_pair_t *p, char c);
static int	pty_to_master_get(pty_pair_t *p, char *c);
static int	pty_slave_stat(vnode_t *vn, posix_stat_t *st);
static int	pty_master_stat(vnode_t *vn, posix_stat_t *st);
static int	vnode_pty_master_read(vnode_t *vn, void *buf, u64 count,
    u64 offset);
static int	vnode_pty_master_write(vnode_t *vn, const void *buf, u64 count,
    u64 offset);
static int	vnode_pty_slave_read(vnode_t *vn, void *buf, u64 count,
    u64 offset);
static int	vnode_pty_slave_write(vnode_t *vn, const void *buf, u64 count,
    u64 offset);

static void
pty_default_termios(struct termios *t)
{
	memset(t, 0, sizeof(*t));
	t->c_iflag = ICRNL | IXON;
	t->c_oflag = OPOST | ONLCR;
	t->c_cflag = CREAD | CS8;
	t->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
	t->c_line = 0;
	t->c_cc[VINTR] = CINTR;
	t->c_cc[VQUIT] = CQUIT;
	t->c_cc[VERASE] = CERASE;
	t->c_cc[VKILL] = CKILL;
	t->c_cc[VEOF] = CEOF;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
	t->c_ispeed = B38400;
	t->c_ospeed = B38400;
}

static pty_pair_t *
pty_alloc(void)
{
	pty_pair_t	*p;
	int		 i;

	for (i = 0; i < PTY_COUNT; i++) {
		p = &pty_pairs[i];
		if (p->id == 0) {
			memset(p, 0, sizeof(*p));
			p->id = i + 1;
			if (entity_is_initialized()) {
				char	name[64];

				p->entity = entity_create(ENTITY_ARCH_PTY, 0,
				    0, 0, 0, 0, 0, 1);
				if (p->entity != 0) {
					snprintf(name, sizeof(name),
					    "/Entity/Pty/%d", p->id);
					entity_ns_bind(name, p->entity);
				}
			}
			pty_default_termios(&p->term);
			p->ws.ws_row = 24;
			p->ws.ws_col = 80;
			p->ws.ws_xpixel = 0;
			p->ws.ws_ypixel = 0;
			p->locked = 0;
			p->open_master = 0;
			p->open_slave = 0;
			p->session = 0;
			p->foreground_pgrp = 0;
			return (p);
		}
	}
	return (NULL);
}

static void
pty_signal_pgrp(pty_pair_t *p, int sig)
{
	process_t	*proc;
	int		 i;

	if (p->foreground_pgrp == 0) {
		return;
	}
	for (i = 0; i < MAX_PROCESSES; i++) {
		proc = &process_table[i];
		if (proc->pid != 0 && proc->pgid == p->foreground_pgrp) {
			process_send_signal(proc->pid, sig);
		}
	}
}

static int
pty_to_slave_put(pty_pair_t *p, char c)
{
	int	next;

	next = (p->to_slave_head + 1) % PTY_BUF_SIZE;
	if (next == p->to_slave_tail) {
		return (-1);
	}
	p->to_slave[p->to_slave_head] = c;
	p->to_slave_head = next;
	p->to_slave_count++;
	proc_wakeup(&p->to_slave_count);
	knote_notify_all(EVFILT_READ, 0, 0, 1);
	posix_poll_notify();
	return (0);
}

static int
pty_to_slave_get(pty_pair_t *p, char *c)
{
	if (p->to_slave_count == 0) {
		return (-1);
	}
	*c = p->to_slave[p->to_slave_tail];
	p->to_slave_tail = (p->to_slave_tail + 1) % PTY_BUF_SIZE;
	p->to_slave_count--;
	proc_wakeup(&p->to_slave_count);
	posix_poll_notify();
	return (0);
}

static int
pty_to_master_put(pty_pair_t *p, char c)
{
	int	next;

	next = (p->to_master_head + 1) % PTY_BUF_SIZE;
	if (next == p->to_master_tail) {
		return (-1);
	}
	p->to_master[p->to_master_head] = c;
	p->to_master_head = next;
	p->to_master_count++;
	proc_wakeup(&p->to_master_count);
	knote_notify_all(EVFILT_READ, 0, 0, 1);
	posix_poll_notify();
	return (0);
}

static int
pty_to_master_get(pty_pair_t *p, char *c)
{
	if (p->to_master_count == 0) {
		return (-1);
	}
	*c = p->to_master[p->to_master_tail];
	p->to_master_tail = (p->to_master_tail + 1) % PTY_BUF_SIZE;
	p->to_master_count--;
	proc_wakeup(&p->to_master_count);
	posix_poll_notify();
	return (0);
}

static int
pty_is_master(vnode_t *vn)
{
	return (vn && vn->ioctl_fn == pty_master_ioctl);
}

static int
pty_output_count(pty_pair_t *p, vnode_t *vn)
{
	if (pty_is_master(vn)) {
		return (p->to_slave_count);
	}
	return (p->to_master_count);
}

static void *
pty_output_channel(pty_pair_t *p, vnode_t *vn)
{
	if (pty_is_master(vn)) {
		return (&p->to_slave_count);
	}
	return (&p->to_master_count);
}

static void
pty_clear_to_slave(pty_pair_t *p)
{
	p->to_slave_head = 0;
	p->to_slave_tail = 0;
	p->to_slave_count = 0;
	proc_wakeup(&p->to_slave_count);
	posix_poll_notify();
}

static void
pty_clear_to_master(pty_pair_t *p)
{
	p->to_master_head = 0;
	p->to_master_tail = 0;
	p->to_master_count = 0;
	proc_wakeup(&p->to_master_count);
	posix_poll_notify();
}

static void
pty_flush_input(pty_pair_t *p, vnode_t *vn)
{
	if (pty_is_master(vn)) {
		pty_clear_to_master(p);
	} else {
		pty_clear_to_slave(p);
	}
}

static void
pty_flush_output(pty_pair_t *p, vnode_t *vn)
{
	if (pty_is_master(vn)) {
		pty_clear_to_slave(p);
	} else {
		pty_clear_to_master(p);
	}
}

static int
pty_drain_output(pty_pair_t *p, vnode_t *vn)
{
	struct process	*proc;
	void		*channel;

	channel = pty_output_channel(p, vn);
	while (pty_output_count(p, vn) > 0) {
		proc = process_current();
		if (proc && (proc->sigpending & ~proc->sigmask)) {
			return (-POSIX_EINTR);
		}
		proc_sleep(channel);
	}
	return (0);
}

int
pty_init(void)
{
	if (pty_initialized) {
		return (0);
	}
	memset(pty_pairs, 0, sizeof(pty_pairs));
	devfs_register("ptmx", DEVFS_DEV_PTMX, NULL, NULL, NULL, NULL,
	    NULL);
	pty_initialized = 1;
	return (0);
}

int
pty_open_master(vnode_t **out)
{
	pty_pair_t	*p;
	vnode_t		*vn;
	char		 name[32];

	if (!out) {
		return (-POSIX_EINVAL);
	}
	p = pty_alloc();
	if (!p) {
		return (-POSIX_ENOMEM);
	}
	p->open_master = 1;

	vn = vnode_alloc(VCHR, "ptm");
	if (!vn) {
		p->id = 0;
		p->open_master = 0;
		return (-POSIX_ENOMEM);
	}
	vn->data = p;
	vn->read_fn = vnode_pty_master_read;
	vn->write_fn = vnode_pty_master_write;
	vn->ioctl_fn = pty_master_ioctl;
	vn->stat_fn = pty_master_stat;
	vn->readdir_fn = NULL;

	snprintf(name, sizeof(name), "pts/%d", p->id - 1);
	if (devfs_register(name, DEVFS_DEV_PTS, vnode_pty_slave_read,
	    vnode_pty_slave_write, pty_slave_ioctl, pty_slave_stat, p) != 0) {
		vn->data = NULL;
		vn->read_fn = NULL;
		vn->write_fn = NULL;
		vn->ioctl_fn = NULL;
		vn->stat_fn = NULL;
		vnode_release(vn);
		p->id = 0;
		p->open_master = 0;
		return (-POSIX_ENOMEM);
	}

	*out = vn;
	return (0);
}

int
pty_name(int id, char *buf, int len)
{
	if (id <= 0 || id > PTY_COUNT || !buf || len <= 0) {
		return (-1);
	}
	if (pty_pairs[id - 1].id != id) {
		return (-1);
	}
	snprintf(buf, (unsigned long)len, "/dev/pts/%d", id - 1);
	return (0);
}

int
pty_master_read(vnode_t *vn, void *buf, u32 count, int nonblock)
{
	pty_pair_t	*p;
	char		*out;
	char		 c;
	int		 n;

	p = (pty_pair_t *)vn->data;
	if (!p || p->id == 0) {
		return (-POSIX_EBADF);
	}
	out = (char *)buf;
	n = 0;
	while (n < (int)count) {
		if (pty_to_master_get(p, &c) == 0) {
			out[n++] = c;
		} else if (nonblock) {
			if (n == 0) {
				return (-POSIX_EAGAIN);
			}
			break;
		} else {
			proc_sleep(&p->to_master_count);
		}
	}
	return (n);
}

int
pty_master_write(vnode_t *vn, const void *buf, u32 count, int nonblock)
{
	pty_pair_t	*p;
	const char	*data;
	int		 i;

	p = (pty_pair_t *)vn->data;
	if (!p || p->id == 0) {
		return (-POSIX_EBADF);
	}
	data = (const char *)buf;
	for (i = 0; i < (int)count; i++) {
		while (pty_to_slave_put(p, data[i]) != 0) {
			if (nonblock) {
				if (i == 0) {
					return (-POSIX_EAGAIN);
				}
				return (i);
			}
			proc_sleep(&p->to_slave_count);
		}
	}
	return ((int)count);
}

static int
pty_terminal_ioctl(vnode_t *vn, u64 cmd, void *arg)
{
	pty_pair_t	*p;
	struct process	*proc;
	struct termios	t;
	struct winsize	ws;
	int		pgrp;
	int		queue;
	int		avail;
	int		ret;
	u32		sid;

	p = (pty_pair_t *)vn->data;
	if (!p || p->id == 0) {
		return (-POSIX_EBADF);
	}
	proc = process_current();

	switch (cmd) {
	case POSIX_TIOCGWINSZ:
		if (!arg || !user_range_fault_in(arg, sizeof(ws), 1)) {
			return (-POSIX_EFAULT);
		}
		ws = p->ws;
		memcpy(arg, &ws, sizeof(ws));
		return (0);

	case POSIX_TIOCSWINSZ:
		if (!arg || !user_range_fault_in(arg, sizeof(ws), 0)) {
			return (-POSIX_EFAULT);
		}
		memcpy(&ws, arg, sizeof(ws));
		p->ws = ws;
		pty_signal_pgrp(p, SIGWINCH);
		return (0);

	case POSIX_TCGETS:
		if (!arg || !user_range_fault_in(arg, sizeof(t), 1)) {
			return (-POSIX_EFAULT);
		}
		t = p->term;
		memcpy(arg, &t, sizeof(t));
		return (0);

	case POSIX_TCSETS:
	case POSIX_TCSETSW:
	case POSIX_TCSETSF:
		if (!arg || !user_range_fault_in(arg, sizeof(t), 0)) {
			return (-POSIX_EFAULT);
		}
		if (cmd == POSIX_TCSETSW || cmd == POSIX_TCSETSF) {
			ret = pty_drain_output(p, vn);
			if (ret != 0) {
				return (ret);
			}
		}
		memcpy(&t, arg, sizeof(t));
		p->term = t;
		if (cmd == POSIX_TCSETSF) {
			pty_flush_input(p, vn);
		}
		return (0);

	case POSIX_TCFLSH:
		queue = (int)(unsigned long)arg;
		if (queue != POSIX_TCIFLUSH && queue != POSIX_TCOFLUSH &&
		    queue != POSIX_TCIOFLUSH) {
			return (-POSIX_EINVAL);
		}
		if (queue == POSIX_TCIFLUSH || queue == POSIX_TCIOFLUSH) {
			pty_flush_input(p, vn);
		}
		if (queue == POSIX_TCOFLUSH || queue == POSIX_TCIOFLUSH) {
			pty_flush_output(p, vn);
		}
		return (0);

	case POSIX_FIONREAD:
		if (!arg || !user_range_fault_in(arg, sizeof(avail), 1)) {
			return (-POSIX_EFAULT);
		}
		avail = pty_read_available(vn);
		memcpy(arg, &avail, sizeof(avail));
		return (0);

	case POSIX_TIOCGPGRP:
		if (!arg || !user_range_fault_in(arg, sizeof(pgrp), 1)) {
			return (-POSIX_EFAULT);
		}
		pgrp = (int)p->foreground_pgrp;
		memcpy(arg, &pgrp, sizeof(pgrp));
		return (0);

	case POSIX_TIOCSPGRP:
		if (!arg || !user_range_fault_in(arg, sizeof(pgrp), 0)) {
			return (-POSIX_EFAULT);
		}
		memcpy(&pgrp, arg, sizeof(pgrp));
		p->foreground_pgrp = (u32)pgrp;
		return (0);

	case POSIX_TIOCGSID:
		if (!arg || !user_range_fault_in(arg, sizeof(sid), 1)) {
			return (-POSIX_EFAULT);
		}
		sid = p->session;
		memcpy(arg, &sid, sizeof(sid));
		return (0);

	case POSIX_TIOCSCTTY:
		if (pty_is_master(vn)) {
			return (-POSIX_ENOTTY);
		}
		if (!proc) {
			return (-POSIX_EFAULT);
		}
		if (!proc->is_session_leader) {
			return (-POSIX_EPERM);
		}
		if (p->session != 0 && p->session != proc->sid) {
			return (-POSIX_EPERM);
		}
		p->session = proc->sid;
		proc->controlling_tty = -2 - p->id;
		return (0);

	default:
		return (-POSIX_ENOTTY);
	}
}

int
pty_master_ioctl(vnode_t *vn, u64 cmd, void *arg)
{
	pty_pair_t	*p;
	int		 n;

	p = (pty_pair_t *)vn->data;
	if (!p || p->id == 0) {
		return (-POSIX_EBADF);
	}

	switch (cmd) {
	case POSIX_TIOCGPTN:
		if (!arg || !user_range_fault_in(arg, sizeof(n), 1)) {
			return (-POSIX_EFAULT);
		}
		n = p->id - 1;
		memcpy(arg, &n, sizeof(n));
		return (0);
	default:
		return (pty_terminal_ioctl(vn, cmd, arg));
	}
}

int
pty_slave_read(vnode_t *vn, void *buf, u32 count, int nonblock)
{
	pty_pair_t	*p;
	char		*out;
	char		 c;
	int		 n;

	p = (pty_pair_t *)vn->data;
	if (!p || p->id == 0) {
		return (-POSIX_EBADF);
	}
	out = (char *)buf;
	n = 0;
	while (n < (int)count) {
		if (pty_to_slave_get(p, &c) == 0) {
			out[n++] = c;
		} else if (nonblock) {
			if (n == 0) {
				return (-POSIX_EAGAIN);
			}
			break;
		} else {
			proc_sleep(&p->to_slave_count);
		}
	}
	return (n);
}

int
pty_slave_write(vnode_t *vn, const void *buf, u32 count, int nonblock)
{
	pty_pair_t	*p;
	const char	*data;
	int		 i;

	p = (pty_pair_t *)vn->data;
	if (!p || p->id == 0) {
		return (-POSIX_EBADF);
	}
	data = (const char *)buf;
	for (i = 0; i < (int)count; i++) {
		if ((p->term.c_oflag & (OPOST | ONLCR)) == (OPOST | ONLCR) &&
		    data[i] == '\n') {
			while (pty_to_master_put(p, '\r') != 0) {
				if (nonblock) {
					if (i == 0) {
						return (-POSIX_EAGAIN);
					}
					return (i);
				}
				proc_sleep(&p->to_master_count);
			}
		}
		while (pty_to_master_put(p, data[i]) != 0) {
			if (nonblock) {
				if (i == 0) {
					return (-POSIX_EAGAIN);
				}
				return (i);
			}
			proc_sleep(&p->to_master_count);
		}
	}
	return ((int)count);
}

int
pty_slave_ioctl(vnode_t *vn, u64 cmd, void *arg)
{
	pty_pair_t	*p;

	p = (pty_pair_t *)vn->data;
	if (!p || p->id == 0) {
		return (-POSIX_EBADF);
	}
	return (pty_terminal_ioctl(vn, cmd, arg));
}

int
pty_read_available(vnode_t *vn)
{
	pty_pair_t	*p;

	if (!vn || !vn->data) {
		return (0);
	}
	p = (pty_pair_t *)vn->data;
	if (p->id == 0) {
		return (0);
	}
	if (vn->ioctl_fn == pty_master_ioctl) {
		return (p->to_master_count);
	}
	if (vn->ioctl_fn == pty_slave_ioctl) {
		return (p->to_slave_count);
	}
	return (0);
}

int
pty_write_available(vnode_t *vn)
{
	pty_pair_t	*p;
	int		 count;

	if (!vn || !vn->data) {
		return (0);
	}
	p = (pty_pair_t *)vn->data;
	if (p->id == 0) {
		return (0);
	}
	count = pty_output_count(p, vn);
	if (count >= PTY_BUF_SIZE - 1) {
		return (0);
	}
	return (PTY_BUF_SIZE - 1 - count);
}

vnode_t *
pty_create_slave_vnode(int id)
{
	pty_pair_t	*p;
	vnode_t		*vn;

	if (id <= 0 || id > PTY_COUNT) {
		return (NULL);
	}
	p = &pty_pairs[id - 1];
	if (p->id != id) {
		return (NULL);
	}
	vn = vnode_alloc(VCHR, "tty");
	if (!vn) {
		return (NULL);
	}
	vn->data = p;
	vn->read_fn = vnode_pty_slave_read;
	vn->write_fn = vnode_pty_slave_write;
	vn->ioctl_fn = pty_slave_ioctl;
	vn->stat_fn = pty_slave_stat;
	vn->readdir_fn = NULL;
	return (vn);
}

static int
pty_slave_stat(vnode_t *vn, posix_stat_t *st)
{
	pty_pair_t	*p;

	(void)vn;
	p = (pty_pair_t *)vn->data;
	(void)p;
	memset(st, 0, sizeof(posix_stat_t));
	st->st_mode = POSIX_S_IFCHR | 0620;
	st->st_size = 0;
	st->st_blksize = 0;
	st->st_blocks = 0;
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	return (0);
}

static int
pty_master_stat(vnode_t *vn, posix_stat_t *st)
{
	(void)vn;
	memset(st, 0, sizeof(posix_stat_t));
	st->st_mode = POSIX_S_IFCHR | 0666;
	st->st_size = 0;
	st->st_blksize = 0;
	st->st_blocks = 0;
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	return (0);
}

static int
vnode_pty_master_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	(void)offset;
	return (pty_master_read(vn, buf, (u32)count, 0));
}

static int
vnode_pty_master_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	(void)offset;
	return (pty_master_write(vn, buf, (u32)count, 0));
}

static int
vnode_pty_slave_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	(void)offset;
	return (pty_slave_read(vn, buf, (u32)count, 0));
}

static int
vnode_pty_slave_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	(void)offset;
	return (pty_slave_write(vn, buf, (u32)count, 0));
}

void
pty_set_session_pgrp(int id, u32 sid, u32 pgid)
{
	pty_pair_t	*p;

	if (id <= 0 || id > PTY_COUNT) {
		return;
	}
	p = &pty_pairs[id - 1];
	if (p->id != id) {
		return;
	}
	p->session = sid;
	p->foreground_pgrp = pgid;
}

void
pty_get_winsize(int id, struct winsize *ws)
{
	pty_pair_t	*p;

	if (!ws) {
		return;
	}
	if (id <= 0 || id > PTY_COUNT) {
		memset(ws, 0, sizeof(*ws));
		ws->ws_row = 24;
		ws->ws_col = 80;
		return;
	}
	p = &pty_pairs[id - 1];
	if (p->id != id) {
		memset(ws, 0, sizeof(*ws));
		ws->ws_row = 24;
		ws->ws_col = 80;
		return;
	}
	*ws = p->ws;
}

void
pty_set_winsize(int id, const struct winsize *ws)
{
	pty_pair_t	*p;

	if (!ws || id <= 0 || id > PTY_COUNT) {
		return;
	}
	p = &pty_pairs[id - 1];
	if (p->id != id) {
		return;
	}
	p->ws = *ws;
	pty_signal_pgrp(p, SIGWINCH);
}

void
pty_get_termios(int id, struct termios *t)
{
	pty_pair_t	*p;

	if (!t) {
		return;
	}
	if (id <= 0 || id > PTY_COUNT) {
		memset(t, 0, sizeof(*t));
		return;
	}
	p = &pty_pairs[id - 1];
	if (p->id != id) {
		memset(t, 0, sizeof(*t));
		return;
	}
	*t = p->term;
}

void
pty_set_termios(int id, const struct termios *t)
{
	pty_pair_t	*p;

	if (!t || id <= 0 || id > PTY_COUNT) {
		return;
	}
	p = &pty_pairs[id - 1];
	if (p->id != id) {
		return;
	}
	p->term = *t;
}

int
pty_slave_read_idx(int id, void *buf, u32 count, int nonblock)
{
	pty_pair_t	*p;
	char		*out;
	char		 c;
	int		 n;

	if (id <= 0 || id > PTY_COUNT || !buf) {
		return (-API_ERR_BAD_HANDLE);
	}
	p = &pty_pairs[id - 1];
	if (p->id != id) {
		return (-API_ERR_BAD_HANDLE);
	}
	out = (char *)buf;
	n = 0;
	while (n < (int)count) {
		if (pty_to_slave_get(p, &c) == 0) {
			out[n++] = c;
		} else if (nonblock) {
			if (n == 0) {
				return (-API_ERR_BUSY);
			}
			break;
		} else {
			proc_sleep(&p->to_slave_count);
		}
	}
	return (n);
}

int
pty_slave_write_idx(int id, const void *buf, u32 count, int nonblock)
{
	pty_pair_t	*p;
	const char	*data;
	int		 i;

	if (id <= 0 || id > PTY_COUNT || !buf) {
		return (-API_ERR_BAD_HANDLE);
	}
	p = &pty_pairs[id - 1];
	if (p->id != id) {
		return (-API_ERR_BAD_HANDLE);
	}
	data = (const char *)buf;
	for (i = 0; i < (int)count; i++) {
		if ((p->term.c_oflag & (OPOST | ONLCR)) == (OPOST | ONLCR) &&
		    data[i] == '\n') {
			while (pty_to_master_put(p, '\r') != 0) {
				if (nonblock) {
					if (i == 0) {
						return (-API_ERR_BUSY);
					}
					return (i);
				}
				proc_sleep(&p->to_master_count);
			}
		}
		while (pty_to_master_put(p, data[i]) != 0) {
			if (nonblock) {
				if (i == 0) {
					return (-API_ERR_BUSY);
				}
				return (i);
			}
			proc_sleep(&p->to_master_count);
		}
	}
	return ((int)count);
}
