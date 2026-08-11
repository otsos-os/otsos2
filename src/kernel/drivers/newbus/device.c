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
 * LIABLE FOR ANY DIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type device_t as pointer to newbus device
$define %type driver_t as newbus driver descriptor
$define %type devclass_t as newbus device class descriptor
$define %type newbus_driver_entry_t as registered linker module wrapper

$define %func newbus_driver_add_module as function with args module
$define %func newbus_driver_remove_module as function with args module
$define %func newbus_driver_range_busy as function with args memory range
$define %func newbus_device_create_root as function with args const char *, int
$define %func newbus_device_set_driver as procedure with args device_t, driver_t *, devclass_t *
$define %func newbus_device_find_name as function with args const char *
$define %func newbus_device_find_nameunit as function with args const char *
$define %func device_add_child as function with args device_t, const char *, int
$define %func newbus_configure_pass as procedure with args int
$define %func newbus_reprobe as procedure with args void
$define %func newbus_shutdown as procedure with args void

*/

/* !SPACE!

$space %internal newbus_alloc_device, newbus_assign_unit
$space %internal newbus_run_identify, newbus_probe_device
$space %internal newbus_attach_device, newbus_configure_one_pass
$space %export newbus_driver_add_module, newbus_driver_remove_module
$space %export newbus_driver_range_busy, newbus_device_create_root
$space %export newbus_device_set_driver, device_add_child
$space %export newbus_device_find_name, newbus_device_find_nameunit
$space %export newbus_configure_pass, newbus_configure
$space %export newbus_reprobe, newbus_shutdown

*/

#include <kernel/drivers/newbus/newbus.h>
#include <kernel/smp/smp.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

typedef struct newbus_driver_entry {
	const newbus_module_t	*module;
} newbus_driver_entry_t;

static device_t		newbus_devices[NEWBUS_MAX_DEVICES];
static int		newbus_device_count;
static newbus_driver_entry_t newbus_drivers[NEWBUS_MAX_DRIVERS];
static int		newbus_driver_count;
static device_t		newbus_root_device;
static int		newbus_generation;
static int		newbus_configured_pass = -1;
static int		newbus_configure_busy;
static int		newbus_reprobe_pending;

static device_t
newbus_alloc_device(const char *name, int unit)
{
	device_t	dev;

	if (newbus_device_count >= NEWBUS_MAX_DEVICES) {
		drivers_log("[NEWBUS] device limit reached\n");
		return (NULL);
	}
	dev = (device_t)kmem_calloc(1, sizeof(*dev));
	if (dev == NULL) {
		return (NULL);
	}
	strncpy(dev->name, name, NEWBUS_NAME_MAX - 1);
	dev->unit = unit;
	dev->state = DS_ALIVE;
	snprintf(dev->nameunit, sizeof(dev->nameunit), "%s%d",
	    dev->name, dev->unit);
	dev->index = newbus_device_count;
	newbus_devices[newbus_device_count++] = dev;
	newbus_generation++;
	return (dev);
}

static int
newbus_assign_unit(const char *name)
{
	int	unit, i, used;

	for (unit = 0; unit < NEWBUS_MAX_DEVICES; unit++) {
		used = 0;
		for (i = 0; i < newbus_device_count; i++) {
			if (strcmp(newbus_devices[i]->name, name) == 0 &&
			    newbus_devices[i]->unit == unit) {
				used = 1;
				break;
			}
		}
		if (!used) {
			return (unit);
		}
	}
	return (-1);
}

int
newbus_driver_add_module(const newbus_module_t *module)
{
	int	i;

	if (module == NULL || module->driver == NULL ||
	    module->bus_name == NULL) {
		return (-1);
	}
	for (i = 0; i < newbus_driver_count; i++) {
		if (newbus_drivers[i].module == module ||
		    newbus_drivers[i].module->driver == module->driver) {
			return (0);
		}
	}
	if (newbus_driver_count >= NEWBUS_MAX_DRIVERS) {
		drivers_log("[NEWBUS] driver limit reached\n");
		return (-1);
	}
	newbus_drivers[newbus_driver_count++].module = module;
	if (module->driver->name != NULL) {
		drivers_log("[NEWBUS] driver %s on %s pass %d:%d\n",
		    module->driver->name, module->bus_name,
		    module->pass, module->order);
	}
	return (0);
}

static int
newbus_ptr_in_range(const void *ptr, const void *base, size_t size)
{
	u64	p, start, end;

	if (ptr == NULL || base == NULL || size == 0) {
		return (0);
	}
	p = (u64)ptr;
	start = (u64)base;
	end = start + (u64)size;
	if (end < start) {
		return (0);
	}
	return (p >= start && p < end);
}

int
newbus_driver_range_busy(const void *base, size_t size)
{
	const newbus_module_t	*module;
	device_t		dev;
	int			i;

	for (i = 0; i < newbus_driver_count; i++) {
		module = newbus_drivers[i].module;
		if (newbus_ptr_in_range(module, base, size) ||
		    (module != NULL &&
		    (newbus_ptr_in_range(module->driver, base, size) ||
		    newbus_ptr_in_range(module->devclass, base, size)))) {
			return (1);
		}
	}
	for (i = 0; i < newbus_device_count; i++) {
		dev = newbus_devices[i];
		if (dev == NULL) {
			continue;
		}
		if (newbus_ptr_in_range(dev->module, base, size) ||
		    newbus_ptr_in_range(dev->driver, base, size) ||
		    newbus_ptr_in_range(dev->devclass, base, size)) {
			return (1);
		}
	}
	return (0);
}

int
newbus_driver_remove_module(const newbus_module_t *module)
{
	device_t	dev;
	int		ret;
	int		i, j;

	if (module == NULL) {
		return (-1);
	}
	for (i = 0; i < newbus_device_count; i++) {
		dev = newbus_devices[i];
		if (dev == NULL) {
			continue;
		}
		if (dev->module != module &&
		    (module->driver == NULL || dev->driver != module->driver)) {
			continue;
		}
		if (dev->state == DS_ATTACHED) {
			if (dev->driver == NULL || dev->driver->detach == NULL) {
				return (-1);
			}
		}
	}
	for (i = 0; i < newbus_device_count; i++) {
		dev = newbus_devices[i];
		if (dev == NULL) {
			continue;
		}
		if (dev->module != module &&
		    (module->driver == NULL || dev->driver != module->driver)) {
			continue;
		}
		if (dev->state == DS_ATTACHED) {
			ret = dev->driver->detach(dev);
			if (ret != 0) {
				return (-1);
			}
		}
		dev->driver = NULL;
		dev->devclass = NULL;
		dev->module = NULL;
		dev->state = DS_ALIVE;
		newbus_entity_device_sync(dev);
		newbus_generation++;
	}
	for (i = 0; i < newbus_driver_count; i++) {
		if (newbus_drivers[i].module != module) {
			continue;
		}
		for (j = i + 1; j < newbus_driver_count; j++) {
			newbus_drivers[j - 1] = newbus_drivers[j];
		}
		memset(&newbus_drivers[newbus_driver_count - 1], 0,
		    sizeof(newbus_drivers[0]));
		newbus_driver_count--;
		newbus_generation++;
		return (0);
	}
	return (-1);
}

device_t
newbus_device_create_root(const char *name, int unit)
{
	if (newbus_root_device != NULL) {
		return (newbus_root_device);
	}
	newbus_root_device = newbus_alloc_device(name, unit);
	return (newbus_root_device);
}

void
newbus_device_set_driver(device_t dev, driver_t *driver,
    devclass_t *devclass)
{
	if (dev == NULL) {
		return;
	}
	dev->driver = driver;
	dev->devclass = devclass;
}

void
newbus_device_set_state(device_t dev, newbus_device_state_t state)
{
	if (dev != NULL) {
		dev->state = state;
	}
}

device_t
device_add_child(device_t parent, const char *name, int unit)
{
	device_t	dev, tail;

	if (parent == NULL || name == NULL) {
		return (NULL);
	}
	if (unit < 0) {
		unit = newbus_assign_unit(name);
	}
	if (unit < 0) {
		return (NULL);
	}
	dev = device_find_child(parent, name, unit);
	if (dev != NULL) {
		return (dev);
	}
	dev = newbus_alloc_device(name, unit);
	if (dev == NULL) {
		return (NULL);
	}
	dev->parent = parent;
	if (parent->child == NULL) {
		parent->child = dev;
	} else {
		tail = parent->child;
		while (tail->next != NULL) {
			tail = tail->next;
		}
		tail->next = dev;
	}
	drivers_log("[NEWBUS] add %s under %s\n",
	    dev->nameunit, parent->nameunit);
	return (dev);
}

device_t
device_find_child(device_t parent, const char *name, int unit)
{
	device_t	child;

	if (parent == NULL || name == NULL) {
		return (NULL);
	}
	for (child = parent->child; child != NULL; child = child->next) {
		if (strcmp(child->name, name) != 0) {
			continue;
		}
		if (unit >= 0 && child->unit != unit) {
			continue;
		}
		return (child);
	}
	return (NULL);
}

device_t
device_get_parent(device_t dev)
{
	return (dev == NULL ? NULL : dev->parent);
}

device_t
device_get_child(device_t dev)
{
	return (dev == NULL ? NULL : dev->child);
}

device_t
device_get_next(device_t dev)
{
	return (dev == NULL ? NULL : dev->next);
}

const char *
device_get_name(device_t dev)
{
	return (dev == NULL ? NULL : dev->name);
}

const char *
device_get_nameunit(device_t dev)
{
	return (dev == NULL ? "none" : dev->nameunit);
}

int
device_get_unit(device_t dev)
{
	return (dev == NULL ? -1 : dev->unit);
}

newbus_device_state_t
device_get_state(device_t dev)
{
	return (dev == NULL ? DS_NOTPRESENT : dev->state);
}

driver_t *
device_get_driver(device_t dev)
{
	return (dev == NULL ? NULL : dev->driver);
}

void *
device_get_softc(device_t dev)
{
	return (dev == NULL ? NULL : dev->softc);
}

void
device_set_softc(device_t dev, void *softc)
{
	if (dev != NULL) {
		dev->softc = softc;
	}
}

void *
device_get_ivars(device_t dev)
{
	return (dev == NULL ? NULL : dev->ivars);
}

void
device_set_ivars(device_t dev, void *ivars)
{
	if (dev != NULL) {
		dev->ivars = ivars;
	}
}

device_t
newbus_device_find_name(const char *name)
{
	device_t	dev;
	int		i;

	if (name == NULL || name[0] == '\0') {
		return (NULL);
	}
	dev = NULL;
	smp_lock();
	for (i = 0; i < newbus_device_count; i++) {
		if (strcmp(newbus_devices[i]->name, name) == 0) {
			dev = newbus_devices[i];
			break;
		}
	}
	smp_unlock();
	return (dev);
}

device_t
newbus_device_find_nameunit(const char *nameunit)
{
	device_t	dev;
	int		i;

	if (nameunit == NULL || nameunit[0] == '\0') {
		return (NULL);
	}
	dev = NULL;
	smp_lock();
	for (i = 0; i < newbus_device_count; i++) {
		if (strcmp(newbus_devices[i]->nameunit, nameunit) == 0) {
			dev = newbus_devices[i];
			break;
		}
	}
	smp_unlock();
	return (dev);
}

static int
newbus_driver_order_before(const newbus_module_t *a,
    const newbus_module_t *b)
{
	if (a->pass < b->pass) {
		return (1);
	}
	if (a->pass > b->pass) {
		return (0);
	}
	return (a->order < b->order);
}

static int
newbus_module_failed_on_device(device_t dev, const newbus_module_t *module)
{
	int	i;

	if (dev == NULL || module == NULL) {
		return (0);
	}
	for (i = 0; i < dev->failed_module_count; i++) {
		if (dev->failed_modules[i] == module) {
			return (1);
		}
	}
	return (0);
}

static int
newbus_module_matches_device(device_t dev, const newbus_module_t *module)
{
	if (dev == NULL || module == NULL || module->driver == NULL) {
		return (0);
	}
	if (strcmp(module->bus_name, "pci") == 0 ||
	    strcmp(module->bus_name, "usb") == 0) {
		return (1);
	}
	if (module->name != NULL && strcmp(module->name, dev->name) == 0) {
		return (1);
	}
	if (module->driver->name != NULL &&
	    strcmp(module->driver->name, dev->name) == 0) {
		return (1);
	}
	return (0);
}

static int
newbus_record_failed_module(device_t dev, const newbus_module_t *module)
{
	if (dev == NULL || module == NULL ||
	    newbus_module_failed_on_device(dev, module)) {
		return (0);
	}
	if (dev->failed_module_count >= NEWBUS_MAX_FAILED_MODULES) {
		return (-1);
	}
	dev->failed_modules[dev->failed_module_count++] = module;
	return (0);
}

static void
newbus_run_identify(int pass)
{
	const newbus_module_t	*module;
	device_t		parent;
	int			i, j;

	for (i = 0; i < newbus_driver_count; i++) {
		module = newbus_drivers[i].module;
		if (module->pass > pass || module->driver->identify == NULL) {
			continue;
		}
		if (!newbus_config_driver_enabled(module) ||
		    !newbus_config_bus_enabled(module->bus_name)) {
			continue;
		}
		for (j = 0; j < newbus_device_count; j++) {
			parent = newbus_devices[j];
			if (parent->state != DS_ATTACHED) {
				continue;
			}
			if (strcmp(parent->name, module->bus_name) != 0) {
				continue;
			}
			module->driver->identify(module->driver, parent);
		}
	}
}

static int
newbus_probe_device(device_t dev, int pass)
{
	const newbus_module_t	*best, *module;
	device_t		parent;
	int			best_score, score;
	int			i;

	if (dev == NULL || dev->driver != NULL ||
	    dev->state >= DS_PROBED) {
		return (0);
	}
	if (!newbus_config_device_enabled(dev)) {
		return (0);
	}
	parent = dev->parent;
	if (parent == NULL || parent->state != DS_ATTACHED) {
		return (0);
	}
	best = NULL;
	best_score = -1000000;
	for (i = 0; i < newbus_driver_count; i++) {
		module = newbus_drivers[i].module;
		if (module->pass > pass ||
		    (strcmp(module->bus_name, parent->name) != 0 &&
		    (parent->driver == NULL ||
		    strcmp(module->bus_name, parent->driver->name) != 0))) {
			continue;
		}
		if (newbus_module_failed_on_device(dev, module)) {
			continue;
		}
		if (!newbus_module_matches_device(dev, module)) {
			continue;
		}
		if (!newbus_config_probe_allowed(dev, module)) {
			continue;
		}
		if (module->driver->probe != NULL) {
			score = module->driver->probe(dev);
			if (score < 0) {
				continue;
			}
		} else if (strcmp(module->driver->name, dev->name) == 0) {
			score = 0;
		} else {
			continue;
		}
		if (best == NULL || score > best_score ||
		    (score == best_score &&
		    newbus_driver_order_before(module, best))) {
			best = module;
			best_score = score;
		}
	}
	if (best == NULL) {
		return (0);
	}
	dev->driver = best->driver;
	dev->devclass = best->devclass;
	dev->module = best;
	dev->state = DS_PROBED;
	drivers_log("[NEWBUS] %s probed by %s\n",
	    dev->nameunit, best->driver->name);
	newbus_generation++;
	return (1);
}

static int
newbus_attach_device(device_t dev)
{
	int	ret;

	if (dev == NULL || dev->driver == NULL ||
	    dev->state != DS_PROBED) {
		return (0);
	}
	if (!newbus_config_probe_allowed(dev, dev->module)) {
		dev->driver = NULL;
		dev->devclass = NULL;
		dev->module = NULL;
		dev->state = DS_ALIVE;
		return (0);
	}
	ret = 0;
	if (dev->driver->attach != NULL) {
		ret = dev->driver->attach(dev);
	}
	if (ret != 0) {
		drivers_log("[NEWBUS] attach failed: %s by %s (%d)\n",
		    dev->nameunit, dev->driver->name, ret);
		if (newbus_record_failed_module(dev, dev->module) != 0) {
			drivers_log("[NEWBUS] too many failed drivers for %s\n",
			    dev->nameunit);
			dev->state = DS_DETACHED;
		}
		dev->driver = NULL;
		dev->devclass = NULL;
		dev->module = NULL;
		if (dev->state != DS_DETACHED) {
			dev->state = DS_ALIVE;
		}
		return (0);
	}
	dev->state = DS_ATTACHED;
	drivers_log("[NEWBUS] attached %s to %s\n",
	    dev->nameunit, dev->driver->name);
	newbus_entity_device_sync(dev);
	newbus_generation++;
	return (1);
}

static void
newbus_attach_devices(int pass)
{
	int	i, order;

	for (order = NEWBUS_ORDER_FIRST; order <= NEWBUS_ORDER_LAST;
	    order++) {
		for (i = 0; i < newbus_device_count; i++) {
			if (newbus_devices[i]->state != DS_PROBED ||
			    newbus_devices[i]->module == NULL) {
				continue;
			}
			if (newbus_devices[i]->module->pass > pass ||
			    newbus_devices[i]->module->order != order) {
				continue;
			}
			newbus_attach_device(newbus_devices[i]);
		}
	}
}

static void
newbus_configure_one_pass(int pass)
{
	int	i, old_generation;

	if (newbus_configure_busy) {
		newbus_reprobe_pending = 1;
		return;
	}
	newbus_configure_busy = 1;
	do {
		old_generation = newbus_generation;
		newbus_run_identify(pass);
		for (i = 0; i < newbus_device_count; i++) {
			newbus_probe_device(newbus_devices[i], pass);
		}
		newbus_attach_devices(pass);
	} while (old_generation != newbus_generation);
	newbus_configure_busy = 0;
	if (newbus_reprobe_pending) {
		newbus_reprobe_pending = 0;
		newbus_configure_one_pass(pass);
	}
}

void
newbus_configure_pass(int pass)
{
	int	next;

	newbus_register_linker_modules();
	if (newbus_root_device == NULL) {
		return;
	}
	for (next = newbus_configured_pass + 1; next <= pass; next++) {
		newbus_configure_one_pass(next);
	}
	newbus_configured_pass = pass;
}

void
newbus_configure(void)
{
	newbus_configure_pass(NEWBUS_PASS_LATE);
}

void
newbus_reprobe(void)
{
	if (newbus_root_device == NULL) {
		return;
	}
	if (newbus_configured_pass < 0) {
		newbus_configure_pass(NEWBUS_PASS_LATE);
		return;
	}
	newbus_configure_one_pass(newbus_configured_pass);
}

void
newbus_shutdown(void)
{
	device_t	dev;
	int	i;

	for (i = newbus_device_count - 1; i >= 0; i--) {
		dev = newbus_devices[i];
		if (dev->state == DS_ATTACHED && dev->driver != NULL &&
		    dev->driver->shutdown != NULL) {
			dev->driver->shutdown(dev);
		}
	}
}

int
newbus_device_count_get(void)
{
	return (newbus_device_count);
}

device_t
newbus_device_get(int index)
{
	if (index < 0 || index >= newbus_device_count) {
		return (NULL);
	}
	return (newbus_devices[index]);
}

int
newbus_driver_count_get(void)
{
	return (newbus_driver_count);
}

const newbus_module_t *
newbus_driver_module_get(int index)
{
	if (index < 0 || index >= newbus_driver_count) {
		return (NULL);
	}
	return (newbus_drivers[index].module);
}
