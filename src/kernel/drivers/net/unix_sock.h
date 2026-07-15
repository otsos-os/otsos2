#ifndef KERNEL_DRIVERS_NET_UNIX_SOCK_H
#define KERNEL_DRIVERS_NET_UNIX_SOCK_H

#include <kernel/drivers/fs/vfs/vfs.h>
#include <mlibc/mlibc.h>

#define	AF_UNSPEC	0
#define	AF_UNIX		1
#define	AF_LOCAL	AF_UNIX

#define	SOCK_STREAM	1
#define	SOCK_DGRAM	2

#define	SHUT_RD		0
#define	SHUT_WR		1
#define	SHUT_RDWR	2

#define	SOL_SOCKET	1
#define	SO_TYPE		3
#define	SO_ERROR	4

#define	SOMAXCONN	8

typedef u32 socklen_t;

struct sockaddr {
	u16	sa_family;
	char	sa_data[14];
} __attribute__((packed));

struct sockaddr_un {
	u16	sun_family;
	char	sun_path[108];
} __attribute__((packed));

typedef struct unix_sock unix_sock_t;

/* Lifecycle */
unix_sock_t	*unix_sock_alloc(int domain, int type, int proto);
void		unix_sock_hold(unix_sock_t *s);
void		unix_sock_put(unix_sock_t *s);

/* Vnode callbacks for posix_io.c / vfs */
int		unix_sock_vnode_read(vnode_t *vn, void *buf, u64 count,
		    u64 offset);
int		unix_sock_vnode_write(vnode_t *vn, const void *buf,
		    u64 count, u64 offset);
void		unix_sock_vnode_close(vnode_t *vn);
void		unix_sock_vnode_hold(vnode_t *vn);
int		unix_sock_vnode_stat(vnode_t *vn, posix_stat_t *st);

/* Bind / path table */
int		unix_sock_bind(unix_sock_t *s, const char *path);
void		unix_sock_unbind(unix_sock_t *s);
unix_sock_t	*unix_sock_find_by_path(const char *path);

/* Connect / listen / accept */
int		unix_sock_listen(unix_sock_t *s);
int		unix_sock_connect_stream(unix_sock_t *c,
		    unix_sock_t *target);
unix_sock_t	*unix_sock_accept_dequeue(unix_sock_t *s,
		    int nonblock);

/* Stream I/O */
int	unix_sock_stream_read(unix_sock_t *s, void *buf, u32 count,
	    int nonblock);
int	unix_sock_stream_write(unix_sock_t *s, const void *buf,
	    u32 count, int nonblock);

/* Dgram I/O */
int	unix_sock_dgram_sendto(unix_sock_t *s, const void *buf,
	    u32 len, const char *path);
int	unix_sock_dgram_recvfrom(unix_sock_t *s, void *buf, u32 len,
	    char *from_path, u32 *from_len, int nonblock);

/* Shutdown */
void	unix_sock_shutdown(unix_sock_t *s, int how);

/* Socket options */
int	unix_sock_get_type(unix_sock_t *s);
int	unix_sock_get_error(unix_sock_t *s);

/* Address info */
int	unix_sock_getsockname(unix_sock_t *s, char *path,
	    int *path_len);
int	unix_sock_getpeername(unix_sock_t *s, char *path,
	    int *path_len);

/* Pair creation */
int	unix_sock_pair(unix_sock_t *a, unix_sock_t *b);

#endif
