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

$define %type rndis_state_t as USB Remote NDIS network interface state
$define %func rndis_probe as function with args device_t
$define %func rndis_attach as function with args device_t
$define %func rndis_detach as function with args device_t

*/

/* !SPACE!

$space %internal rndis_control, rndis_query, rndis_set, rndis_initialize
$space %internal rndis_rx_complete, rndis_transmit, rndis_poll, rndis_link
$space %internal rndis_probe, rndis_attach, rndis_detach
$space %internal rndis_buffers_free

*/

#include <kernel/drivers/USB/usb.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/mm/kmem.h>
#include <kernel/net/net.h>
#include <kernel/net/netdev/netdev.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	RNDIS_REQ_SEND_COMMAND		0
#define	RNDIS_REQ_GET_RESPONSE		1
#define	RNDIS_MSG_PACKET		0x00000001
#define	RNDIS_MSG_INITIALIZE		0x00000002
#define	RNDIS_MSG_QUERY		0x00000004
#define	RNDIS_MSG_SET			0x00000005
#define	RNDIS_MSG_HALT			0x00000003
#define	RNDIS_MSG_INITIALIZE_CMPLT	0x80000002
#define	RNDIS_MSG_QUERY_CMPLT		0x80000004
#define	RNDIS_MSG_SET_CMPLT		0x80000005
#define	RNDIS_MSG_INDICATE_STATUS	0x00000007
#define	RNDIS_MSG_KEEPALIVE		0x00000008
#define	RNDIS_STATUS_SUCCESS		0x00000000
#define	RNDIS_OID_CURRENT_ADDRESS	0x01010102
#define	RNDIS_OID_PACKET_FILTER	0x0001010E
#define	RNDIS_PACKET_FILTER		0x0000000F
#define	RNDIS_RX_BUFFER_SIZE		2048
#define	RNDIS_TX_BUFFER_SIZE		2048

typedef struct {
	u32	type;
	u32	length;
	u32	request_id;
	u32	major;
	u32	minor;
	u32	max_transfer;
} __attribute__((packed)) rndis_initialize_t;

typedef struct {
	u32	type;
	u32	length;
	u32	request_id;
	u32	status;
	u32	major;
	u32	minor;
	u32	flags;
	u32	medium;
	u32	max_packets;
	u32	max_transfer;
	u32	alignment;
	u32	af_offset;
	u32	af_size;
} __attribute__((packed)) rndis_init_complete_t;

typedef struct {
	u32	type;
	u32	length;
	u32	request_id;
	u32	oid;
	u32	info_length;
	u32	info_offset;
	u32	reserved;
} __attribute__((packed)) rndis_query_t;

typedef struct {
	u32	type;
	u32	length;
	u32	request_id;
	u32	status;
	u32	info_length;
	u32	info_offset;
} __attribute__((packed)) rndis_query_complete_t;

typedef struct {
	u32	type;
	u32	length;
	u32	request_id;
	u32	oid;
	u32	info_length;
	u32	info_offset;
	u32	reserved;
} __attribute__((packed)) rndis_set_t;

typedef struct {
	u32	type;
	u32	length;
	u32	request_id;
	u32	status;
} __attribute__((packed)) rndis_set_complete_t;


typedef struct {
	u32	type;
	u32	length;
	u32	data_offset;
	u32	data_length;
	u32	oob_offset;
	u32	oob_length;
	u32	oob_count;
	u32	ppi_offset;
	u32	ppi_length;
	u32	vc_handle;
	u32	reserved;
} __attribute__((packed)) rndis_packet_t;

typedef struct {
	device_t		dev;
	usb_interface_t		*control;
	usb_interface_t		*data;
	usb_endpoint_t		*bulk_in;
	usb_endpoint_t		*bulk_out;
	u8			*rx_buffer;
	u8			*tx_buffer;
	u32			request_id;
	u8			running;
	u8			rx_active;
	netdev_t		ndev;
	net_iface_t		iface;
} rndis_state_t;

static int	rndis_probe(device_t dev);
static int	rndis_attach(device_t dev);
static int	rndis_detach(device_t dev);
static usb_interface_t	*rndis_find_data_interface(usb_device_t *usb);
static int	rndis_control(rndis_state_t *state, void *command, u16 command_len,
		    void *response, u16 response_len);
static int	rndis_query(rndis_state_t *state, u32 oid, void *out, u32 out_len);
static int	rndis_set(rndis_state_t *state, u32 oid, const void *data,
		    u32 data_len);
static int	rndis_initialize(rndis_state_t *state);
static void	rndis_rx_complete(usb_device_t *usb, void *buffer, u32 length,
		    int status, void *arg);
static int	rndis_transmit(netdev_t *ndev, const u8 *frame, u16 length);
static int	rndis_poll(netdev_t *ndev);
static int	rndis_link(netdev_t *ndev);

static netdev_ops_t rndis_netdev_ops = {
	.name		= "rndis",
	.transmit	= rndis_transmit,
	.poll		= rndis_poll,
	.is_link_up	= rndis_link,
};

static int
rndis_control(rndis_state_t *state, void *command, u16 command_len,
    void *response, u16 response_len)
{
	usb_setup_t	setup;

	memset(&setup, 0, sizeof(setup));
	setup.bmRequestType = USB_DIR_OUT | USB_REQ_TYPE_CLASS |
	    USB_RECIP_INTERFACE;
	setup.bRequest = RNDIS_REQ_SEND_COMMAND;
	setup.wIndex = state->control->number;
	setup.wLength = command_len;
	if (usb_control_transfer(state->control->device, &setup, command,
	    command_len, 1000) != 0) {
		usb_log_printf("rndis: control send bRequest=%u failed\n",
		    setup.bRequest);
		usb_log_flush();
		return (-1);
	}
	setup.bmRequestType = USB_DIR_IN | USB_REQ_TYPE_CLASS |
	    USB_RECIP_INTERFACE;
	setup.bRequest = RNDIS_REQ_GET_RESPONSE;
	setup.wLength = response_len;
	if (usb_control_transfer(state->control->device, &setup, response,
	    response_len, 1000) != 0) {
		usb_log_printf("rndis: control get bRequest=%u failed\n",
		    setup.bRequest);
		usb_log_flush();
		return (-1);
	}
	return (0);
}

static int
rndis_initialize(rndis_state_t *state)
{
	rndis_initialize_t	command;
	rndis_init_complete_t	response;

	memset(&command, 0, sizeof(command));
	memset(&response, 0, sizeof(response));
	command.type = RNDIS_MSG_INITIALIZE;
	command.length = sizeof(command);
	command.request_id = ++state->request_id;
	command.major = 1;
	command.minor = 0;
	command.max_transfer = RNDIS_RX_BUFFER_SIZE;
	if (rndis_control(state, &command, command.length, &response,
	    sizeof(response)) != 0 || response.type != RNDIS_MSG_INITIALIZE_CMPLT ||
	    response.length < sizeof(response) ||
	    response.request_id != command.request_id ||
	    response.status != RNDIS_STATUS_SUCCESS || response.medium != 0) {
		usb_log_printf("rndis: initialize failed type=%x len=%u "
		    "rid=%u status=%x medium=%u\n", response.type,
		    response.length, response.request_id, response.status,
		    response.medium);
		usb_log_flush();
		return (-1);
	}
	return (0);
}

static int
rndis_query(rndis_state_t *state, u32 oid, void *out, u32 out_len)
{
	rndis_query_t		command;
	rndis_query_complete_t	response;
	u8			buffer[256];
	u32			offset;

	if (out == NULL || out_len == 0 || out_len > sizeof(buffer)) {
		return (-1);
	}
	memset(&command, 0, sizeof(command));
	memset(buffer, 0, sizeof(buffer));
	command.type = RNDIS_MSG_QUERY;
	command.length = sizeof(command);
	command.request_id = ++state->request_id;
	command.oid = oid;
	if (rndis_control(state, &command, sizeof(command), buffer,
	    sizeof(buffer)) != 0) {
		usb_log_printf("rndis: query oid=%x transport failed\n", oid);
		usb_log_flush();
		return (-1);
	}
	memcpy(&response, buffer, sizeof(response));
	if (response.type != RNDIS_MSG_QUERY_CMPLT ||
	    response.length < sizeof(response) ||
	    response.request_id != command.request_id ||
	    response.status != RNDIS_STATUS_SUCCESS ||
	    response.info_length < out_len) {
		usb_log_printf("rndis: query oid=%x failed type=%x len=%u "
		    "rid=%u status=%x info_len=%u\n", oid, response.type,
		    response.length, response.request_id, response.status,
		    response.info_length);
		usb_log_flush();
		return (-1);
	}
	offset = 8 + response.info_offset;
	if (offset > response.length || out_len > response.length - offset ||
	    response.length > sizeof(buffer)) {
		return (-1);
	}
	memcpy(out, buffer + offset, out_len);
	return (0);
}

static int
rndis_set(rndis_state_t *state, u32 oid, const void *data, u32 data_len)
{
	rndis_set_t		command;
	rndis_set_complete_t	response;
	u8			buffer[sizeof(command) + sizeof(u32)];

	if (data == NULL || data_len > sizeof(buffer) - sizeof(command)) {
		return (-1);
	}
	memset(buffer, 0, sizeof(buffer));
	memset(&command, 0, sizeof(command));
	command.type = RNDIS_MSG_SET;
	command.length = sizeof(command) + data_len;
	command.request_id = ++state->request_id;
	command.oid = oid;
	command.info_length = data_len;
	command.info_offset = sizeof(command) - 8;
	memcpy(buffer, &command, sizeof(command));
	memcpy(buffer + sizeof(command), data, data_len);
	memset(&response, 0, sizeof(response));
	if (rndis_control(state, buffer, command.length, &response,
	    sizeof(response)) != 0 || response.type != RNDIS_MSG_SET_CMPLT ||
	    response.length < sizeof(response) ||
	    response.request_id != command.request_id ||
	    response.status != RNDIS_STATUS_SUCCESS) {
		usb_log_printf("rndis: set oid=%x failed type=%x len=%u "
		    "rid=%u status=%x\n", oid, response.type,
		    response.length, response.request_id, response.status);
		usb_log_flush();
		return (-1);
	}
	return (0);
}

static void
rndis_buffers_free(rndis_state_t *state)
{
	if (state->rx_buffer != NULL) {
		kmem_free(state->rx_buffer);
		state->rx_buffer = NULL;
	}
	if (state->tx_buffer != NULL) {
		kmem_free(state->tx_buffer);
		state->tx_buffer = NULL;
	}
}

static void
rndis_rx_complete(usb_device_t *usb, void *buffer, u32 length, int status,
    void *arg)
{
	rndis_state_t	*state;
	rndis_packet_t	*packet;
	u32		offset, packet_length, data_offset;

	(void)usb;
	state = arg;
	if (state == NULL || !state->running) {
		return;
	}
	if (status == 0) {
		for (offset = 0; offset + sizeof(*packet) <= length;) {
			packet = (rndis_packet_t *)((u8 *)buffer + offset);
			packet_length = packet->length;
			if (packet->type != RNDIS_MSG_PACKET ||
			    packet_length < sizeof(*packet) ||
			    packet_length > length - offset) {
				break;
			}
			data_offset = 8 + packet->data_offset;
			if (data_offset <= packet_length && packet->data_length <=
			    packet_length - data_offset &&
			    packet->data_length >= 14 &&
			    packet->data_length <= state->ndev.mtu + 14 &&
			    state->ndev.rx_handler != NULL) {
				state->ndev.rx_handler(&state->ndev,
				    (u8 *)packet + data_offset, packet->data_length);
				state->ndev.rx_delivered++;
			} else {
				state->ndev.rx_dropped++;
			}
			offset += packet_length;
		}
	}
	state->rx_active = 0;
	if (usb_bulk_submit(state->control->device, state->bulk_in,
	    state->rx_buffer, RNDIS_RX_BUFFER_SIZE, rndis_rx_complete,
	    state) == 0) {
		state->rx_active = 1;
	}
}

static int
rndis_transmit(netdev_t *ndev, const u8 *frame, u16 length)
{
	rndis_state_t	*state;
	rndis_packet_t	*packet;
	u32		transfer_length;

	if (ndev == NULL || frame == NULL || length < 60 ||
	    length > ndev->mtu + 14) {
		return (-1);
	}
	state = ndev->priv;
	if (state == NULL || !state->running) {
		return (-1);
	}
	packet = (rndis_packet_t *)state->tx_buffer;
	memset(packet, 0, sizeof(*packet));
	packet->type = RNDIS_MSG_PACKET;
	packet->length = sizeof(*packet) + length;
	packet->data_offset = sizeof(*packet) - 8;
	packet->data_length = length;
	memcpy(state->tx_buffer + sizeof(*packet), frame, length);
	transfer_length = packet->length;
	if (state->bulk_out->max_packet_size != 0 &&
	    transfer_length % state->bulk_out->max_packet_size == 0) {
		state->tx_buffer[transfer_length++] = 0;
	}
	if (usb_bulk_transfer(state->control->device, state->bulk_out,
	    state->tx_buffer, &transfer_length, 1000) != 0) {
		ndev->tx_dropped++;
		return (-1);
	}
	ndev->tx_submitted++;
	ndev->tx_completed++;
	return (0);
}

static int
rndis_poll(netdev_t *ndev)
{
	rndis_state_t	*state;

	if (ndev == NULL || (state = ndev->priv) == NULL || !state->running) {
		return (-1);
	}
	if (!state->rx_active && usb_bulk_submit(state->control->device,
	    state->bulk_in, state->rx_buffer, RNDIS_RX_BUFFER_SIZE,
	    rndis_rx_complete, state) == 0) {
		state->rx_active = 1;
	}
	return (0);
}

static int
rndis_link(netdev_t *ndev)
{
	rndis_state_t	*state;

	state = ndev == NULL ? NULL : ndev->priv;
	return (state != NULL && state->running);
}

static usb_interface_t *
rndis_find_data_interface(usb_device_t *usb)
{
	usb_interface_t	*interface;
	u8		index;

	if (usb == NULL) {
		return (NULL);
	}
	for (index = 0; index < usb->interface_count; index++) {
		interface = &usb->interfaces[index];
		if (interface->class_code == USB_CLASS_DATA &&
		    interface->endpoint_count != 0) {
			return (interface);
		}
	}
	return (NULL);
}

static int
rndis_is_standard_interface(usb_interface_t *interface)
{
	if (interface == NULL) {
		return (0);
	}
	return ((interface->class_code == 0xE0 &&
	    interface->subclass == 1 && interface->protocol == 3) ||
	    (interface->class_code == USB_CLASS_COMM &&
	    interface->subclass == 2 && interface->protocol == 0xFF));
}

static int
rndis_has_standard_interface(usb_device_t *usb)
{
	usb_interface_t	*interface;
	u8		index;

	if (usb == NULL) {
		return (0);
	}
	for (index = 0; index < usb->interface_count; index++) {
		interface = &usb->interfaces[index];
		if (rndis_is_standard_interface(interface)) {
			return (1);
		}
	}
	return (0);
}

static int
rndis_probe(device_t dev)
{
	usb_interface_t	*interface;
	usb_device_t	*usb;
	int		standard;

	interface = usb_interface_get(dev);
	if (interface == NULL) {
		return (-1);
	}
	usb = interface->device;
	standard = rndis_is_standard_interface(interface);
	if (!standard && !(interface->class_code == 0xFF &&
	    !rndis_has_standard_interface(usb) &&
	    rndis_find_data_interface(usb) != NULL)) {
		usb_log_printf("rndis: probe %s: no match class="
		    "%02x/%02x/%02x dev=%04x:%04x\n",
		    device_get_nameunit(dev),
		    interface->class_code, interface->subclass,
		    interface->protocol,
		    usb != NULL ? usb->vendor_id : 0,
		    usb != NULL ? usb->product_id : 0);
		usb_log_flush();
		return (-1);
	}
	usb_log_printf("rndis: probe %s: match class=%02x/%02x/%02x "
	    "dev=%04x:%04x\n",
	    device_get_nameunit(dev), interface->class_code,
	    interface->subclass, interface->protocol,
	    usb != NULL ? usb->vendor_id : 0,
	    usb != NULL ? usb->product_id : 0);
	usb_log_flush();
	return (100);
}

static int
rndis_attach(device_t dev)
{
	rndis_state_t	*state;
	usb_interface_t	*interface;
	usb_endpoint_t	*endpoint;
	u32		filter;
	u8		index;

	interface = usb_interface_get(dev);
	if (interface == NULL) {
		return (-1);
	}
	if (!net_is_initialized()) {
		usb_log_printf("rndis: attach %s: net not initialized\n",
		    device_get_nameunit(dev));
		usb_log_flush();
		return (-1);
	}
	state = kmem_calloc(1, sizeof(*state));
	if (state == NULL) {
		return (-1);
	}
	state->dev = dev;
	state->control = interface;
	state->data = rndis_find_data_interface(interface->device);
	if (state->data == NULL) {
		usb_log_printf("rndis: attach %s: no data interface found\n",
		    device_get_nameunit(dev));
		usb_log_flush();
		kmem_free(state);
		return (-1);
	}
	if (state->data->alternate != 0) {
		if (usb_set_interface(interface->device, state->data->number,
		    state->data->alternate) != 0) {
			usb_log_printf("rndis: attach %s: set interface %u "
			    "alt %u failed\n", device_get_nameunit(dev),
			    state->data->number, state->data->alternate);
			usb_log_flush();
			kmem_free(state);
			return (-1);
		}
	}
	for (index = 0; index < state->data->endpoint_count; index++) {
		endpoint = &state->data->endpoints[index];
		if ((endpoint->attributes & 3) != USB_ENDPOINT_XFER_BULK) {
			continue;
		}
		if (endpoint->address & USB_DIR_IN) {
			state->bulk_in = endpoint;
		} else {
			state->bulk_out = endpoint;
		}
	}
	state->rx_buffer = kmem_alloc(RNDIS_RX_BUFFER_SIZE);
	state->tx_buffer = kmem_alloc(RNDIS_TX_BUFFER_SIZE);
	if (state->bulk_in == NULL || state->bulk_out == NULL ||
	    state->rx_buffer == NULL || state->tx_buffer == NULL ||
	    rndis_initialize(state) != 0 || rndis_query(state,
	    RNDIS_OID_CURRENT_ADDRESS, state->ndev.mac,
	    sizeof(state->ndev.mac)) != 0) {
		usb_log_printf("rndis: attach %s: init/query failed "
		    "(in=%p out=%p)\n", device_get_nameunit(dev),
		    (void *)state->bulk_in, (void *)state->bulk_out);
		usb_log_flush();
		rndis_buffers_free(state);
		kmem_free(state);
		return (-1);
	}
	filter = RNDIS_PACKET_FILTER;
	if (rndis_set(state, RNDIS_OID_PACKET_FILTER, &filter,
	    sizeof(filter)) != 0) {
		usb_log_printf("rndis: attach %s: set packet filter failed\n",
		    device_get_nameunit(dev));
		usb_log_flush();
		rndis_buffers_free(state);
		kmem_free(state);
		return (-1);
	}
	snprintf(state->ndev.name, sizeof(state->ndev.name), "rndis%d",
	    device_get_unit(dev));
	state->ndev.mtu = NETDEV_MTU_DEFAULT;
	state->ndev.flags = NETDEV_F_UP | NETDEV_F_RUNNING | NETDEV_F_BROADCAST;
	state->ndev.index = -1;
	state->ndev.priv = state;
	state->ndev.ops = &rndis_netdev_ops;
	snprintf(state->iface.name, sizeof(state->iface.name), "rndis%d",
	    device_get_unit(dev));
	state->iface.flags = NET_IFF_UP | NET_IFF_RUNNING;
	state->iface.index = -1;
	if (netdev_register(&state->ndev) != 0 ||
	    net_iface_register(&state->iface, &state->ndev) != 0) {
		usb_log_printf("rndis: attach %s: netdev register failed\n",
		    device_get_nameunit(dev));
		usb_log_flush();
		netdev_unregister(&state->ndev);
		rndis_buffers_free(state);
		kmem_free(state);
		return (-1);
	}
	state->running = 1;
	device_set_softc(dev, state);
	usb_log_printf("rndis: attach %s: up mac=%02x:%02x:%02x:%02x:"
	    "%02x:%02x\n", device_get_nameunit(dev), state->ndev.mac[0],
	    state->ndev.mac[1], state->ndev.mac[2], state->ndev.mac[3],
	    state->ndev.mac[4], state->ndev.mac[5]);
	usb_log_flush();
	if (rndis_poll(&state->ndev) != 0) {
		usb_log_printf("rndis: attach %s: initial rx submit "
		    "failed, will retry\n", device_get_nameunit(dev));
		usb_log_flush();
	}
	return (0);
}

static int
rndis_detach(device_t dev)
{
	rndis_state_t	*state;

	state = device_get_softc(dev);
	if (state == NULL) {
		return (0);
	}
	usb_log_printf("rndis: detach %s (%s)\n", device_get_nameunit(dev),
	    state->ndev.name);
	usb_log_flush();
	state->running = 0;
	net_iface_unregister(&state->iface);
	netdev_unregister(&state->ndev);
	rndis_buffers_free(state);
	device_set_softc(dev, NULL);
	kmem_free(state);
	return (0);
}

static devclass_t rndis_devclass = {
	.name		= "rndis",
	.maxunit	= USB_MAX_DEVICES,
};

static driver_t rndis_driver = {
	.name		= "rndis",
	.identify	= NULL,
	.probe		= rndis_probe,
	.attach		= rndis_attach,
	.detach		= rndis_detach,
};

DRIVER_MODULE(rndis, usb, rndis_driver, rndis_devclass,
    NEWBUS_PASS_LATE, NEWBUS_ORDER_MIDDLE);
