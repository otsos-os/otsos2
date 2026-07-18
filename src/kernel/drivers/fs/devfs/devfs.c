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

#include <kernel/drivers/fs/devfs/devfs.h>
#include <kernel/console/terminal.h>
#include <kernel/console/pty.h>
#include <kernel/crypto/rng/rng.h>
#include <kernel/drivers/video/drm/fbdev.h>
#include <kernel/process.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

typedef struct devfs_node {
	char			name[32];
	int			device_id;
	int			(*read_fn)(vnode_t *, void *, u64, u64);
	int			(*write_fn)(vnode_t *, const void *, u64, u64);
	int			(*ioctl_fn)(vnode_t *, u64, void *);
	int			(*stat_fn)(vnode_t *, posix_stat_t *);
	void			*data;
	struct devfs_node	*next;
} devfs_node_t;

static devfs_node_t	*devfs_list = NULL;
static int		devfs_count = 0;

static int
vnode_simple_read(vnode_t *vn, void *buf, u64 count, u64 offset,
    int (*fn)(void *, u64))
{
	(void)vn;
	(void)offset;
	if (!fn) {
		return (-1);
	}
	return (fn(buf, count));
}

static int
vnode_simple_write(vnode_t *vn, const void *buf, u64 count, u64 offset,
    int (*fn)(const void *, u64))
{
	(void)vn;
	(void)offset;
	if (!fn) {
		return (-1);
	}
	return (fn(buf, count));
}

static int
vnode_simple_ioctl(vnode_t *vn, u64 cmd, void *arg)
{
	(void)vn;
	(void)cmd;
	(void)arg;
	return (-POSIX_ENOTTY);
}

static int
vnode_simple_stat(vnode_t *vn, posix_stat_t *st)
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
dev_null_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	(void)buf;
	(void)count;
	(void)vn;
	(void)offset;
	return (0);
}

static int
dev_null_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	(void)vn;
	(void)buf;
	(void)offset;
	return ((int)count);
}

static int
dev_zero_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	(void)vn;
	(void)offset;
	memset(buf, 0, (unsigned long)count);
	return ((int)count);
}

static int
dev_zero_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	(void)vn;
	(void)buf;
	(void)offset;
	return ((int)count);
}

static int
dev_random_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	(void)vn;
	(void)offset;
	if (count == 0) {
		return (0);
	}
	if (crypto_rng_bytes((u8 *)buf, (u32)count) != 0) {
		return (-1);
	}
	return ((int)count);
}

static int
dev_random_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	(void)vn;
	(void)offset;
	crypto_rng_add_entropy((const u8 *)buf, (u32)count);
	return ((int)count);
}

static int
dev_urandom_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	(void)vn;
	(void)offset;
	if (count == 0) {
		return (0);
	}
	if (crypto_rng_bytes((u8 *)buf, (u32)count) != 0) {
		return (-1);
	}
	return ((int)count);
}

static int
dev_urandom_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	(void)vn;
	(void)buf;
	(void)offset;
	return ((int)count);
}

static int
vnode_tty_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	(void)offset;
	return (terminal_read_vnode(vn, buf, (u32)count, 0, 0));
}

static int
vnode_tty_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	(void)offset;
	return (terminal_write_vnode(vn, buf, (u32)count));
}

static int
vnode_tty_ioctl(vnode_t *vn, u64 cmd, void *arg)
{
	return (terminal_ioctl_vnode(vn, cmd, arg));
}

static int
vnode_tty_stat(vnode_t *vn, posix_stat_t *st)
{
	(void)vn;
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

static devfs_node_t *
devfs_find(const char *name)
{
	devfs_node_t	*cur;

	cur = devfs_list;
	while (cur) {
		if (strcmp(cur->name, name) == 0) {
			return (cur);
		}
		cur = cur->next;
	}
	return (NULL);
}

static const char *
devfs_strip_prefix(const char *path)
{
	const char	*p;

	if (!path || path[0] == '\0') {
		return ("");
	}
	if (path[0] != '/') {
		return (path);
	}

	p = path + 1;
	while (*p != '\0' && *p != '/') {
		p++;
	}
	if (*p == '\0') {
		return ("");
	}
	if (p[1] == '\0') {
		return ("");
	}
	return (p + 1);
}

int
devfs_register(const char *name, int device_id,
    int (*read_fn)(vnode_t *, void *, u64, u64),
    int (*write_fn)(vnode_t *, const void *, u64, u64),
    int (*ioctl_fn)(vnode_t *, u64, void *),
    int (*stat_fn)(vnode_t *, posix_stat_t *),
    void *data)
{
	devfs_node_t	*node;
	int		j;

	if (!name || name[0] == '\0') {
		return (-1);
	}

	if (devfs_find(name) != NULL) {
		return (-1);
	}

	node = (devfs_node_t *)kmem_calloc(1, sizeof(devfs_node_t));
	if (!node) {
		return (-1);
	}

	memset(node->name, 0, sizeof(node->name));
	for (j = 0; j < 31 && name[j] != '\0'; j++) {
		node->name[j] = name[j];
	}
	node->name[j] = '\0';
	node->device_id = device_id;
	node->read_fn = read_fn;
	node->write_fn = write_fn;
	node->ioctl_fn = ioctl_fn;
	node->stat_fn = stat_fn;
	node->data = data;
	node->next = devfs_list;
	devfs_list = node;
	devfs_count++;

	return (0);
}

int
devfs_unregister(const char *name)
{
	devfs_node_t	*cur;
	devfs_node_t	*prev;

	prev = NULL;
	cur = devfs_list;
	while (cur) {
		if (strcmp(cur->name, name) == 0) {
			if (prev) {
				prev->next = cur->next;
			} else {
				devfs_list = cur->next;
			}
			kmem_free(cur);
			devfs_count--;
			return (0);
		}
		prev = cur;
		cur = cur->next;
	}
	return (-1);
}

void
devfs_init(void)
{
	devfs_list = NULL;
	devfs_count = 0;

	devfs_register("null", DEVFS_DEV_NULL,
	    dev_null_read, dev_null_write, NULL, NULL, NULL);
	devfs_register("zero", DEVFS_DEV_ZERO,
	    dev_zero_read, dev_zero_write, NULL, NULL, NULL);
	devfs_register("tty", DEVFS_DEV_TTY,
	    vnode_tty_read, vnode_tty_write, vnode_tty_ioctl,
	    vnode_tty_stat, (void *)(unsigned long)-1);
	devfs_register("console", DEVFS_DEV_CONSOLE,
	    vnode_tty_read, vnode_tty_write, vnode_tty_ioctl,
	    vnode_tty_stat, (void *)(unsigned long)-1);
	devfs_register("random", DEVFS_DEV_RANDOM,
	    dev_random_read, dev_random_write, NULL, NULL, NULL);
	devfs_register("urandom", DEVFS_DEV_URANDOM,
	    dev_urandom_read, dev_urandom_write, NULL, NULL, NULL);
	if (drm_fbdev_is_ready()) {
		devfs_register("fb0", DEVFS_DEV_FB0,
		    drm_fbdev_vnode_read, drm_fbdev_vnode_write,
		    drm_fbdev_vnode_ioctl, drm_fbdev_vnode_stat, NULL);
	}

	pty_init();

	drivers_log("[DEVFS] mounted at /dev (%d devices, RAM-backed)\n",
	    devfs_count);
}

vnode_t *
devfs_lookup(const char *path)
{
	const char		*dev_name;
	devfs_node_t		*dev;
	vnode_t			*vn;
	process_t		*proc;
	int			 tty_idx;

	dev_name = devfs_strip_prefix(path);

	if (dev_name[0] == '\0') {
		vn = vnode_alloc(VDIR, "dev");
		if (vn) {
			vn->readdir_fn = devfs_root_readdir;
			vn->data = NULL;
		}
		return (vn);
	}

	dev = devfs_find(dev_name);
	if (!dev) {
		return (NULL);
	}

	vn = vnode_alloc(VCHR, dev->name);
	if (!vn) {
		return (NULL);
	}

	vn->data = dev->data;
	vn->read_fn = dev->read_fn;
	vn->write_fn = dev->write_fn;
	vn->ioctl_fn = dev->ioctl_fn;
	vn->stat_fn = dev->stat_fn ? dev->stat_fn : vnode_simple_stat;
	vn->readdir_fn = NULL;

	if (dev->device_id == DEVFS_DEV_FB0) {
		vn->mode = POSIX_S_IFCHR | 0600;
		vn->uid = 0;
		vn->gid = 0;
	}

	if (dev->device_id == DEVFS_DEV_TTY) {
		proc = process_current();
		if (proc && proc->controlling_tty < -1) {
			int	pty_id;
			vnode_t	*slave_vn;

			pty_id = -proc->controlling_tty - 2;
			vnode_release(vn);
			slave_vn = pty_create_slave_vnode(pty_id);
			return (slave_vn);
		}
		tty_idx = -1;
		if (proc && proc->controlling_tty >= 0) {
			tty_idx = proc->controlling_tty;
		}
		vn->data = (void *)(unsigned long)tty_idx;
	}

	return (vn);
}

int
devfs_root_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	devfs_node_t	*cur;
	u32		i;

	(void)vn;

	if (index >= (u32)devfs_count) {
		return (0);
	}

	cur = devfs_list;
	i = 0;

	{
		u32	target;

		target = (u32)devfs_count - 1 - index;
		while (cur && i < target) {
			cur = cur->next;
			i++;
		}
	}

	if (!cur) {
		return (0);
	}

	{
		int	j;

		for (j = 0; j < 31 && cur->name[j] != '\0'; j++) {
			name[j] = cur->name[j];
		}
		name[j] = '\0';
	}

	if (type) {
		*type = VCHR;
	}

	return (1);
}
