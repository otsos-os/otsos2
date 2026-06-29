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
#include <kernel/drivers/tty.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/crypto/rng/rng.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

#define MAX_DEVFS_DEVICES 16

typedef struct {
	char		name[32];
	int		device_id;
	int		(*read_fn)(void *, u64);
	int		(*write_fn)(const void *, u64);
} devfs_entry_t;

static devfs_entry_t	devfs_devices[MAX_DEVFS_DEVICES];
static int		devfs_device_count = 0;

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
dev_tty_read(void *buf, u64 count)
{
	return (tty_read(buf, (u32)count));
}

static int
dev_tty_write(const void *buf, u64 count)
{
	return (tty_write(buf, (u32)count));
}

static int
dev_console_read(void *buf, u64 count)
{
	return (tty_read(buf, (u32)count));
}

static int
dev_console_write(const void *buf, u64 count)
{
	return (tty_write(buf, (u32)count));
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

static void
devfs_register(const char *name, int device_id,
    int (*read_fn)(void *, u64), int (*write_fn)(const void *, u64))
{
	int	i;

	if (devfs_device_count >= MAX_DEVFS_DEVICES) {
		return;
	}

	i = devfs_device_count;
	memset(devfs_devices[i].name, 0, sizeof(devfs_devices[i].name));
	int	j;
	for (j = 0; j < 31 && name[j] != '\0'; j++) {
		devfs_devices[i].name[j] = name[j];
	}
	devfs_devices[i].name[j] = '\0';
	devfs_devices[i].device_id = device_id;
	devfs_devices[i].read_fn = read_fn;
	devfs_devices[i].write_fn = write_fn;
	devfs_device_count++;
}

void
devfs_init(void)
{
	devfs_device_count = 0;
	memset(devfs_devices, 0, sizeof(devfs_devices));

	devfs_register("null", DEVFS_DEV_NULL,
	    dev_null_read, dev_null_write);
	devfs_register("zero", DEVFS_DEV_ZERO,
	    dev_zero_read, dev_zero_write);
	devfs_register("tty", DEVFS_DEV_TTY,
	    dev_tty_read, dev_tty_write);
	devfs_register("console", DEVFS_DEV_CONSOLE,
	    dev_console_read, dev_console_write);
	devfs_register("random", DEVFS_DEV_RANDOM,
	    dev_random_read, dev_random_write);
	devfs_register("urandom", DEVFS_DEV_URANDOM,
	    dev_urandom_read, dev_urandom_write);

	com1_printf("[DEVFS] initialized (%d devices)\n",
	    devfs_device_count);
}

static int
devfs_dev_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	devfs_entry_t	*dev;

	(void)offset;
	dev = (devfs_entry_t *)vn->data;
	if (!dev || !dev->read_fn) {
		return (-1);
	}
	return (dev->read_fn(buf, count));
}

static int
devfs_dev_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	devfs_entry_t	*dev;

	(void)offset;
	dev = (devfs_entry_t *)vn->data;
	if (!dev || !dev->write_fn) {
		return (-1);
	}
	return (dev->write_fn(buf, count));
}

static int
devfs_dev_stat(vnode_t *vn, posix_stat_t *st)
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

static devfs_entry_t *
devfs_find_device(const char *name)
{
	int	i;

	for (i = 0; i < devfs_device_count; i++) {
		if (strcmp(devfs_devices[i].name, name) == 0) {
			return (&devfs_devices[i]);
		}
	}
	return (NULL);
}

vnode_t *
devfs_lookup(const char *path)
{
	const char	*dev_name;
	devfs_entry_t	*dev;
	vnode_t		*vn;

	dev_name = path;
	if (strcmp(dev_name, "/dev/") == 0) {
		dev_name = "";
	} else {
		int	i;
		int	match;
		const char	*prefix = "/dev/";

		match = 1;
		for (i = 0; i < 5; i++) {
			if (dev_name[i] != prefix[i]) {
				match = 0;
				break;
			}
		}
		if (match) {
			dev_name = dev_name + 5;
		} else {
			prefix = "dev/";
			match = 1;
			for (i = 0; i < 4; i++) {
				if (dev_name[i] != prefix[i]) {
					match = 0;
					break;
				}
			}
			if (match) {
				dev_name = dev_name + 4;
			}
		}
	}

	if (dev_name[0] == '\0') {
		vn = vnode_alloc(VDIR, "dev");
		if (vn) {
			vn->readdir_fn = devfs_root_readdir;
			vn->data = NULL;
		}
		return (vn);
	}

	dev = devfs_find_device(dev_name);
	if (!dev) {
		return (NULL);
	}

	vn = vnode_alloc(VCHR, dev->name);
	if (!vn) {
		return (NULL);
	}

	vn->data = dev;
	vn->read_fn = devfs_dev_read;
	vn->write_fn = devfs_dev_write;
	vn->stat_fn = devfs_dev_stat;
	vn->readdir_fn = NULL;

	return (vn);
}

int
devfs_root_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	(void)vn;

	if (index >= (u32)devfs_device_count) {
		return (0);
	}

	int	j;
	for (j = 0; j < 31 && devfs_devices[index].name[j] != '\0'; j++) {
		name[j] = devfs_devices[index].name[j];
	}
	name[j] = '\0';

	if (type) {
		*type = VCHR;
	}

	return (1);
}
