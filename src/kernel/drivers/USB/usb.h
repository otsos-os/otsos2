/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type usb_setup_t as packed USB control setup packet
$define %type usb_endpoint_t as USB endpoint descriptor state
$define %type usb_interface_t as USB interface descriptor state
$define %type usb_device_t as enumerated USB device state
$define %type usb_controller_t as USB host-controller state
$define %type usb_controller_ops_t as host-controller transport callbacks

$define %func usb_controller_init as function with args usb_controller_t *, ops, void *, device_t, u8
$define %func usb_controller_scan as procedure with args usb_controller_t *
$define %func usb_controller_fini as procedure with args usb_controller_t *
$define %func usb_control_transfer as function with args usb_device_t *, setup, void *, u16, u32
$define %func usb_bulk_transfer as function with args usb_device_t *, endpoint, void *, u32 *, u32
$define %func usb_bulk_submit as function with args usb_device_t *, endpoint, void *, u32, callback, void *
$define %func usb_interrupt_submit as function with args usb_device_t *, endpoint, void *, u32, callback, void *
$define %func usb_set_interface as function with args usb_device_t *, u8, u8
$define %func usb_interface_get as function with args device_t
$define %func usb_log_printf as function with args const char *, ...
$define %func usb_log_flush as procedure with args void

*/

/* !SPACE!

$space %export usb_controller_init, usb_controller_scan, usb_controller_fini
$space %export usb_control_transfer, usb_bulk_transfer, usb_interrupt_submit
$space %export usb_bulk_submit
$space %export usb_set_interface
$space %export usb_interface_get
$space %export usb_log_printf, usb_log_flush

*/

#ifndef KERNEL_DRIVERS_USB_USB_H
#define KERNEL_DRIVERS_USB_USB_H

#include <kernel/drivers/newbus/newbus.h>
#include <kernel/mm/dma/dma.h>
#include <mlibc/mlibc.h>

#define	USB_MAX_PORTS		32
#define	USB_MAX_DEVICES		64
#define	USB_MAX_INTERFACES	16
#define	USB_MAX_ENDPOINTS	16
#define	USB_MAX_CONFIG_SIZE	4096

#define	USB_DIR_OUT		0x00
#define	USB_DIR_IN		0x80
#define	USB_REQ_TYPE_STANDARD	0x00
#define	USB_REQ_TYPE_CLASS	0x20
#define	USB_RECIP_DEVICE	0x00
#define	USB_RECIP_INTERFACE	0x01

#define	USB_REQ_GET_DESCRIPTOR	0x06
#define	USB_REQ_SET_CONFIGURATION	0x09
#define	USB_REQ_SET_INTERFACE	0x0B
#define	USB_DESC_DEVICE		0x01
#define	USB_DESC_CONFIGURATION	0x02
#define	USB_DESC_INTERFACE	0x04
#define	USB_DESC_ENDPOINT	0x05

#define	USB_SPEED_LOW		1
#define	USB_SPEED_FULL		2
#define	USB_SPEED_HIGH		3
#define	USB_SPEED_SUPER		4

#define	USB_CLASS_HID		0x03
#define	USB_CLASS_HUB		0x09
#define	USB_CLASS_COMM		0x02
#define	USB_CLASS_DATA		0x0A

#define	USB_ENDPOINT_XFER_CONTROL	0x00
#define	USB_ENDPOINT_XFER_ISOC	0x01
#define	USB_ENDPOINT_XFER_BULK	0x02
#define	USB_ENDPOINT_XFER_INT	0x03

typedef struct {
	u8	bmRequestType;
	u8	bRequest;
	u16	wValue;
	u16	wIndex;
	u16	wLength;
} __attribute__((packed)) usb_setup_t;

typedef struct {
	u8	address;
	u8	attributes;
	u16	max_packet_size;
	u8	interval;
	u8	toggle;
} usb_endpoint_t;

struct usb_device;
typedef void (*usb_complete_t)(struct usb_device *, void *, u32, int,
    void *);

typedef struct {
	u8	number;
	u8	alternate;
	u8	class_code;
	u8	subclass;
	u8	protocol;
	u8	endpoint_count;
	usb_endpoint_t	endpoints[USB_MAX_ENDPOINTS];
	struct usb_device	*device;
} usb_interface_t;

typedef struct usb_controller_ops {
	int	(*port_connected)(void *priv, u8 port, u8 *speed);
	int	(*port_reset)(void *priv, u8 port);
	int	(*address_device)(void *priv, struct usb_device *dev);
	int	(*update_ep0)(void *priv, struct usb_device *dev);
	int	(*configure_device)(void *priv, struct usb_device *dev);
	int	(*configure_interface)(void *priv, struct usb_device *dev,
		    u8 interface_number, u8 alternate);
	int	(*remove_device)(void *priv, struct usb_device *dev);
	int	(*control)(void *priv, struct usb_device *dev,
		    const usb_setup_t *setup, void *data, u16 length, u32 timeout);
	int	(*bulk)(void *priv, struct usb_device *dev, usb_endpoint_t *ep,
		    void *data, u32 *length, u32 timeout);
	int	(*bulk_submit)(void *priv, struct usb_device *dev,
		    usb_endpoint_t *ep, void *data, u32 length,
		    usb_complete_t complete, void *arg);
	int	(*interrupt)(void *priv, struct usb_device *dev,
		    usb_endpoint_t *ep, void *data, u32 length,
		    usb_complete_t complete, void *arg);
} usb_controller_ops_t;

typedef struct usb_device {
	struct usb_controller	*controller;
	device_t		bus_device;
	u8			address;
	u8			slot_id;
	u8			speed;
	u8			port;
	u16			max_packet_size0;
	u8			configuration;
	u16			vendor_id;
	u16			product_id;
	u8			device_class;
	u8			device_subclass;
	u8			device_protocol;
	u8			interface_count;
	usb_interface_t	interfaces[USB_MAX_INTERFACES];
} usb_device_t;

typedef struct usb_controller {
	const usb_controller_ops_t	*ops;
	void				*priv;
	device_t				bus_device;
	usb_device_t			*ports[USB_MAX_PORTS];
	u32					port_done;
	u32					rescan_mask;
	u8				retry_ticks[USB_MAX_PORTS];
	u8				fail_count[USB_MAX_PORTS];
	u8				port_count;
} usb_controller_t;

int	usb_controller_init(usb_controller_t *controller,
    const usb_controller_ops_t *ops, void *priv, device_t bus_device,
    u8 port_count);
void	usb_controller_scan(usb_controller_t *controller);
void	usb_controller_fini(usb_controller_t *controller);
void	usb_controller_port_retry(usb_controller_t *controller, u8 port);
int	usb_control_transfer(usb_device_t *dev, const usb_setup_t *setup,
	    void *data, u16 length, u32 timeout);
int	usb_bulk_transfer(usb_device_t *dev, usb_endpoint_t *ep, void *data,
	    u32 *length, u32 timeout);
int	usb_bulk_submit(usb_device_t *dev, usb_endpoint_t *ep, void *data,
	    u32 length, usb_complete_t complete, void *arg);
int	usb_interrupt_submit(usb_device_t *dev, usb_endpoint_t *ep,
	    void *data, u32 length, usb_complete_t complete, void *arg);
int	usb_set_interface(usb_device_t *dev, u8 interface_number,
	    u8 alternate);
usb_interface_t	*usb_interface_get(device_t dev);
int		usb_log_printf(const char *fmt, ...);
void		usb_log_flush(void);

#endif
