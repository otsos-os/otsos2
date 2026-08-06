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

$define %func newbus_interface_register as function with args device_t, interface
$define %func newbus_interface_unregister as function with args device_t, interface
$define %func newbus_interface_count as function with args device_t
$define %func newbus_interface_get as function with args device_t, int
$define %func newbus_interface_find as function with args device_t, const char *
$define %func newbus_interface_open as function with args device_t, const char *
$define %func newbus_interface_close as procedure with args int
$define %func newbus_interface_name_valid as function with args const char *

*/

/* !SPACE!

$space %internal newbus_interface_name_valid
$space %export newbus_interface_register, newbus_interface_unregister
$space %export newbus_interface_count, newbus_interface_get
$space %export newbus_interface_find, newbus_interface_open
$space %export newbus_interface_close

*/

#include <kernel/drivers/newbus/newbus.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/smp/smp.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

typedef struct newbus_interface_entry {
	device_t			dev;
	const newbus_interface_t	*iface;
	u32				open_count;
	u64				entity;
	int				used;
} newbus_interface_entry_t;

static newbus_interface_entry_t	newbus_interface_slots[NEWBUS_MAX_INTERFACES];

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
	newbus_interface_slots[i].used = 1;
	smp_unlock();
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
		memset(&newbus_interface_slots[i], 0,
		    sizeof(newbus_interface_slots[i]));
		smp_unlock();
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
			if (newbus_interface_slots[i].open_count == 0) {
				entity_id_t	id;
				int		handle;

				if (entity_is_initialized()) {
					id = entity_create(
					    ENTITY_ARCH_NB_INTERFACE, 0,
					    0, 0, 0, 0, 0, 1);
					if (id != 0) {
						char	name[128];
						const char *dev_name;

						entity_io_set_ptr(id,
						    ENTITY_IO_PTR_BACKING,
						    &newbus_interface_slots[i]);
						dev_name =
						    device_get_nameunit(dev);
						snprintf(name, sizeof(name),
						    "/Entity/Interface/%s/%s",
						    dev_name ? dev_name : "?",
						    newbus_interface_slots[i].
						    iface->name);
						entity_ns_bind(name, id);
						handle = entity_handle_alloc(
						    NULL, id,
						    ENTITY_ACCESS_READ |
						    ENTITY_ACCESS_WRITE);
						if (handle >= 0) {
							entity_release(id);
							newbus_interface_slots[i].
							    entity = id;
							newbus_interface_slots[i].
							    open_count++;
							smp_unlock();
							return (handle);
						}
						entity_destroy(id);
					}
				}
			}
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
	entity_id_t	id;
	u32		access;
	int		slot;
	int		entity_mode;

	smp_lock();
	slot = -1;
	entity_mode = 0;
	if (entity_is_initialized() &&
	    entity_handle_lookup(NULL, handle, &id, &access) == 0 &&
	    entity_arch(id) == ENTITY_ARCH_NB_INTERFACE) {
		newbus_interface_entry_t	*entry;

		entry = (newbus_interface_entry_t *)entity_io_ptr(id,
		    ENTITY_IO_PTR_BACKING);
		if (entry && entry->used && entry->open_count > 0) {
			entry->open_count--;
			if (entry->open_count == 0) {
				entry->entity = 0;
			}
		}
		slot = 1;
		entity_mode = 1;
	} else if (handle >= 0 && handle < NEWBUS_MAX_INTERFACES &&
	    newbus_interface_slots[handle].used &&
	    newbus_interface_slots[handle].open_count > 0) {
		newbus_interface_slots[handle].open_count--;
		slot = 1;
	}
	if (slot > 0 && entity_mode) {
		entity_handle_free(NULL, handle);
	}
	smp_unlock();
}
