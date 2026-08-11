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
$define %func usb_control_transfer as function with args usb_device_t *, setup, void *, u16, u32
$define %func usb_bulk_transfer as function with args usb_device_t *, endpoint, void *, u32 *, u32
$define %func usb_bulk_submit as function with args usb_device_t *, endpoint, void *, u32, callback, void *
$define %func usb_interrupt_submit as function with args usb_device_t *, endpoint, void *, u32, callback, void *
$define %func usb_interface_get as function with args device_t

*/

/* !SPACE!

$space %internal usb_parse_configuration, usb_enumerate_port
$space %export usb_controller_init, usb_controller_scan
$space %export usb_control_transfer, usb_bulk_transfer, usb_interrupt_submit
$space %export usb_bulk_submit
$space %export usb_interface_get

*/

#include <kernel/drivers/USB/usb.h>
#include <kernel/mm/kmem.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

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
	return (0);
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

DRIVER_MODULE(usb_bus, xhci, usb_bus_driver, usb_bus_devclass,
    NEWBUS_PASS_STORAGE, NEWBUS_ORDER_MIDDLE);

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
	setup.wLength = sizeof(device_desc);
	if (usb_control_transfer(dev, &setup, &device_desc,
	    sizeof(device_desc), 1000) != 0) {
		printk("usb: port %u: get device descriptor failed\n", port);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	if (dev->speed >= USB_SPEED_SUPER) {
		if (device_desc.max_packet_size0 > 11) {
			printk("usb: port %u: bad super-speed ep0 size %u\n",
			    port, device_desc.max_packet_size0);
			(void)controller->ops->remove_device(controller->priv, dev);
			kmem_free(dev);
			return (-1);
		}
		dev->max_packet_size0 = 1U << device_desc.max_packet_size0;
	} else {
		dev->max_packet_size0 = device_desc.max_packet_size0;
	}
	if (dev->max_packet_size0 == 0) {
		printk("usb: port %u: zero ep0 max packet size\n", port);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	dev->vendor_id = device_desc.vendor_id;
	dev->product_id = device_desc.product_id;
	dev->device_class = device_desc.device_class;
	dev->device_subclass = device_desc.device_subclass;
	dev->device_protocol = device_desc.device_protocol;
	if (controller->ops->update_ep0(controller->priv, dev) != 0) {
		printk("usb: port %u: update ep0 failed\n", port);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	memset(&config, 0, sizeof(config));
	setup.wValue = USB_DESC_CONFIGURATION << 8;
	setup.wLength = sizeof(config);
	if (usb_control_transfer(dev, &setup, &config, sizeof(config),
	    1000) != 0 || config.total_length < sizeof(config) ||
	    config.total_length > USB_MAX_CONFIG_SIZE) {
		printk("usb: port %u: get config header failed (total=%u)\n",
		    port, config.total_length);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
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
		printk("usb: port %u: get/parse configuration failed\n", port);
		kmem_free(config_data);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	dev->configuration = config.configuration_value;
	setup.bmRequestType = USB_DIR_OUT | USB_REQ_TYPE_STANDARD |
	    USB_RECIP_DEVICE;
	setup.bRequest = USB_REQ_SET_CONFIGURATION;
	setup.wValue = dev->configuration;
	setup.wLength = 0;
	if (usb_control_transfer(dev, &setup, NULL, 0, 1000) != 0) {
		printk("usb: port %u: set configuration failed\n", port);
		kmem_free(config_data);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
	if (controller->ops->configure_device(controller->priv, dev) != 0) {
		printk("usb: port %u: configure endpoints failed\n", port);
		kmem_free(config_data);
		(void)controller->ops->remove_device(controller->priv, dev);
		kmem_free(dev);
		return (-1);
	}
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
	u8	port, speed;

	if (controller == NULL) {
		return;
	}
	for (port = 1; port <= controller->port_count; port++) {
		speed = 0;
		if (controller->ports[port - 1] != NULL ||
		    (controller->port_done & (1U << (port - 1))) != 0 ||
		    controller->ops->port_connected(controller->priv, port,
		    &speed) == 0) {
			continue;
		}
		if (controller->ops->port_reset(controller->priv, port) == 0) {
			(void)usb_enumerate_port(controller, port, speed);
		}
		controller->port_done |= 1U << (port - 1);
	}
}

void
usb_controller_port_retry(usb_controller_t *controller, u8 port)
{
	if (controller == NULL || port == 0 || port > controller->port_count) {
		return;
	}
	controller->port_done &= ~(1U << (port - 1));
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
