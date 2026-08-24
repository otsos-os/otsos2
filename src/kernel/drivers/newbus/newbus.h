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

$define %type device_t as pointer to newbus device
$define %type driver_t as newbus driver descriptor
$define %type devclass_t as newbus device class descriptor
$define %type resource_t as allocated bus resource
$define %type newbus_module_t as linker-set driver declaration
$define %type newbus_bootinfo_t as boot-discovered firmware resource state
$define %type newbus_device_state_t as device lifecycle state enum
$define %type newbus_interface_t as named driver I/O interface table

$define %func newbus_bootstrap as procedure with args newbus_bootinfo_t *
$define %func newbus_register_linker_modules as procedure with args void
$define %func newbus_configure_pass as procedure with args int
$define %func newbus_configure as procedure with args void
$define %func newbus_reprobe as procedure with args void
$define %func newbus_driver_add_module as function with args module
$define %func newbus_driver_remove_module as function with args module
$define %func newbus_driver_range_busy as function with args memory range
$define %func device_add_child as function with args device_t, const char *, int
$define %func device_delete_child as function with args device_t, device_t
$define %func device_find_child as function with args device_t, const char *, int
$define %func device_get_name as function with args device_t
$define %func device_get_nameunit as function with args device_t
$define %func newbus_device_count_get as function with args void
$define %func newbus_device_get as function with args int
$define %func newbus_driver_count_get as function with args void
$define %func newbus_driver_module_get as function with args int
$define %func newbus_interface_register as function with args device_t, interface
$define %func newbus_interface_unregister as function with args device_t, interface
$define %func newbus_interface_count as function with args device_t
$define %func newbus_interface_get as function with args device_t, int
$define %func newbus_interface_find as function with args device_t, const char *
$define %func newbus_interface_open as function with args device_t, const char *
$define %func newbus_interface_close as procedure with args int
$define %func newbus_device_find_name as function with args const char *
$define %func newbus_device_find_nameunit as function with args const char *
$define %func newbus_config_driver_enabled as function with args const newbus_module_t *
$define %func newbus_config_device_enabled as function with args device_t
$define %func newbus_config_bus_enabled as function with args const char *
$define %func newbus_config_probe_allowed as function with args device_t, const newbus_module_t *
$define %func newbus_config_driver_get_bool as function with args const char *, const char *, int
$define %func newbus_config_driver_get_u32 as function with args const char *, const char *, u32
$define %func newbus_config_driver_get_string as function with args const char *, const char *, char *, u32, const char *
$define %func bus_alloc_resource as function with args device_t, int, int *, u64, u64, u32
$define %func bus_setup_intr as function with args device_t, resource_t *, newbus_intr_handler_t, void *, void **
$define %func bus_setup_poll as function with args device_t, u32, newbus_poll_handler_t, void *, void **
$define %func newbus_poll_dispatch as procedure with args u32
$define %func newbus_entity_init as procedure with args void
$define %func newbus_entity_device_sync as procedure with args device_t
$define %func newbus_interface_read_entity as function with args entity id, buffer, count, offset
$define %func newbus_interface_write_entity as function with args entity id, buffer, count, offset
$define %func newbus_interface_ioctl_entity as function with args entity id, command, argument
$define %func newbus_interface_stat_entity as function with args entity id, size

*/

/* !SPACE!

$space %export newbus_bootstrap, newbus_register_linker_modules
$space %export newbus_configure_pass, newbus_configure, newbus_reprobe
$space %export newbus_driver_add_module, newbus_driver_remove_module
$space %export newbus_driver_range_busy
$space %export device_add_child, device_find_child
$space %export device_delete_child
$space %export device_get_name, device_get_nameunit
$space %export newbus_device_count_get, newbus_device_get
$space %export newbus_driver_count_get, newbus_driver_module_get
$space %export newbus_interface_register, newbus_interface_unregister
$space %export newbus_interface_count, newbus_interface_get
$space %export newbus_interface_find, newbus_interface_open
$space %export newbus_interface_close
$space %export newbus_device_find_name, newbus_device_find_nameunit
$space %export newbus_config_driver_enabled
$space %export newbus_config_device_enabled, newbus_config_bus_enabled
$space %export newbus_config_probe_allowed
$space %export newbus_config_driver_get_bool
$space %export newbus_config_driver_get_u32
$space %export newbus_config_driver_get_string
$space %export bus_alloc_resource, bus_setup_intr
$space %export bus_setup_poll, newbus_poll_dispatch
$space %export newbus_entity_init, newbus_entity_device_sync
$space %export newbus_interface_read_entity, newbus_interface_write_entity
$space %export newbus_interface_ioctl_entity, newbus_interface_stat_entity

*/

#ifndef KERNEL_DRIVERS_NEWBUS_NEWBUS_H
#define KERNEL_DRIVERS_NEWBUS_NEWBUS_H

#include <mlibc/mlibc.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/entity/entity.h>

#define	NEWBUS_MAX_DEVICES		256
#define	NEWBUS_MAX_DRIVERS		256
#define	NEWBUS_MAX_RESOURCES		16
#define	NEWBUS_MAX_FAILED_MODULES	8
#define	NEWBUS_MAX_INTR_HANDLERS	128
#define	NEWBUS_MAX_POLL_HOOKS		128
#define	NEWBUS_MAX_INTERFACES		256
#define	NEWBUS_NAME_MAX		32
#define	NEWBUS_NAMEUNIT_MAX	48

#define	NEWBUS_PASS_ROOT		0
#define	NEWBUS_PASS_BUS		10
#define	NEWBUS_PASS_FIRMWARE		20
#define	NEWBUS_PASS_INTERRUPT		30
#define	NEWBUS_PASS_TIMER		40
#define	NEWBUS_PASS_CORE		50
#define	NEWBUS_PASS_STORAGE		60
#define	NEWBUS_PASS_INPUT		70
#define	NEWBUS_PASS_DISPLAY		80
#define	NEWBUS_PASS_NETWORK		90
#define	NEWBUS_PASS_FILESYSTEM		100
#define	NEWBUS_PASS_LATE		110

#define	NEWBUS_ORDER_FIRST		0
#define	NEWBUS_ORDER_EARLY		25
#define	NEWBUS_ORDER_MIDDLE		50
#define	NEWBUS_ORDER_LATE		75
#define	NEWBUS_ORDER_LAST		100

#define	SYS_RES_IOPORT		1
#define	SYS_RES_MEMORY		2
#define	SYS_RES_IRQ		3
#define	SYS_RES_DMA		4
#define	SYS_RES_BOOTMEM		5
#define	SYS_RES_FRAMEBUFFER	6
#define	SYS_RES_MODULE		7
#define	SYS_RES_CLOCK		8
#define	SYS_RES_TIMER		9

#define	RF_ALLOCATED		0x0001
#define	RF_ACTIVE		0x0002
#define	RF_SHAREABLE		0x0004
#define	RF_BUSY			0x0008
#define	RF_IRQ_LEVEL		0x0010
#define	RF_IRQ_ACTIVE_LOW	0x0020
#define	RF_IRQ_ISA		0x0040
#define	RF_IRQ_GSI		0x0080

#define	NB_POLL_TIMER		0x0001
#define	NB_POLL_IDLE		0x0002
#define	NB_POLL_SHUTDOWN	0x0004

typedef enum newbus_device_state {
	DS_NOTPRESENT = 0,
	DS_ALIVE,
	DS_IDENTIFIED,
	DS_PROBED,
	DS_ATTACHED,
	DS_SUSPENDED,
	DS_DETACHED
} newbus_device_state_t;

struct newbus_device;
struct newbus_driver;
struct newbus_devclass;
struct newbus_resource;

typedef struct newbus_device		*device_t;
typedef struct newbus_driver		driver_t;
typedef struct newbus_devclass		devclass_t;
typedef struct newbus_resource		resource_t;
typedef int				(newbus_intr_handler_t)(void *);
typedef void				(newbus_poll_handler_t)(void *);

typedef struct newbus_bootinfo {
	u32	magic;
	void	*mb1;
	void	*mb2;
	void	*module_pool;
	u32	module_pool_size;
	u32	timer_hz;
	int	disable_apic;
	int	debug_mode;
} newbus_bootinfo_t;

typedef struct newbus_module {
	const char	*name;
	const char	*bus_name;
	driver_t	*driver;
	devclass_t	*devclass;
	int		pass;
	int		order;
} newbus_module_t;

struct newbus_resource {
	device_t	owner;
	int	type;
	int	rid;
	u64	start;
	u64	count;
	u32	flags;
	void	*cookie;
};

struct newbus_device {
	char			name[NEWBUS_NAME_MAX];
	char			nameunit[NEWBUS_NAMEUNIT_MAX];
	int			unit;
	newbus_device_state_t	state;
	device_t		parent;
	device_t		child;
	device_t		next;
	driver_t		*driver;
	devclass_t		*devclass;
	const newbus_module_t	*module;
	void			*softc;
	void			*ivars;
	const newbus_module_t	*failed_modules[NEWBUS_MAX_FAILED_MODULES];
	int			failed_module_count;
	int			index;
	resource_t		resources[NEWBUS_MAX_RESOURCES];
	int			resource_count;
	u64			entity;
};

struct newbus_devclass {
	const char	*name;
	int		maxunit;
};

struct newbus_driver {
	const char	*name;
	void		(*identify)(driver_t *driver, device_t parent);
	int		(*probe)(device_t dev);
	int		(*attach)(device_t dev);
	int		(*detach)(device_t dev);
	int		(*suspend)(device_t dev);
	int		(*resume)(device_t dev);
	void		(*shutdown)(device_t dev);
	void		*priv;
};

typedef struct newbus_interface {
	const char	*name;
	int		(*read)(device_t dev, void *buf, u64 count,
			    u64 offset);
	int		(*write)(device_t dev, const void *buf, u64 count,
			    u64 offset);
	int		(*ioctl)(device_t dev, u64 cmd, void *arg);
	int		(*stat)(device_t dev, posix_stat_t *st);
} newbus_interface_t;

#define	NEWBUS_SECTION	__attribute__((used, section(".newbus.drivers")))

#define	DRIVER_MODULE(modname, busname, drv_sym, devclass_sym, passv, orderv) \
	static const newbus_module_t __newbus_module_##modname \
	    NEWBUS_SECTION = { \
		.name = #modname, \
		.bus_name = #busname, \
		.driver = &(drv_sym), \
		.devclass = &(devclass_sym), \
		.pass = (passv), \
		.order = (orderv), \
	}

#define	PLATFORM_DRIVER_MODULE(modname, drv_sym, devclass_sym, passv, orderv) \
	DRIVER_MODULE(modname, platform, drv_sym, devclass_sym, passv, orderv)

#define	ISA_DRIVER_MODULE(modname, drv_sym, devclass_sym, passv, orderv) \
	DRIVER_MODULE(modname, isa, drv_sym, devclass_sym, passv, orderv)

#define	FIRMWARE_DRIVER_MODULE(modname, drv_sym, devclass_sym, passv, orderv) \
	DRIVER_MODULE(modname, firmware, drv_sym, devclass_sym, passv, orderv)

#define	PSEUDO_DRIVER_MODULE(modname, drv_sym, devclass_sym, passv, orderv) \
	DRIVER_MODULE(modname, pseudo, drv_sym, devclass_sym, passv, orderv)

void		newbus_bootstrap(newbus_bootinfo_t *bootinfo);
void		newbus_update_bootinfo(newbus_bootinfo_t *bootinfo);
const newbus_bootinfo_t	*newbus_get_bootinfo(void);
void		newbus_register_linker_modules(void);
void		newbus_configure_pass(int pass);
void		newbus_configure(void);
void		newbus_reprobe(void);
void		newbus_shutdown(void);
void		newbus_dump_tree(void);
void		newbus_dump_drivers(void);
int		newbus_device_count_get(void);
device_t	newbus_device_get(int index);
int		newbus_driver_count_get(void);
const newbus_module_t	*newbus_driver_module_get(int index);
int		newbus_config_driver_enabled(const newbus_module_t *module);
int		newbus_config_device_enabled(device_t dev);
int		newbus_config_bus_enabled(const char *bus_name);
int		newbus_config_probe_allowed(device_t dev,
		    const newbus_module_t *module);
int		newbus_config_driver_get_bool(const char *driver_name,
		    const char *value, int default_val);
u32		newbus_config_driver_get_u32(const char *driver_name,
		    const char *value, u32 default_val);
int		newbus_config_driver_get_string(const char *driver_name,
		    const char *value, char *out, u32 out_size,
		    const char *default_val);

int		newbus_driver_add_module(const newbus_module_t *module);
int		newbus_driver_remove_module(const newbus_module_t *module);
int		newbus_driver_range_busy(const void *base, size_t size);
device_t	newbus_device_create_root(const char *name, int unit);
void		newbus_device_set_driver(device_t dev, driver_t *driver,
		    devclass_t *devclass);
void		newbus_device_set_state(device_t dev,
		    newbus_device_state_t state);

device_t	device_add_child(device_t parent, const char *name, int unit);
int		device_delete_child(device_t parent, device_t child);
device_t	device_find_child(device_t parent, const char *name, int unit);
device_t	device_get_parent(device_t dev);
device_t	device_get_child(device_t dev);
device_t	device_get_next(device_t dev);
const char	*device_get_name(device_t dev);
const char	*device_get_nameunit(device_t dev);
int		device_get_unit(device_t dev);
newbus_device_state_t	device_get_state(device_t dev);
driver_t	*device_get_driver(device_t dev);
void		*device_get_softc(device_t dev);
void		device_set_softc(device_t dev, void *softc);
void		*device_get_ivars(device_t dev);
void		device_set_ivars(device_t dev, void *ivars);
device_t	newbus_device_find_name(const char *name);
device_t	newbus_device_find_nameunit(const char *nameunit);

int		newbus_interface_register(device_t dev,
		    const newbus_interface_t *iface);
int		newbus_interface_unregister(device_t dev,
		    const newbus_interface_t *iface);
int		newbus_interface_count(device_t dev);
const newbus_interface_t	*newbus_interface_get(device_t dev,
		    int index);
const newbus_interface_t	*newbus_interface_find(device_t dev,
		    const char *name);
int		newbus_interface_open(device_t dev, const char *name);
void		newbus_interface_close(int handle);

int		bus_set_resource(device_t dev, int type, int rid,
		    u64 start, u64 count, u32 flags);
resource_t	*bus_get_resource(device_t dev, int type, int rid);
resource_t	*bus_alloc_resource(device_t dev, int type, int *rid,
		    u64 start, u64 count, u32 flags);
resource_t	*bus_alloc_resource_any(device_t dev, int type, int *rid,
		    u32 flags);
int		bus_release_resource(device_t dev, int type, int rid,
		    resource_t *res);
int		bus_activate_resource(device_t dev, resource_t *res);
int		bus_deactivate_resource(device_t dev, resource_t *res);

int		bus_setup_intr(device_t dev, resource_t *res,
		    newbus_intr_handler_t *handler, void *arg,
		    void **cookiep);
int		bus_teardown_intr(device_t dev, resource_t *res,
		    void *cookie);

int		bus_setup_poll(device_t dev, u32 event,
		    newbus_poll_handler_t *handler, void *arg,
		    void **cookiep);
int		bus_teardown_poll(device_t dev, void *cookie);
void		newbus_poll_dispatch(u32 event);

void		newbus_entity_init(void);
void		newbus_entity_device_sync(device_t dev);
int		newbus_interface_read_entity(entity_id_t id, void *buf,
		    u64 count, u64 offset);
int		newbus_interface_write_entity(entity_id_t id,
		    const void *buf, u64 count, u64 offset);
int		newbus_interface_ioctl_entity(entity_id_t id, u64 cmd,
		    void *arg);
int		newbus_interface_stat_entity(entity_id_t id, u64 *size);

#endif
