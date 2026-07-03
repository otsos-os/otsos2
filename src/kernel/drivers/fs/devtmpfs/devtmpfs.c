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

#include <kernel/drivers/fs/devtmpfs/devtmpfs.h>
#include <kernel/console/terminal.h>
#include <kernel/crypto/rng/rng.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

typedef struct devtmpfs_node {
	char			name[32];
	int			device_id;
	int			(*read_fn)(void *, u64);
	int			(*write_fn)(const void *, u64);
	struct devtmpfs_node	*next;
} devtmpfs_node_t;

static devtmpfs_node_t	*devtmpfs_list = NULL;
static int		devtmpfs_count = 0;

static int
dev_null_read(void *buf, u64 count)
{
	(void)buf;
	(void)count;
	return (0);
}

static int
dev_null_write(const void *buf, u64 count)
{
	(void)buf;
	return ((int)count);
}

static int
dev_zero_read(void *buf, u64 count)
{
	memset(buf, 0, (unsigned long)count);
	return ((int)count);
}

static int
dev_zero_write(const void *buf, u64 count)
{
	(void)buf;
	return ((int)count);
}

static int
dev_terminal_read(void *buf, u64 count)
{
	return (terminal_read(buf, (u32)count));
}

static int
dev_terminal_write(const void *buf, u64 count)
{
	return (terminal_write(buf, (u32)count));
}

static int
dev_console_read(void *buf, u64 count)
{
	return (terminal_read(buf, (u32)count));
}

static int
dev_console_write(const void *buf, u64 count)
{
	return (terminal_write(buf, (u32)count));
}

static int
dev_random_read(void *buf, u64 count)
{
	if (count == 0) {
		return (0);
	}
	if (crypto_rng_bytes((u8 *)buf, (u32)count) != 0) {
		return (-1);
	}
	return ((int)count);
}

static int
dev_random_write(const void *buf, u64 count)
{
	crypto_rng_add_entropy((const u8 *)buf, (u32)count);
	return ((int)count);
}

static int
dev_urandom_read(void *buf, u64 count)
{
	if (count == 0) {
		return (0);
	}
	if (crypto_rng_bytes((u8 *)buf, (u32)count) != 0) {
		return (-1);
	}
	return ((int)count);
}

static int
dev_urandom_write(const void *buf, u64 count)
{
	(void)buf;
	return ((int)count);
}

static int
devtmpfs_dev_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	devtmpfs_node_t	*dev;

	(void)offset;
	dev = (devtmpfs_node_t *)vn->data;
	if (!dev || !dev->read_fn) {
		return (-1);
	}
	return (dev->read_fn(buf, count));
}

static int
devtmpfs_dev_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	devtmpfs_node_t	*dev;

	(void)offset;
	dev = (devtmpfs_node_t *)vn->data;
	if (!dev || !dev->write_fn) {
		return (-1);
	}
	return (dev->write_fn(buf, count));
}

static int
devtmpfs_dev_stat(vnode_t *vn, posix_stat_t *st)
{
	memset(st, 0, sizeof(posix_stat_t));
	st->st_mode = POSIX_S_IFCHR | 0666;
	st->st_size = 0;
	st->st_blksize = 0;
	st->st_blocks = 0;
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	(void)vn;
	return (0);
}

static devtmpfs_node_t *
devtmpfs_find(const char *name)
{
	devtmpfs_node_t	*cur;

	cur = devtmpfs_list;
	while (cur) {
		if (strcmp(cur->name, name) == 0) {
			return (cur);
		}
		cur = cur->next;
	}
	return (NULL);
}

static const char *
devtmpfs_strip_prefix(const char *path)
{
	const char	*prefix;
	int		i;
	int		match;

	if (strcmp(path, "/dev/") == 0) {
		return ("");
	}

	prefix = "/dev/";
	match = 1;
	for (i = 0; i < 5; i++) {
		if (path[i] != prefix[i]) {
			match = 0;
			break;
		}
	}
	if (match) {
		return (path + 5);
	}

	prefix = "dev/";
	match = 1;
	for (i = 0; i < 4; i++) {
		if (path[i] != prefix[i]) {
			match = 0;
			break;
		}
	}
	if (match) {
		return (path + 4);
	}

	return (path);
}

int
devtmpfs_register(const char *name, int device_id,
    int (*read_fn)(void *, u64), int (*write_fn)(const void *, u64))
{
	devtmpfs_node_t	*node;

	if (!name || name[0] == '\0') {
		return (-1);
	}

	if (devtmpfs_find(name) != NULL) {
		return (-1);
	}

	node = (devtmpfs_node_t *)kmem_calloc(1, sizeof(devtmpfs_node_t));
	if (!node) {
		return (-1);
	}

	memset(node->name, 0, sizeof(node->name));
	{
		int	j;

		for (j = 0; j < 31 && name[j] != '\0'; j++) {
			node->name[j] = name[j];
		}
		node->name[j] = '\0';
	}
	node->device_id = device_id;
	node->read_fn = read_fn;
	node->write_fn = write_fn;
	node->next = devtmpfs_list;
	devtmpfs_list = node;
	devtmpfs_count++;

	return (0);
}

int
devtmpfs_unregister(const char *name)
{
	devtmpfs_node_t	*cur;
	devtmpfs_node_t	*prev;

	prev = NULL;
	cur = devtmpfs_list;
	while (cur) {
		if (strcmp(cur->name, name) == 0) {
			if (prev) {
				prev->next = cur->next;
			} else {
				devtmpfs_list = cur->next;
			}
			kmem_free(cur);
			devtmpfs_count--;
			return (0);
		}
		prev = cur;
		cur = cur->next;
	}
	return (-1);
}

void
devtmpfs_init(void)
{
	devtmpfs_list = NULL;
	devtmpfs_count = 0;

	devtmpfs_register("null", DEVTMPFS_DEV_NULL,
	    dev_null_read, dev_null_write);
	devtmpfs_register("zero", DEVTMPFS_DEV_ZERO,
	    dev_zero_read, dev_zero_write);
	devtmpfs_register("tty", DEVTMPFS_DEV_TTY,
	    dev_terminal_read, dev_terminal_write);
	devtmpfs_register("console", DEVTMPFS_DEV_CONSOLE,
	    dev_console_read, dev_console_write);
	devtmpfs_register("random", DEVTMPFS_DEV_RANDOM,
	    dev_random_read, dev_random_write);
	devtmpfs_register("urandom", DEVTMPFS_DEV_URANDOM,
	    dev_urandom_read, dev_urandom_write);

	drivers_log("[DEVTMPFS] mounted at /dev (%d devices, RAM-backed)\n",
	    devtmpfs_count);
}

vnode_t *
devtmpfs_lookup(const char *path)
{
	const char		*dev_name;
	devtmpfs_node_t		*dev;
	vnode_t			*vn;

	dev_name = devtmpfs_strip_prefix(path);

	if (dev_name[0] == '\0') {
		vn = vnode_alloc(VDIR, "dev");
		if (vn) {
			vn->readdir_fn = devtmpfs_root_readdir;
			vn->data = NULL;
		}
		return (vn);
	}

	dev = devtmpfs_find(dev_name);
	if (!dev) {
		return (NULL);
	}

	vn = vnode_alloc(VCHR, dev->name);
	if (!vn) {
		return (NULL);
	}

	vn->data = dev;
	vn->read_fn = devtmpfs_dev_read;
	vn->write_fn = devtmpfs_dev_write;
	vn->stat_fn = devtmpfs_dev_stat;
	vn->readdir_fn = NULL;

	return (vn);
}

int
devtmpfs_root_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	devtmpfs_node_t	*cur;
	u32			i;

	(void)vn;

	if (index >= (u32)devtmpfs_count) {
		return (0);
	}

	cur = devtmpfs_list;
	i = 0;

	/*
	 * The list is stored newest-first (prepend on register).
	 * readdir index 0 should return the first registered device
	 * for stable ordering, so we walk to the (count - 1 - index)
	 * position — effectively reversing the list.
	 */
	{
		u32	target;

		target = (u32)devtmpfs_count - 1 - index;
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
