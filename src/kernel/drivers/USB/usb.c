/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type usb_desc_header_t as packed USB descriptor header
$define %type usb_device_desc_t as packed USB device descriptor
$define %type usb_config_desc_t as packed USB configuration descriptor
$define %type usb_interface_desc_t as packed USB interface descriptor
$define %type usb_endpoint_desc_t as packed USB endpoint descriptor

$define %func usb_parse_configuration as function with args usb_device_t *, const u8 *, u16
$define %func usb_enumerate_port as function with args usb_controller_t *, u8, u8
$define %func usb_controller_init as function with args usb_controller_t *, ops, void *, device_t, u8
$define %func usb_controller_scan as procedure with args usb_controller_t *
$define %func usb_controller_fini as procedure with args usb_controller_t *
$define %func usb_dma_alloc as function with args usb_dma_t *, u32, u32, u64
$define %func usb_dma_free as procedure with args usb_dma_t *
$define %func usb_control_transfer as function with args usb_device_t *, setup, void *, u16, u32
$define %func usb_bulk_transfer as function with args usb_device_t *, endpoint, void *, u32 *, u32
$define %func usb_bulk_submit as function with args usb_device_t *, endpoint, void *, u32, callback, void *
$define %func usb_interrupt_submit as function with args usb_device_t *, endpoint, void *, u32, callback, void *
$define %func usb_set_interface as function with args usb_device_t *, u8, u8
$define %func usb_interface_get as function with args device_t
$define %func usb_log_printf as function with args const char *, ...
$define %func usb_log_flush as procedure with args void
$define %func usb_logflush_identify as procedure with args driver_t *, device_t
$define %func usb_logflush_attach as function with args device_t


*/

/* !SPACE!

$space %internal usb_parse_configuration, usb_enumerate_port
$space %export usb_controller_init, usb_controller_scan, usb_controller_fini
$space %export usb_dma_alloc, usb_dma_free
$space %export usb_control_transfer, usb_bulk_transfer, usb_interrupt_submit
$space %export usb_bulk_submit
$space %export usb_set_interface
$space %export usb_interface_get
$space %export usb_log_printf, usb_log_flush
$space %internal usb_logflush_identify, usb_logflush_attach
*/

#include <kernel/drivers/USB/usb.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/mm/vm/vm_page.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	USB_LOG_PATH		"/log.txt"
#define	USB_LOG_SIZE		32768
#define	USB_PORT_RETRY_PASSES	100
#define	USB_PORT_FAIL_LIMIT	5

static char		usb_log_buf[USB_LOG_SIZE];
static u32		usb_log_len;

int
usb_dma_alloc(usb_dma_t *dma, u32 size, u32 alignment, u64 max_address)
{
	u64	phys;
	u32	pages;

	if (dma == NULL || size == 0 || alignment == 0) {
		return (-1);
	}
	pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	phys = vm_page_alloc_contig(pages, alignment, max_address);
	if (phys == 0) {
		return (-1);
	}
	dma->virt = (void *)(phys + DMAP_BASE);
	dma->phys = phys;
	dma->size = pages * PAGE_SIZE;
	dma->page_count = pages;
	memset(dma->virt, 0, dma->size);
	return (0);
}

void
usb_dma_free(usb_dma_t *dma)
{
	if (dma == NULL || dma->virt == NULL) {
		return;
	}
	vm_page_free_contig(dma->phys, dma->page_count);
	memset(dma, 0, sizeof(*dma));
}

int
usb_log_printf(const char *fmt, ...)
{
	__builtin_va_list	args;
	char			line[256];
	int			n;

	__builtin_va_start(args, fmt);
	vsnprintf(line, sizeof(line), fmt, args);
	__builtin_va_end(args);
	printk("%s", line);
	n = (int)strlen(line);
	if (n > 0 && usb_log_len + (u32)n < USB_LOG_SIZE) {
		memcpy(usb_log_buf + usb_log_len, line, (u32)n);
		usb_log_len += (u32)n;
	}
	(void)vfs_write_file(USB_LOG_PATH, (const u8 *)usb_log_buf,
	    usb_log_len);
	return (0);
}

void
usb_log_flush(void)
{
	if (usb_log_len == 0) {
		return;
	}
	(void)vfs_write_file(USB_LOG_PATH, (const u8 *)usb_log_buf,
	    usb_log_len);
}

typedef struct {
	u8	length;
	u8	type;
} __attribute__((packed)) usb_desc_header_t;

typedef struct {
	u8	length;
	u8	type;
	u16	bcd_usb;
	u8	device_class;
	u8	device_subclass;
	u8	device_protocol;
	u8	max_packet_size0;
	u16	vendor_id;
	u16	product_id;
	u16	bcd_device;
	u8	manufacturer;
	u8	product;
	u8	serial;
	u8	configuration_count;
} __attribute__((packed)) usb_device_desc_t;

typedef struct {
	u8	length;
	u8	type;
	u16	total_length;
	u8	interface_count;
	u8	configuration_value;
	u8	configuration;
	u8	attributes;
	u8	max_power;
} __attribute__((packed)) usb_config_desc_t;

typedef struct {
	u8	length;
	u8	type;
	u8	interface_number;
	u8	alternate_setting;
	u8	endpoint_count;
	u8	interface_class;
	u8	interface_subclass;
	u8	interface_protocol;
	u8	interface;
} __attribute__((packed)) usb_interface_desc_t;

typedef struct {
	u8	length;
	u8	type;
	u8	endpoint_address;
	u8	attributes;
	u16	max_packet_size;
	u8	interval;
} __attribute__((packed)) usb_endpoint_desc_t;

static int
usb_parse_configuration(usb_device_t *dev, const u8 *buffer, u16 length)
{
	const usb_desc_header_t	*header;
	const usb_interface_desc_t	*interface;
	const usb_endpoint_desc_t	*endpoint;
	usb_interface_t		*current;
	u16				offset;

	if (dev == NULL || buffer == NULL || length < sizeof(usb_config_desc_t)) {
		return (-1);
	}
	dev->interface_count = 0;
	current = NULL;
	for (offset = 0; offset + sizeof(*header) <= length;) {
		header = (const usb_desc_header_t *)(buffer + offset);
		if (header->length < sizeof(*header) ||
		    header->length > length - offset) {
			return (-1);
		}
		if (header->type == USB_DESC_INTERFACE &&
		    header->length >= sizeof(*interface) &&
		    dev->interface_count < USB_MAX_INTERFACES) {
			interface = (const usb_interface_desc_t *)header;
			current = &dev->interfaces[dev->interface_count++];
			memset(current, 0, sizeof(*current));
			current->number = interface->interface_number;
			current->alternate = interface->alternate_setting;
			current->class_code = interface->interface_class;
			current->subclass = interface->interface_subclass;
			current->protocol = interface->interface_protocol;
			current->device = dev;
		} else if (header->type == USB_DESC_ENDPOINT && current != NULL &&
		    header->length >= sizeof(*endpoint) &&
		    current->endpoint_count < USB_MAX_ENDPOINTS) {
			endpoint = (const usb_endpoint_desc_t *)header;
			current->endpoints[current->endpoint_count].address =
			    endpoint->endpoint_address;
			current->endpoints[current->endpoint_count].attributes =
			    endpoint->attributes;
			current->endpoints[current->endpoint_count].max_packet_size =
			    endpoint->max_packet_size;
			current->endpoints[current->endpoint_count].interval =
			    endpoint->interval;
			current->endpoint_count++;
		}
		offset += header->length;
	}
	return (dev->interface_count == 0 ? -1 : 0);
}

static int
usb_bus_attach(device_t dev)
{
	(void)dev;
	return (0);
}

static devclass_t usb_bus_devclass = {
	.name		= "usb",
	.maxunit	= USB_MAX_DEVICES,
};

static driver_t usb_bus_driver = {
	.name		= "usb",
	.identify	= NULL,
	.probe		= NULL,
	.attach		= usb_bus_attach,
};

static driver_t usb_bus_ehci_driver = {
	.name		= "usb",
	.identify	= NULL,
	.probe		= NULL,
	.attach		= usb_bus_attach,
};

static driver_t usb_bus_ohci_driver = {
	.name		= "usb",
	.identify	= NULL,
	.probe		= NULL,
	.attach		= usb_bus_attach,
};

DRIVER_MODULE(usb_bus, xhci, usb_bus_driver, usb_bus_devclass,
    NEWBUS_PASS_STORAGE, NEWBUS_ORDER_MIDDLE);

DRIVER_MODULE(usb_bus_ehci, ehci, usb_bus_ehci_driver, usb_bus_devclass,
    NEWBUS_PASS_STORAGE, NEWBUS_ORDER_MIDDLE);

DRIVER_MODULE(usb_bus_ohci, ohci, usb_bus_ohci_driver, usb_bus_devclass,
	NEWBUS_PASS_STORAGE, NEWBUS_ORDER_MIDDLE);

static int
usb_ivars_belongs(device_t child, usb_device_t *dev)
{
	void	*ivars;
	u64	p, start, end;

	if (child == NULL || dev == NULL) {
		return (0);
	}
	ivars = device_get_ivars(child);
	if (ivars == NULL) {
		return (0);
	}
	p = (u64)ivars;
	start = (u64)dev->interfaces;
	end = start + sizeof(dev->interfaces);
	return (p >= start && p < end);
}

static int
usb_enumerate_port(usb_controller_t *controller, u8 port, u8 speed)
{
	usb_config_desc_t	config;
	usb_device_desc_t	device_desc;
	usb_device_t		*dev;
	usb_setup_t		setup;
	u8				*config_data;
	u16				total_length;
	device_t			child;
	int				index;

	dev = kmem_calloc(1, sizeof(*dev));
	if (dev == NULL) {
		return (-1);
	}
	dev->controller = controller;
	dev->port = port;
	dev->speed = speed;
	dev->max_packet_size0 = 8;
	if (controller->ops->address_device(controller->priv, dev) != 0) {
		kmem_free(dev);
		return (-1);
	}
	memset(&setup, 0, sizeof(setup));
	setup.bmRequestType = USB_DIR_IN | USB_REQ_TYPE_STANDARD |
	    USB_RECIP_DEVICE;
	setup.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup.wValue = USB_DESC_DEVICE << 8;
	setup.wLength = 8;
	if (usb_control_transfer(dev, &setup, &device_desc,
	    8, 1000) != 0) {
		usb_log_printf("usb: port %u: get device descriptor failed\n",
		    port);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	if (device_desc.length < 8 || device_desc.type != USB_DESC_DEVICE) {
		usb_log_printf("usb: port %u: bad short device descriptor "
		    "len=%u type=%u\n", port, device_desc.length,
		    device_desc.type);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	usb_log_printf("usb: port %u: dev8=%02x %02x %04x %02x %02x %02x "
	    "%02x\n", port, device_desc.length, device_desc.type,
	    device_desc.bcd_usb, device_desc.device_class,
	    device_desc.device_subclass, device_desc.device_protocol,
	    device_desc.max_packet_size0);
	if (dev->speed >= USB_SPEED_SUPER) {
		if (device_desc.max_packet_size0 > 11) {
			usb_log_printf("usb: port %u: bad super-speed ep0 "
			    "size %u\n", port, device_desc.max_packet_size0);
			(void)controller->ops->remove_device(controller->priv, dev);
			kmem_free(dev);
			return (-1);
		}
		dev->max_packet_size0 = 1U << device_desc.max_packet_size0;
	} else {
		dev->max_packet_size0 = device_desc.max_packet_size0;
	}
	if (dev->max_packet_size0 != 8 && dev->max_packet_size0 != 16 &&
	    dev->max_packet_size0 != 32 && dev->max_packet_size0 != 64) {
		usb_log_printf("usb: port %u: bad ep0 max packet size %u\n",
		    port, dev->max_packet_size0);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	if (controller->ops->update_ep0(controller->priv, dev) != 0) {
		usb_log_printf("usb: port %u: update ep0 failed\n", port);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	usb_log_printf("usb: port %u: ep0 max=%u\n", port,
	    dev->max_packet_size0);
	memset(&setup, 0, sizeof(setup));
	setup.bmRequestType = USB_DIR_IN | USB_REQ_TYPE_STANDARD |
	    USB_RECIP_DEVICE;
	setup.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup.wValue = USB_DESC_DEVICE << 8;
	setup.wLength = sizeof(device_desc);
	if (usb_control_transfer(dev, &setup, &device_desc,
	    sizeof(device_desc), 1000) != 0) {
		usb_log_printf("usb: port %u: get full device descriptor "
		    "failed\n", port);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	if (device_desc.length != sizeof(usb_device_desc_t) ||
	    device_desc.type != USB_DESC_DEVICE) {
		usb_log_printf("usb: port %u: bad device descriptor "
		    "len=%u type=%u\n", port, device_desc.length,
		    device_desc.type);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	usb_log_printf("usb: port %u: dev=%02x %02x %04x %02x %02x %02x "
	    "%02x %04x:%04x %04x\n", port, device_desc.length,
	    device_desc.type, device_desc.bcd_usb, device_desc.device_class,
	    device_desc.device_subclass, device_desc.device_protocol,
	    device_desc.max_packet_size0, device_desc.vendor_id,
	    device_desc.product_id, device_desc.bcd_device);
	dev->vendor_id = device_desc.vendor_id;
	dev->product_id = device_desc.product_id;
	dev->device_class = device_desc.device_class;
	dev->device_subclass = device_desc.device_subclass;
	dev->device_protocol = device_desc.device_protocol;
	memset(&config, 0, sizeof(config));
	setup.wValue = USB_DESC_CONFIGURATION << 8;
	setup.wLength = sizeof(config);
	if (usb_control_transfer(dev, &setup, &config, sizeof(config),
	    1000) != 0 || config.length != sizeof(usb_config_desc_t) ||
	    config.type != USB_DESC_CONFIGURATION ||
	    config.total_length < sizeof(config) ||
	    config.total_length > USB_MAX_CONFIG_SIZE) {
		usb_log_printf("usb: port %u: get config header failed "
		    "(total=%u)\n", port, config.total_length);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	usb_log_printf("usb: port %u: cfg=%02x %02x %04x %02x %02x %02x "
	    "%02x\n", port, config.length, config.type, config.total_length,
	    config.interface_count, config.configuration_value,
	    config.configuration, config.attributes);
	total_length = config.total_length;
	config_data = kmem_alloc(total_length);
	if (config_data == NULL) {
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	setup.wLength = total_length;
	if (usb_control_transfer(dev, &setup, config_data, total_length,
	    1000) != 0 || usb_parse_configuration(dev, config_data,
	    total_length) != 0) {
		usb_log_printf("usb: port %u: get/parse configuration "
		    "failed\n", port);
		kmem_free(config_data);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	usb_log_printf("usb: port %u: dev %04x:%04x speed=%u\n", port,
	    dev->vendor_id, dev->product_id, dev->speed);
	for (index = 0; index < dev->interface_count; index++) {
		usb_log_printf("usb:   iface %u alt %u class %02x/%02x/%02x "
		    "eps %u\n",
		    dev->interfaces[index].number,
		    dev->interfaces[index].alternate,
		    dev->interfaces[index].class_code,
		    dev->interfaces[index].subclass,
		    dev->interfaces[index].protocol,
		    dev->interfaces[index].endpoint_count);
	}
	dev->configuration = config.configuration_value;
	setup.bmRequestType = USB_DIR_OUT | USB_REQ_TYPE_STANDARD |
	    USB_RECIP_DEVICE;
	setup.bRequest = USB_REQ_SET_CONFIGURATION;
	setup.wValue = dev->configuration;
	setup.wLength = 0;
	if (usb_control_transfer(dev, &setup, NULL, 0, 1000) != 0) {
		usb_log_printf("usb: port %u: set configuration failed\n",
		    port);
		kmem_free(config_data);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	usb_log_printf("usb: port %u: set config %u ok\n", port,
	    dev->configuration);
	if (controller->ops->configure_device(controller->priv, dev) != 0) {
		usb_log_printf("usb: port %u: configure endpoints failed\n",
		    port);
		kmem_free(config_data);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	usb_log_printf("usb: port %u: configure ok\n", port);
	for (index = 0; index < dev->interface_count; index++) {
		child = device_add_child(controller->bus_device, "usbif", -1);
		if (child != NULL) {
			device_set_ivars(child, &dev->interfaces[index]);
		}
	}
	controller->ports[port - 1] = dev;
	kmem_free(config_data);
	newbus_reprobe();
	return (0);
}

int
usb_controller_init(usb_controller_t *controller,
    const usb_controller_ops_t *ops, void *priv, device_t bus_device,
    u8 port_count)
{
	if (controller == NULL || ops == NULL || ops->port_connected == NULL ||
	    ops->port_reset == NULL || ops->address_device == NULL ||
	    ops->update_ep0 == NULL || ops->configure_device == NULL ||
	    ops->remove_device == NULL ||
	    ops->control == NULL ||
	    ops->bulk == NULL || ops->bulk_submit == NULL ||
	    ops->interrupt == NULL || bus_device == NULL ||
	    port_count == 0 || port_count > USB_MAX_PORTS) {
		return (-1);
	}
	memset(controller, 0, sizeof(*controller));
	controller->ops = ops;
	controller->priv = priv;
	controller->bus_device = bus_device;
	controller->port_count = port_count;
	controller->port_done = 0;
	return (0);
}

void
usb_controller_scan(usb_controller_t *controller)
{
	usb_device_t	*old;
	device_t	child, next;
	u32		mask;
	u8		port, speed, rescan;

	if (controller == NULL) {
		return;
	}
	for (port = 1; port <= controller->port_count; port++) {
		speed = 0;
		mask = 1U << (port - 1);
		rescan = (u8)((controller->rescan_mask & mask) != 0);
		controller->rescan_mask &= ~mask;
		if (controller->retry_ticks[port - 1] != 0 && !rescan) {
			controller->retry_ticks[port - 1]--;
			if (controller->retry_ticks[port - 1] == 0) {
				controller->port_done &= ~mask;
			}
			continue;
		}
		if (controller->ports[port - 1] != NULL) {
			if (!rescan &&
			    controller->ops->port_connected(controller->priv,
			    port, &speed) != 0) {
				continue;
			}
			old = controller->ports[port - 1];
			controller->ports[port - 1] = NULL;
			usb_log_printf("usb: port %u: device "
			    "re-enumerating\n", port);
			for (child = device_get_child(controller->bus_device);
			    child != NULL; child = next) {
				next = device_get_next(child);
				if (usb_ivars_belongs(child, old)) {
					(void)device_delete_child(
					    controller->bus_device,
					    child);
				}
			}
			(void)controller->ops->remove_device(
			    controller->priv,
			    old);
			kmem_free(old);
			controller->port_done &= ~mask;
			speed = 0;
		}
		if ((controller->port_done & mask) != 0 ||
		    controller->ops->port_connected(controller->priv, port,
		    &speed) == 0) {
			continue;
		}
		if (controller->ops->port_reset(controller->priv, port) == 0) {
			speed = 0;
			if (controller->ops->port_connected(controller->priv,
			    port, &speed) != 0 &&
			    usb_enumerate_port(controller, port, speed) == 0) {
				controller->port_done |= mask;
				controller->fail_count[port - 1] = 0;
			} else {
				controller->port_done |= mask;
				controller->fail_count[port - 1]++;
				if (controller->fail_count[port - 1] <
				    USB_PORT_FAIL_LIMIT) {
					controller->retry_ticks[port - 1] =
					    USB_PORT_RETRY_PASSES;
				}
			}
		} else {
			controller->port_done |= mask;
		}
		usb_log_flush();
	}
}

void
usb_controller_fini(usb_controller_t *controller)
{
	usb_device_t	*dev;
	device_t	child, next;
	u8		port;

	if (controller == NULL) {
		return;
	}
	for (child = device_get_child(controller->bus_device); child != NULL;
	    child = next) {
		next = device_get_next(child);
		(void)device_delete_child(controller->bus_device, child);
	}
	for (port = 0; port < controller->port_count; port++) {
		dev = controller->ports[port];
		if (dev == NULL) {
			continue;
		}
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		controller->ports[port] = NULL;
	}
}

void
usb_controller_port_retry(usb_controller_t *controller, u8 port)
{
	if (controller == NULL || port == 0 || port > controller->port_count) {
		return;
	}
	controller->port_done &= ~(1U << (port - 1));
	controller->rescan_mask |= 1U << (port - 1);
	controller->fail_count[port - 1] = 0;
	controller->retry_ticks[port - 1] = 0;
}

int
usb_control_transfer(usb_device_t *dev, const usb_setup_t *setup,
    void *data, u16 length, u32 timeout)
{
	if (dev == NULL || dev->controller == NULL || setup == NULL ||
	    (length != 0 && data == NULL)) {
		return (-1);
	}
	return (dev->controller->ops->control(dev->controller->priv, dev,
	    setup, data, length, timeout));
}

int
usb_set_interface(usb_device_t *dev, u8 interface_number, u8 alternate)
{
	usb_setup_t	setup;
	u8		index;

	if (dev == NULL || dev->controller == NULL) {
		return (-1);
	}
	for (index = 0; index < dev->interface_count; index++) {
		if (dev->interfaces[index].number == interface_number &&
		    dev->interfaces[index].alternate == alternate) {
			break;
		}
	}
	if (index == dev->interface_count) {
		return (-1);
	}
	memset(&setup, 0, sizeof(setup));
	setup.bmRequestType = USB_DIR_OUT | USB_REQ_TYPE_STANDARD |
	    USB_RECIP_INTERFACE;
	setup.bRequest = USB_REQ_SET_INTERFACE;
	setup.wValue = alternate;
	setup.wIndex = interface_number;
	if (usb_control_transfer(dev, &setup, NULL, 0, 1000) != 0) {
		return (-1);
	}
	if (dev->controller->ops->configure_interface != NULL) {
		if (dev->controller->ops->configure_interface(
		    dev->controller->priv, dev, interface_number,
		    alternate) != 0) {
			return (-1);
		}
	}
	usb_log_printf("usb: set interface %u alt %u ok\n", interface_number,
	    alternate);
	return (0);
}

int
usb_bulk_transfer(usb_device_t *dev, usb_endpoint_t *ep, void *data,
    u32 *length, u32 timeout)
{
	if (dev == NULL || dev->controller == NULL || ep == NULL || data == NULL ||
	    length == NULL || (ep->attributes & 3) != USB_ENDPOINT_XFER_BULK) {
		return (-1);
	}
	return (dev->controller->ops->bulk(dev->controller->priv, dev, ep,
	    data, length, timeout));
}

int
usb_interrupt_submit(usb_device_t *dev, usb_endpoint_t *ep, void *data,
    u32 length, usb_complete_t complete, void *arg)
{
	if (dev == NULL || dev->controller == NULL || ep == NULL || data == NULL ||
	    length == 0 || complete == NULL ||
	    (ep->attributes & 3) != USB_ENDPOINT_XFER_INT) {
		return (-1);
	}
	return (dev->controller->ops->interrupt(dev->controller->priv, dev, ep,
	    data, length, complete, arg));
}

int
usb_bulk_submit(usb_device_t *dev, usb_endpoint_t *ep, void *data,
    u32 length, usb_complete_t complete, void *arg)
{
	if (dev == NULL || dev->controller == NULL || ep == NULL || data == NULL ||
	    length == 0 || complete == NULL ||
	    (ep->attributes & 3) != USB_ENDPOINT_XFER_BULK) {
		return (-1);
	}
	return (dev->controller->ops->bulk_submit(dev->controller->priv, dev,
	    ep, data, length, complete, arg));
}

usb_interface_t *
usb_interface_get(device_t dev)
{
	if (dev == NULL || strcmp(device_get_name(dev), "usbif") != 0) {
		return (NULL);
	}
	return ((usb_interface_t *)device_get_ivars(dev));
}

static void
usb_logflush_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "usb_logflush", 0) == NULL) {
		device_add_child(parent, "usb_logflush", 0);
	}
}

static int
usb_logflush_attach(device_t dev)
{
	(void)dev;
	usb_log_flush();
	return (0);
}

static devclass_t usb_logflush_devclass = {
	.name		= "usb_logflush",
	.maxunit	= 1,
};

static driver_t usb_logflush_driver = {
	.name		= "usb_logflush",
	.identify	= usb_logflush_identify,
	.probe		= NULL,
	.attach		= usb_logflush_attach,
};

PSEUDO_DRIVER_MODULE(usb_logflush, usb_logflush_driver, usb_logflush_devclass,
    NEWBUS_PASS_LATE, NEWBUS_ORDER_LAST);
