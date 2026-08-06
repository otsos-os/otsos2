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

$define %type newbus_interface_t as named driver I/O interface table
$define %type newbus_interface_entry_t as registered interface slot
$define %type device_t as pointer to newbus device
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type entity_io_ops as table of entity I/O callbacks

$define %func newbus_interface_register as function with args device_t, interface
$define %func newbus_interface_unregister as function with args device_t, interface
$define %func newbus_interface_count as function with args device_t
$define %func newbus_interface_get as function with args device_t, int
$define %func newbus_interface_find as function with args device_t, const char *
$define %func newbus_interface_open as function with args device_t, const char *
$define %func newbus_interface_close as procedure with args int
$define %func newbus_interface_name_valid as function with args const char *
$define %func newbus_interface_entity_bind as function with args int
$define %func newbus_interface_entity_release as procedure with args entity id
$define %func newbus_device_entity_release as procedure with args entity id
$define %func newbus_entity_device_sync as procedure with args device_t
$define %func newbus_entity_init as procedure with args void
$define %func newbus_interface_read_entity as function with args entity id, buffer, count, offset
$define %func newbus_interface_write_entity as function with args entity id, buffer, count, offset
$define %func newbus_interface_ioctl_entity as function with args entity id, command, argument
$define %func newbus_interface_stat_entity as function with args entity id, size

*/

/* !SPACE!

$space %internal newbus_interface_name_valid
$space %internal newbus_interface_entity_bind
$space %internal newbus_interface_entity_release
$space %internal newbus_device_entity_release
$space %export newbus_interface_register, newbus_interface_unregister
$space %export newbus_interface_count, newbus_interface_get
$space %export newbus_interface_find, newbus_interface_open
$space %export newbus_interface_close
$space %export newbus_entity_device_sync, newbus_entity_init
$space %export newbus_interface_read_entity, newbus_interface_write_entity
$space %export newbus_interface_ioctl_entity, newbus_interface_stat_entity

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/api/posix/posix.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/smp/smp.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	NB_ENTITY_PATH_PREFIX	"/Entity/Interface/Driver"
#define	NB_ENTITY_PATH_MAX	128

typedef struct newbus_interface_entry {
	device_t			dev;
	const newbus_interface_t	*iface;
	u32				open_count;
	u64				entity;
	int				used;
} newbus_interface_entry_t;

static newbus_interface_entry_t	newbus_interface_slots[NEWBUS_MAX_INTERFACES];
static int			newbus_entity_initialized;

static int
newbus_interface_name_valid(const char *name)
{
	if (name == NULL || name[0] == '\0') {
		return (0);
	}
	if (strlen(name) >= NEWBUS_NAME_MAX) {
		return (0);
	}
	return (1);
}

static void
newbus_interface_entity_release(entity_id_t id)
{
	newbus_interface_entry_t	*entry;
	s32				slot_index;

	if (entity_io_i32(id, 0, &slot_index) != 0 ||
	    slot_index < 0 || slot_index >= NEWBUS_MAX_INTERFACES) {
		return;
	}
	entry = &newbus_interface_slots[(u32)slot_index];
	if (entry->used && entry->entity == id) {
		entry->entity = 0;
	}
}

static void
newbus_device_entity_release(entity_id_t id)
{
	device_t	dev;
	s32		dev_index;

	if (entity_io_i32(id, 4, &dev_index) != 0) {
		return;
	}
	dev = newbus_device_get((int)dev_index);
	if (dev != NULL && dev->entity == id) {
		dev->entity = 0;
	}
}

static int
newbus_interface_entity_bind(int index)
{
	newbus_interface_entry_t	*entry;
	entity_id_t			id;
	const char			*devname;
	char				name[NB_ENTITY_PATH_MAX];
	int				ret;

	if (index < 0 || index >= NEWBUS_MAX_INTERFACES) {
		return (-1);
	}
	entry = &newbus_interface_slots[index];
	if (!entry->used || entry->dev == NULL ||
	    entry->iface == NULL || !entity_is_initialized()) {
		return (-1);
	}
	id = entity_create(ENTITY_ARCH_NB_INTERFACE, 0,
	    0, 0, 0, 0, 0, 1);
	if (id == 0) {
		drivers_log("[NEWBUS] interface entity alloc failed "
		    "for %s/%s\n", device_get_nameunit(entry->dev),
		    entry->iface->name);
		return (-1);
	}
	entity_io_set_i32(id, 0, (s32)index);
	entity_io_set_i32(id, 1, (s32)entry->dev->index);
	entity_set_data(id, ENTITY_IO_DATA_OFFSET, 0);
	devname = device_get_nameunit(entry->dev);
	snprintf(name, sizeof(name), NB_ENTITY_PATH_PREFIX "/%s/%s",
	    devname ? devname : "?", entry->iface->name);
	ret = entity_ns_bind(name, id);
	if (ret != 0) {
		drivers_log("[NEWBUS] interface entity bind %s failed "
		    "(%d)\n", name, ret);
		entity_destroy(id);
		return (ret);
	}
	entry->entity = id;
	drivers_log("[NEWBUS] interface entity %s\n", name);
	return (0);
}

void
newbus_entity_device_sync(device_t dev)
{
	entity_id_t	id;
	char		name[NB_ENTITY_PATH_MAX];
	int		visible;

	if (dev == NULL || !entity_is_initialized()) {
		return;
	}
	visible = device_get_driver(dev) != NULL &&
	    device_get_state(dev) == DS_ATTACHED &&
	    newbus_interface_count(dev) > 0;
	if (dev->entity != 0) {
		if (visible && entity_valid(dev->entity)) {
			return;
		}
		entity_ns_unbind_all_id(dev->entity);
		entity_destroy(dev->entity);
		dev->entity = 0;
		return;
	}
	if (!visible) {
		return;
	}
	id = entity_create(ENTITY_ARCH_NB_DEVICE, 0,
	    0, 0, 0, 0, 0, 1);
	if (id == 0) {
		drivers_log("[NEWBUS] device entity alloc failed "
		    "for %s\n", device_get_nameunit(dev));
		return;
	}
	entity_io_set_i32(id, 1, (s32)device_get_state(dev));
	entity_io_set_i32(id, 2, (s32)device_get_unit(dev));
	entity_io_set_i32(id, 3, (s32)newbus_interface_count(dev));
	entity_io_set_i32(id, 4, (s32)dev->index);
	snprintf(name, sizeof(name), NB_ENTITY_PATH_PREFIX "/%s",
	    device_get_nameunit(dev));
	if (entity_ns_bind(name, id) != 0) {
		drivers_log("[NEWBUS] device entity bind %s failed\n",
		    name);
		entity_destroy(id);
		return;
	}
	dev->entity = id;
	drivers_log("[NEWBUS] device entity %s\n", name);
}

void
newbus_entity_init(void)
{
	if (newbus_entity_initialized) {
		return;
	}
	entity_arch_release_register(ENTITY_ARCH_NB_INTERFACE,
	    newbus_interface_entity_release);
	entity_arch_release_register(ENTITY_ARCH_NB_DEVICE,
	    newbus_device_entity_release);
	newbus_entity_initialized = 1;
}

int
newbus_interface_register(device_t dev, const newbus_interface_t *iface)
{
	int	i;

	if (dev == NULL || iface == NULL ||
	    !newbus_interface_name_valid(iface->name)) {
		return (-1);
	}

	smp_lock();
	for (i = 0; i < NEWBUS_MAX_INTERFACES; i++) {
		if (!newbus_interface_slots[i].used) {
			continue;
		}
		if (newbus_interface_slots[i].dev == dev &&
		    strcmp(newbus_interface_slots[i].iface->name,
		    iface->name) == 0) {
			smp_unlock();
			return (-1);
		}
	}
	for (i = 0; i < NEWBUS_MAX_INTERFACES; i++) {
		if (!newbus_interface_slots[i].used) {
			break;
		}
	}
	if (i >= NEWBUS_MAX_INTERFACES) {
		smp_unlock();
		return (-1);
	}

	newbus_interface_slots[i].dev = dev;
	newbus_interface_slots[i].iface = iface;
	newbus_interface_slots[i].open_count = 0;
	newbus_interface_slots[i].entity = 0;
	newbus_interface_slots[i].used = 1;
	smp_unlock();
	if (newbus_interface_entity_bind(i) != 0) {
		drivers_log("[NEWBUS] interface %s on %s has no entity; "
		    "raw fallback\n", iface->name,
		    device_get_nameunit(dev));
	}
	newbus_entity_device_sync(dev);
	drivers_log("[NEWBUS] interface %s on %s\n", iface->name,
	    device_get_nameunit(dev));
	return (0);
}

int
newbus_interface_unregister(device_t dev, const newbus_interface_t *iface)
{
	int	i;

	if (dev == NULL || iface == NULL) {
		return (-1);
	}

	smp_lock();
	for (i = 0; i < NEWBUS_MAX_INTERFACES; i++) {
		if (!newbus_interface_slots[i].used ||
		    newbus_interface_slots[i].dev != dev) {
			continue;
		}
		if (newbus_interface_slots[i].iface != iface &&
		    strcmp(newbus_interface_slots[i].iface->name,
		    iface->name) != 0) {
			continue;
		}
		if (newbus_interface_slots[i].open_count != 0) {
			smp_unlock();
			return (-1);
		}
		if (newbus_interface_slots[i].entity != 0 &&
		    entity_valid(newbus_interface_slots[i].entity) &&
		    entity_refs(newbus_interface_slots[i].entity) > 1) {
			smp_unlock();
			return (-1);
		}
		if (newbus_interface_slots[i].entity != 0) {
			entity_ns_unbind_all_id(
			    newbus_interface_slots[i].entity);
			entity_destroy(newbus_interface_slots[i].entity);
			newbus_interface_slots[i].entity = 0;
		}
		memset(&newbus_interface_slots[i], 0,
		    sizeof(newbus_interface_slots[i]));
		smp_unlock();
		newbus_entity_device_sync(dev);
		drivers_log("[NEWBUS] interface %s removed from %s\n",
		    iface->name, device_get_nameunit(dev));
		return (0);
	}
	smp_unlock();
	return (-1);
}

int
newbus_interface_count(device_t dev)
{
	int	i, count;

	if (dev == NULL) {
		return (0);
	}
	count = 0;
	smp_lock();
	for (i = 0; i < NEWBUS_MAX_INTERFACES; i++) {
		if (newbus_interface_slots[i].used &&
		    newbus_interface_slots[i].dev == dev) {
			count++;
		}
	}
	smp_unlock();
	return (count);
}

const newbus_interface_t *
newbus_interface_get(device_t dev, int index)
{
	const newbus_interface_t	*iface;
	int				i, seen;

	if (dev == NULL || index < 0) {
		return (NULL);
	}
	iface = NULL;
	seen = 0;
	smp_lock();
	for (i = 0; i < NEWBUS_MAX_INTERFACES; i++) {
		if (!newbus_interface_slots[i].used ||
		    newbus_interface_slots[i].dev != dev) {
			continue;
		}
		if (seen == index) {
			iface = newbus_interface_slots[i].iface;
			break;
		}
		seen++;
	}
	smp_unlock();
	return (iface);
}

const newbus_interface_t *
newbus_interface_find(device_t dev, const char *name)
{
	const newbus_interface_t	*iface;
	int				i;

	if (dev == NULL || !newbus_interface_name_valid(name)) {
		return (NULL);
	}
	iface = NULL;
	smp_lock();
	for (i = 0; i < NEWBUS_MAX_INTERFACES; i++) {
		if (!newbus_interface_slots[i].used ||
		    newbus_interface_slots[i].dev != dev) {
			continue;
		}
		if (strcmp(newbus_interface_slots[i].iface->name,
		    name) == 0) {
			iface = newbus_interface_slots[i].iface;
			break;
		}
	}
	smp_unlock();
	return (iface);
}

int
newbus_interface_open(device_t dev, const char *name)
{
	int	i;

	if (dev == NULL || !newbus_interface_name_valid(name)) {
		return (-1);
	}
	smp_lock();
	for (i = 0; i < NEWBUS_MAX_INTERFACES; i++) {
		if (!newbus_interface_slots[i].used ||
		    newbus_interface_slots[i].dev != dev) {
			continue;
		}
		if (strcmp(newbus_interface_slots[i].iface->name,
		    name) == 0) {
			newbus_interface_slots[i].open_count++;
			smp_unlock();
			return (i);
		}
	}
	smp_unlock();
	return (-1);
}

void
newbus_interface_close(int handle)
{
	smp_lock();
	if (handle >= 0 && handle < NEWBUS_MAX_INTERFACES &&
	    newbus_interface_slots[handle].used &&
	    newbus_interface_slots[handle].open_count > 0) {
		newbus_interface_slots[handle].open_count--;
	}
	smp_unlock();
}

static newbus_interface_entry_t *
newbus_interface_entry_by_id(entity_id_t id)
{
	newbus_interface_entry_t	*entry;
	s32				slot_index;

	if (entity_io_i32(id, 0, &slot_index) != 0 ||
	    slot_index < 0 || slot_index >= NEWBUS_MAX_INTERFACES) {
		return (NULL);
	}
	entry = &newbus_interface_slots[(u32)slot_index];
	if (!entry->used || entry->entity != id) {
		return (NULL);
	}
	return (entry);
}

int
newbus_interface_read_entity(entity_id_t id, void *buf, u64 count,
    u64 offset)
{
	newbus_interface_entry_t	*entry;

	entry = newbus_interface_entry_by_id(id);
	if (entry == NULL || entry->iface == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (entry->iface->read == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (entry->iface->read(entry->dev, buf, count, offset));
}

int
newbus_interface_write_entity(entity_id_t id, const void *buf, u64 count,
    u64 offset)
{
	newbus_interface_entry_t	*entry;

	entry = newbus_interface_entry_by_id(id);
	if (entry == NULL || entry->iface == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (entry->iface->write == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (entry->iface->write(entry->dev, buf, count, offset));
}

int
newbus_interface_ioctl_entity(entity_id_t id, u64 cmd, void *arg)
{
	newbus_interface_entry_t	*entry;

	entry = newbus_interface_entry_by_id(id);
	if (entry == NULL || entry->iface == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (entry->iface->ioctl == NULL) {
		return (-POSIX_ENOTTY);
	}
	return (entry->iface->ioctl(entry->dev, cmd, arg));
}

int
newbus_interface_stat_entity(entity_id_t id, u64 *size)
{
	newbus_interface_entry_t	*entry;
	posix_stat_t			st;

	if (size == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	entry = newbus_interface_entry_by_id(id);
	if (entry == NULL || entry->iface == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (entry->iface->stat == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (entry->iface->stat(entry->dev, &st) != 0) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	*size = (u64)st.st_size;
	return (0);
}
