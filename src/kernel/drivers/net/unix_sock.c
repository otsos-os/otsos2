#include <kernel/drivers/net/unix_sock.h>
#include <kernel/api/errno.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/api/posix/posix.h>
#include <kernel/process.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

static unix_sock_t	unix_socks[UNIX_SOCK_MAX];

static int
unix_sock_api_ret(int ret)
{
	int	err;

	if (ret >= 0) {
		return (ret);
	}

	err = -ret;
	switch (err) {
	case API_ERR_EXISTS:
		return (-POSIX_EADDRINUSE);
	case API_ERR_NOT_FOUND:
		return (-POSIX_ENOENT);
	case API_ERR_NOT_DIR:
		return (-POSIX_ENOTDIR);
	case API_ERR_TOO_BIG:
		return (-POSIX_ENAMETOOLONG);
	case API_ERR_NO_MEMORY:
	case API_ERR_NOMEM:
		return (-POSIX_ENOMEM);
	case API_ERR_NO_SPACE:
		return (-POSIX_ENOSPC);
	case API_ERR_ACCESS:
	case API_ERR_PERM:
		return (-POSIX_EACCES);
	case API_ERR_BAD_VALUE:
	case API_ERR_INVAL:
		return (-POSIX_EINVAL);
	default:
		return (-POSIX_EIO);
	}
}

static int
sock_alloc_id(void)
{
	static int next_id = 1;
	int id;

	id = next_id++;
	if (id <= 0)
		id = 1;
	return (id);
}

unix_sock_t *
unix_sock_alloc(int domain, int type, int protocol)
{
	unix_sock_t	*s;
	int		i;

	for (i = 0; i < UNIX_SOCK_MAX; i++) {
		s = &unix_socks[i];
		if (s->state == UNIX_SOCK_FREE) {
			memset(s, 0, sizeof(*s));
			s->id = sock_alloc_id();
			s->state = UNIX_SOCK_UNBOUND;
			s->domain = domain;
			s->type = type;
			s->protocol = protocol;
			s->refcount = 1;

			struct process *proc = process_current();
			if (proc) {
				s->uid = proc->euid;
				s->gid = proc->egid;
				s->pid = proc->pid;
			}
			return (s);
		}
	}
	return (NULL);
}

void
unix_sock_hold(unix_sock_t *s)
{
	if (s)
		s->refcount++;
}

static void
sock_cleanup(unix_sock_t *s)
{
	dgram_msg_t	*msg, *next;

	if (s->peer) {
		s->peer->peer = NULL;
		proc_wakeup(&s->peer->read_wait);
		proc_wakeup(&s->peer->write_wait);
		s->peer = NULL;
	}

	msg = s->msg_head;
	while (msg) {
		next = msg->next;
		kmem_free(msg);
		msg = next;
	}
	s->msg_head = NULL;
	s->msg_tail = NULL;
	s->msg_count = 0;

	if (s->bound_path[0] != '\0')
		unix_sock_unbind(s);
}

void
unix_sock_put(unix_sock_t *s)
{
	if (!s)
		return;
	s->refcount--;
	if (s->refcount > 0)
		return;

	sock_cleanup(s);
	s->state = UNIX_SOCK_FREE;
	memset(s, 0, sizeof(*s));
}

unix_sock_t *
unix_sock_find_by_path(const char *path)
{
	unix_sock_t	*s;
	int		i;

	for (i = 0; i < UNIX_SOCK_MAX; i++) {
		s = &unix_socks[i];
		if (s->state != UNIX_SOCK_FREE &&
		    s->state != UNIX_SOCK_CLOSED &&
		    s->bound_path[0] != '\0' &&
		    strcmp(s->bound_path, path) == 0)
			return (s);
	}
	return (NULL);
}

int
unix_sock_bind(unix_sock_t *s, const char *path)
{
	int	len, ret;

	if (!path) {
		return (-POSIX_EINVAL);
	}
	len = strlen(path);
	if (len <= 0 || len >= 108)
		return (-POSIX_EINVAL);

	if (unix_sock_find_by_path(path) != NULL)
		return (-POSIX_EADDRINUSE);

	strcpy(s->bound_path, path);

	ret = chainfs_create_socket(path);
	if (ret != 0) {
		s->bound_path[0] = '\0';
		return (unix_sock_api_ret(ret));
	}

	s->state = UNIX_SOCK_BOUND;
	return (0);
}

void
unix_sock_unbind(unix_sock_t *s)
{
	if (s->bound_path[0] != '\0') {
		chainfs_delete_file(s->bound_path);
		s->bound_path[0] = '\0';
	}
}

int
unix_sock_listen(unix_sock_t *s)
{
	if (s->type != SOCK_STREAM)
		return (-POSIX_EOPNOTSUPP);
	if (s->state != UNIX_SOCK_BOUND)
		return (-POSIX_EINVAL);

	s->state = UNIX_SOCK_LISTENING;
	return (0);
}

int
unix_sock_connect_stream(unix_sock_t *c, unix_sock_t *target)
{
	unix_sock_t	*n;
	int		slot;

	n = unix_sock_alloc(AF_UNIX, SOCK_STREAM, 0);
	if (!n)
		return (-POSIX_ENOMEM);

	if (target->accept_count >= UNIX_SOCK_BACKLOG) {
		sock_cleanup(n);
		n->state = UNIX_SOCK_FREE;
		return (-POSIX_ECONNREFUSED);
	}

	c->peer = n;
	n->peer = c;
	c->state = UNIX_SOCK_CONNECTED;
	n->state = UNIX_SOCK_CONNECTED;

	slot = (target->accept_head + target->accept_count) %
	    UNIX_SOCK_BACKLOG;
	target->accept_queue[slot] = (int)(n - unix_socks);
	target->accept_count++;
	unix_sock_hold(n);

	proc_wakeup(&target->accept_wait);
	return (0);
}

unix_sock_t *
unix_sock_accept_dequeue(unix_sock_t *s, int nonblock)
{
	unix_sock_t	*n;
	int		slot;

	if (s->state != UNIX_SOCK_LISTENING)
		return (NULL);

	while (s->accept_count == 0) {
		if (nonblock)
			return (NULL);
		proc_sleep(&s->accept_wait);
		if (s->state != UNIX_SOCK_LISTENING)
			return (NULL);
	}

	slot = s->accept_tail;
	n = &unix_socks[s->accept_queue[slot]];
	s->accept_tail = (s->accept_tail + 1) % UNIX_SOCK_BACKLOG;
	s->accept_count--;
	unix_sock_put(n);

	return (n);
}

int
unix_sock_stream_read(unix_sock_t *s, void *buf, u32 count, int nonblock)
{
	char	*out;
	int	n;

	if (!s || s->state == UNIX_SOCK_FREE)
		return (-POSIX_EBADF);
	if (s->shut_rd)
		return (0);
	if (!s->peer || s->peer->state == UNIX_SOCK_FREE ||
	    s->peer->state == UNIX_SOCK_CLOSED)
		return (0);

	out = (char *)buf;
	n = 0;

	while (n < (int)count) {
		if (s->stream_count > 0) {
			out[n++] = s->stream_buf[s->stream_tail];
			s->stream_tail = (s->stream_tail + 1) %
			    UNIX_SOCK_BUF_SIZE;
			s->stream_count--;
			proc_wakeup(&s->peer->write_wait);
		} else if (nonblock) {
			if (n == 0)
				return (-POSIX_EAGAIN);
			break;
		} else if (s->peer->shut_wr ||
		    s->peer->state == UNIX_SOCK_CLOSED ||
		    s->peer->state == UNIX_SOCK_FREE) {
			if (n == 0)
				return (0);
			break;
		} else {
			proc_sleep(&s->read_wait);
		}
	}
	return (n);
}

int
unix_sock_stream_write(unix_sock_t *s, const void *buf, u32 count,
    int nonblock)
{
	const char	*data;
	int		i;

	if (!s || s->state == UNIX_SOCK_FREE)
		return (-POSIX_EBADF);
	if (s->shut_wr)
		return (-POSIX_EPIPE);
	if (!s->peer || s->peer->state == UNIX_SOCK_FREE ||
	    s->peer->state == UNIX_SOCK_CLOSED)
		return (-POSIX_EPIPE);

	data = (const char *)buf;
	for (i = 0; i < (int)count; i++) {
		int	next;

		next = (s->peer->stream_head + 1) % UNIX_SOCK_BUF_SIZE;
		while (next == s->peer->stream_tail) {
			if (nonblock) {
				if (i == 0)
					return (-POSIX_EAGAIN);
				return (i);
			}
			proc_sleep(&s->write_wait);
			if (s->shut_wr)
				return (-POSIX_EPIPE);
			if (!s->peer ||
			    s->peer->state == UNIX_SOCK_FREE ||
			    s->peer->state == UNIX_SOCK_CLOSED)
				return (-POSIX_EPIPE);
			next = (s->peer->stream_head + 1) %
			    UNIX_SOCK_BUF_SIZE;
		}
		s->peer->stream_buf[s->peer->stream_head] = data[i];
		s->peer->stream_head = next;
		s->peer->stream_count++;
		proc_wakeup(&s->peer->read_wait);
	}
	return ((int)count);
}

int
unix_sock_dgram_sendto(unix_sock_t *s, const void *buf, u32 len,
    const char *dest_path)
{
	unix_sock_t	*target;
	dgram_msg_t	*msg;

	if (!s || s->state == UNIX_SOCK_FREE)
		return (-POSIX_EBADF);
	if (!dest_path || dest_path[0] == '\0')
		return (-POSIX_EDESTADDRREQ);

	target = unix_sock_find_by_path(dest_path);
	if (!target)
		return (-POSIX_ECONNREFUSED);
	if (target->type != SOCK_DGRAM)
		return (-POSIX_EPROTOTYPE);

	msg = (dgram_msg_t *)kmem_alloc(sizeof(dgram_msg_t) + len);
	if (!msg)
		return (-POSIX_ENOBUFS);

	msg->next = NULL;
	msg->len = (int)len;
	memcpy(msg->data, buf, len);
	if (s->bound_path[0])
		strcpy(msg->from_path, s->bound_path);
	else
		msg->from_path[0] = '\0';

	if (target->msg_tail)
		target->msg_tail->next = msg;
	else
		target->msg_head = msg;
	target->msg_tail = msg;
	target->msg_count++;

	proc_wakeup(&target->read_wait);
	return ((int)len);
}

int
unix_sock_dgram_recvfrom(unix_sock_t *s, void *buf, u32 len,
    char *from_path, u32 *from_len, int nonblock)
{
	dgram_msg_t	*msg;
	int		copy_len;

	if (!s || s->state == UNIX_SOCK_FREE)
		return (-POSIX_EBADF);
	if (s->shut_rd)
		return (0);

	while (!s->msg_head) {
		if (nonblock)
			return (-POSIX_EAGAIN);
		proc_sleep(&s->read_wait);
		if (s->shut_rd)
			return (0);
	}

	msg = s->msg_head;
	s->msg_head = msg->next;
	if (!s->msg_head)
		s->msg_tail = NULL;
	s->msg_count--;

	copy_len = msg->len;
	if (copy_len > (int)len)
		copy_len = (int)len;
	memcpy(buf, msg->data, copy_len);

	if (from_path && from_len) {
		int flen = strlen(msg->from_path);
		if (flen >= (int)*from_len)
			flen = (int)*from_len - 1;
		if (flen > 0) {
			memcpy(from_path, msg->from_path, flen);
			from_path[flen] = '\0';
		} else
			from_path[0] = '\0';
		*from_len = (u32)flen;
	}

	kmem_free(msg);
	return (copy_len);
}

int
unix_sock_pair(unix_sock_t *a, unix_sock_t *b)
{
	if (!a || !b)
		return (-POSIX_EINVAL);

	a->peer = b;
	b->peer = a;
	a->state = UNIX_SOCK_CONNECTED;
	b->state = UNIX_SOCK_CONNECTED;

	return (0);
}

static int
vnode_read_wrapper(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	(void)offset;
	unix_sock_t *s = (unix_sock_t *)vn->data;
	if (s->type == SOCK_STREAM)
		return (unix_sock_stream_read(s, buf, (u32)count, 0));
	else
		return (unix_sock_dgram_recvfrom(s, buf, (u32)count,
		    NULL, NULL, 0));
}

static int
vnode_write_wrapper(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	(void)offset;
	unix_sock_t *s = (unix_sock_t *)vn->data;
	if (s->type == SOCK_STREAM)
		return (unix_sock_stream_write(s, buf, (u32)count, 0));
	else if (s->peer)
		return (unix_sock_dgram_sendto(s, buf, (u32)count,
		    s->peer->bound_path));
	return (-POSIX_EPIPE);
}

int
unix_sock_vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	return (vnode_read_wrapper(vn, buf, count, offset));
}

int
unix_sock_vnode_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	return (vnode_write_wrapper(vn, buf, count, offset));
}

void
unix_sock_vnode_close(vnode_t *vn)
{
	if (!vn || !vn->data)
		return;
	unix_sock_put((unix_sock_t *)vn->data);
	vn->data = NULL;
}

void
unix_sock_vnode_hold(vnode_t *vn)
{
	if (!vn || !vn->data)
		return;
	unix_sock_hold((unix_sock_t *)vn->data);
}

int
unix_sock_vnode_stat(vnode_t *vn, posix_stat_t *st)
{
	(void)vn;
	memset(st, 0, sizeof(posix_stat_t));
	st->st_mode = POSIX_S_IFSOCK | 0600;
	st->st_size = 0;
	st->st_blksize = 0;
	st->st_blocks = 0;
	st->st_nlink = 1;
	return (0);
}

int
unix_sock_get_type(unix_sock_t *s)
{
	return (s ? s->type : 0);
}

int
unix_sock_get_error(unix_sock_t *s)
{
	if (!s)
		return (0);
	int e = s->error;
	s->error = 0;
	return (e);
}

int
unix_sock_getsockname(unix_sock_t *s, char *path, int *path_len)
{
	if (!s || !path || !path_len)
		return (-POSIX_EINVAL);

	int len = strlen(s->bound_path);
	if (len >= *path_len)
		len = *path_len - 1;
	if (len > 0) {
		memcpy(path, s->bound_path, len);
		path[len] = '\0';
	}
	*path_len = len;
	return (0);
}

int
unix_sock_getpeername(unix_sock_t *s, char *path, int *path_len)
{
	if (!s || !s->peer)
		return (-POSIX_ENOTCONN);

	return (unix_sock_getsockname(s->peer, path, path_len));
}

void
unix_sock_shutdown(unix_sock_t *s, int how)
{
	if (!s)
		return;

	switch (how) {
	case SHUT_RD:
		s->shut_rd = 1;
		break;
	case SHUT_WR:
		s->shut_wr = 1;
		break;
	case SHUT_RDWR:
		s->shut_rd = 1;
		s->shut_wr = 1;
		break;
	}

	proc_wakeup(&s->read_wait);
	proc_wakeup(&s->write_wait);

	if (s->peer) {
		proc_wakeup(&s->peer->read_wait);
		proc_wakeup(&s->peer->write_wait);
	}
}
