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

$define %type vnode_t as VFS vnode
$define %type vfs_dirent_t as VFS directory entry
$define %type device_t as pointer to newbus device
$define %type newbus_interface_t as named driver I/O interface table
$define %type driver_ns_leaf_t as open namespace interface binding

$define %func driver_ns_is_path as function with args const char *
$define %func driver_ns_lookup as function with args const char *
$define %func driver_ns_split_path as function with args path, component, component
$define %func driver_ns_device_visible as function with args device_t
$define %func driver_ns_find_device as function with args const char *
$define %func driver_ns_name_copy as procedure with args const char *, char *
$define %func driver_ns_name_seen_before as function with args device_t
$define %func driver_ns_root_listdir as function with args vnode_t, u32, entries, u32, count
$define %func driver_ns_dev_listdir as function with args vnode_t, u32, entries, u32, count
$define %func driver_ns_leaf_read as function with args vnode_t, void *, u64, u64
$define %func driver_ns_leaf_write as function with args vnode_t, const void *, u64, u64
$define %func driver_ns_leaf_ioctl as function with args vnode_t, u64, void *
$define %func driver_ns_leaf_stat as function with args vnode_t, posix_stat_t *
$define %func driver_ns_leaf_release as procedure with args vnode_t *
$define %func driver_ns_lookup_path as function with args const char *
$define %func driver_ns_identify as procedure with args driver_t, device_t
$define %func driver_ns_attach as function with args device_t

*/

/* !SPACE!

$space %internal driver_ns_split_path, driver_ns_device_visible
$space %internal driver_ns_find_device, driver_ns_name_copy
$space %internal driver_ns_name_seen_before
$space %internal driver_ns_root_listdir, driver_ns_dev_listdir
$space %internal driver_ns_leaf_read, driver_ns_leaf_write
$space %internal driver_ns_leaf_ioctl, driver_ns_leaf_stat
$space %internal driver_ns_leaf_release, driver_ns_lookup_path
$space %internal driver_ns_identify, driver_ns_attach
$space %export driver_ns_is_path, driver_ns_lookup

*/

#include <kernel/api/errno.h>
#include <kernel/api/posix/posix.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/newbus/driver_ns.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/smp/smp.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#define	DRIVER_NS_PREFIX	"/Driver"
#define	DRIVER_NS_PREFIX_LEN	7
#define	DRIVER_NS_COMPONENT_MAX	32

typedef struct driver_ns_leaf {
	device_t			dev;
	const newbus_interface_t	*iface;
	int				handle;
} driver_ns_leaf_t;

static int
driver_ns_split_path(const char *path, char *c1, size_t c1sz,
    char *c2, size_t c2sz)
{
	const char	*p;
	char		*dst;
	size_t		dstsz;
	int		comp, len;

	p = path + DRIVER_NS_PREFIX_LEN;
	comp = 0;
	for (;;) {
		while (*p == '/') {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		if (comp >= 2) {
			return (-1);
		}
		if (comp == 0) {
			dst = c1;
			dstsz = c1sz;
		} else {
			dst = c2;
			dstsz = c2sz;
		}
		len = 0;
		while (*p != '\0' && *p != '/' &&
		    len < (int)dstsz - 1) {
			dst[len++] = *p++;
		}
		if (*p != '\0' && *p != '/' && len >= (int)dstsz - 1) {
			return (-1);
		}
		dst[len] = '\0';
		if (strcmp(dst, ".") == 0 || strcmp(dst, "..") == 0) {
			return (-1);
		}
		comp++;
		while (*p != '\0' && *p != '/') {
			p++;
		}
	}
	return (comp);
}

static int
driver_ns_device_visible(device_t dev)
{
	if (dev == NULL || device_get_driver(dev) == NULL ||
	    device_get_state(dev) != DS_ATTACHED) {
		return (0);
	}
	return (newbus_interface_count(dev) > 0);
}

static device_t
driver_ns_find_device(const char *name)
{
	device_t	dev;
	int		i;

	for (i = 0; i < newbus_device_count_get(); i++) {
		dev = newbus_device_get(i);
		if (dev == NULL || !driver_ns_device_visible(dev)) {
			continue;
		}
		if (strcmp(device_get_nameunit(dev), name) == 0) {
			return (dev);
		}
	}
	for (i = 0; i < newbus_device_count_get(); i++) {
		dev = newbus_device_get(i);
		if (dev == NULL || !driver_ns_device_visible(dev)) {
			continue;
		}
		if (strcmp(device_get_name(dev), name) == 0) {
			return (dev);
		}
	}
	return (NULL);
}

static void
driver_ns_name_copy(const char *src, char *dst)
{
	int	i;

	for (i = 0; i < 31 && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
}

static int
driver_ns_name_seen_before(device_t dev)
{
	device_t	prev;
	int		i;

	for (i = 0; i < newbus_device_count_get(); i++) {
		prev = newbus_device_get(i);
		if (prev == dev) {
			break;
		}
		if (driver_ns_device_visible(prev) &&
		    strcmp(device_get_name(prev),
		    device_get_name(dev)) == 0) {
			return (1);
		}
	}
	return (0);
}

static int
driver_ns_root_listdir(vnode_t *vn, u32 start, vfs_dirent_t *entries,
    u32 max_entries, u32 *count)
{
	device_t	dev;
	u32		seen, copied;
	int		i;

	(void)vn;
	if (entries == NULL || count == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	*count = 0;
	if (max_entries == 0) {
		return (0);
	}

	copied = 0;
	seen = 0;
	smp_lock();
	for (i = 0; i < newbus_device_count_get(); i++) {
		dev = newbus_device_get(i);
		if (dev == NULL || !driver_ns_device_visible(dev)) {
			continue;
		}
		if (!driver_ns_name_seen_before(dev)) {
			if (seen >= start && copied < max_entries) {
				memset(&entries[copied], 0,
				    sizeof(entries[copied]));
				driver_ns_name_copy(device_get_name(dev),
				    entries[copied].name);
				entries[copied].type = VDIR;
				copied++;
			}
			seen++;
		}
		if (device_get_unit(dev) > 0) {
			if (seen >= start && copied < max_entries) {
				memset(&entries[copied], 0,
				    sizeof(entries[copied]));
				driver_ns_name_copy(
				    device_get_nameunit(dev),
				    entries[copied].name);
				entries[copied].type = VDIR;
				copied++;
			}
			seen++;
		}
		if (copied >= max_entries) {
			break;
		}
	}
	smp_unlock();
	*count = copied;
	return (0);
}

static int
driver_ns_dev_listdir(vnode_t *vn, u32 start, vfs_dirent_t *entries,
    u32 max_entries, u32 *count)
{
	const newbus_interface_t	*iface;
	device_t			dev;
	u32				total, i, copied;

	dev = (device_t)vn->data;
	if (dev == NULL || entries == NULL || count == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	*count = 0;
	if (max_entries == 0) {
		return (0);
	}

	smp_lock();
	total = (u32)newbus_interface_count(dev);
	if (start >= total) {
		smp_unlock();
		return (0);
	}
	copied = 0;
	for (i = start; i < total && copied < max_entries; i++) {
		iface = newbus_interface_get(dev, (int)i);
		if (iface == NULL) {
			break;
		}
		memset(&entries[copied], 0, sizeof(entries[copied]));
		driver_ns_name_copy(iface->name, entries[copied].name);
		entries[copied].type = VCHR;
		copied++;
	}
	smp_unlock();
	*count = copied;
	return (0);
}

static int
driver_ns_leaf_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	driver_ns_leaf_t	*leaf;

	leaf = (driver_ns_leaf_t *)vn->data;
	if (leaf == NULL || leaf->iface == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	if (leaf->iface->read == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (leaf->iface->read(leaf->dev, buf, count, offset));
}

static int
driver_ns_leaf_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	driver_ns_leaf_t	*leaf;

	leaf = (driver_ns_leaf_t *)vn->data;
	if (leaf == NULL || leaf->iface == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	if (leaf->iface->write == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (leaf->iface->write(leaf->dev, buf, count, offset));
}

static int
driver_ns_leaf_ioctl(vnode_t *vn, u64 cmd, void *arg)
{
	driver_ns_leaf_t	*leaf;

	leaf = (driver_ns_leaf_t *)vn->data;
	if (leaf == NULL || leaf->iface == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	if (leaf->iface->ioctl == NULL) {
		return (-POSIX_ENOTTY);
	}
	return (leaf->iface->ioctl(leaf->dev, cmd, arg));
}

static int
driver_ns_leaf_stat(vnode_t *vn, posix_stat_t *st)
{
	driver_ns_leaf_t	*leaf;

	leaf = (driver_ns_leaf_t *)vn->data;
	if (leaf == NULL || st == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	if (leaf->iface != NULL && leaf->iface->stat != NULL) {
		return (leaf->iface->stat(leaf->dev, st));
	}
	memset(st, 0, sizeof(*st));
	st->st_mode = POSIX_S_IFCHR | 0666;
	st->st_blksize = 512;
	st->st_nlink = 1;
	return (0);
}

static void
driver_ns_leaf_release(vnode_t *vn)
{
	driver_ns_leaf_t	*leaf;

	leaf = (driver_ns_leaf_t *)vn->data;
	if (leaf != NULL) {
		newbus_interface_close(leaf->handle);
	}
}

static vnode_t *
driver_ns_lookup_path(const char *path)
{
	char			c1[DRIVER_NS_COMPONENT_MAX];
	char			c2[DRIVER_NS_COMPONENT_MAX];
	driver_ns_leaf_t	*leaf;
	const newbus_interface_t	*iface;
	device_t		dev;
	vnode_t			*vn;
	int			handle, ncomp;

	ncomp = driver_ns_split_path(path, c1, sizeof(c1), c2, sizeof(c2));
	if (ncomp < 0) {
		return (NULL);
	}
	if (ncomp == 0) {
		vn = vnode_alloc(VDIR, "Driver");
		if (vn != NULL) {
			vn->listdir_fn = driver_ns_root_listdir;
		}
		return (vn);
	}

	smp_lock();
	dev = driver_ns_find_device(c1);
	if (dev == NULL) {
		smp_unlock();
		return (NULL);
	}
	if (ncomp == 1) {
		vn = vnode_alloc(VDIR, c1);
		if (vn != NULL) {
			vn->data = dev;
			vn->listdir_fn = driver_ns_dev_listdir;
		}
		smp_unlock();
		return (vn);
	}

	iface = newbus_interface_find(dev, c2);
	if (iface == NULL) {
		smp_unlock();
		return (NULL);
	}
	handle = newbus_interface_open(dev, c2);
	smp_unlock();
	if (handle < 0) {
		return (NULL);
	}

	vn = vnode_alloc(VCHR, iface->name);
	if (vn == NULL) {
		newbus_interface_close(handle);
		return (NULL);
	}
	leaf = (driver_ns_leaf_t *)kmem_calloc(1, sizeof(*leaf));
	if (leaf == NULL) {
		newbus_interface_close(handle);
		vnode_release(vn);
		return (NULL);
	}
	leaf->dev = dev;
	leaf->iface = iface;
	leaf->handle = handle;
	vn->data = leaf;
	vn->data_owned = 1;
	vn->read_fn = driver_ns_leaf_read;
	vn->write_fn = driver_ns_leaf_write;
	vn->ioctl_fn = driver_ns_leaf_ioctl;
	vn->stat_fn = driver_ns_leaf_stat;
	vn->release_fn = driver_ns_leaf_release;
	return (vn);
}

int
driver_ns_is_path(const char *path)
{
	if (path == NULL) {
		return (0);
	}
	if (strncmp(path, DRIVER_NS_PREFIX, DRIVER_NS_PREFIX_LEN) != 0) {
		return (0);
	}
	return (path[DRIVER_NS_PREFIX_LEN] == '\0' ||
	    path[DRIVER_NS_PREFIX_LEN] == '/');
}

vnode_t *
driver_ns_lookup(const char *path)
{
	if (!driver_ns_is_path(path)) {
		return (NULL);
	}
	return (driver_ns_lookup_path(path));
}

static void
driver_ns_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "driver_ns", 0) == NULL) {
		device_add_child(parent, "driver_ns", 0);
	}
}

static int
driver_ns_attach(device_t dev)
{
	(void)dev;
	drivers_log("[DRIVER-NS] /Driver native namespace ready\n");
	return (0);
}

static devclass_t driver_ns_devclass = {
	.name		= "driver_ns",
	.maxunit	= 1,
};

static driver_t driver_ns_driver = {
	.name		= "driver_ns",
	.identify	= driver_ns_identify,
	.probe		= NULL,
	.attach		= driver_ns_attach,
};

PSEUDO_DRIVER_MODULE(driver_ns, driver_ns_driver,
    driver_ns_devclass, NEWBUS_PASS_FILESYSTEM, NEWBUS_ORDER_LAST);
