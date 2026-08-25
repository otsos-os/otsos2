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

$define %type xhci_trb_t as packed xHCI transfer request block
$define %type xhci_ring_t as producer ring with cycle state
$define %type xhci_state_t as xHCI PCI controller state

$define %func xhci_wait32 as function with args volatile u32 *, u32, u32, u32
$define %func xhci_halt as function with args xhci_state_t *
$define %func xhci_reset as function with args xhci_state_t *
$define %func xhci_setup_rings as function with args xhci_state_t *
$define %func xhci_drain_enter as function with args xhci_state_t *, u64 *
$define %func xhci_drain_exit as procedure with args xhci_state_t *, u64
$define %func xhci_drain_ring as procedure with args xhci_state_t *
$define %func xhci_command_locked as function with args xhci_state_t *, u64, u32, u32, u8 *
$define %func xhci_control_locked as function with args xhci_state_t *, usb_device_t *, const usb_setup_t *, void *, u16, u32
$define %func xhci_normal_transfer_locked as function with args xhci_state_t *, usb_device_t *, usb_endpoint_t *, void *, u32 *, u32
$define %func xhci_interrupt_locked as function with args xhci_state_t *, usb_device_t *, usb_endpoint_t *, void *, u32, usb_complete_t, void *
$define %func xhci_pci_probe as function with args pci_device_t *, match
$define %func xhci_pci_remove as procedure with args pci_device_t *
$define %func xhci_pci_register as function with args void

*/

/* !SPACE!

$space %internal xhci_wait32, xhci_halt, xhci_reset, xhci_setup_rings
$space %internal xhci_drain_enter, xhci_drain_exit, xhci_drain_ring
$space %internal xhci_command_locked, xhci_control_locked
$space %internal xhci_normal_transfer_locked, xhci_interrupt_locked
$space %internal xhci_pci_probe, xhci_pci_remove
$space %export xhci_pci_register

*/

#include <kernel/drivers/USB/xhci.h>
#include <kernel/drivers/USB/usb.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/pci/pci.h>
#include <kernel/pci/utils/bar.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	XHCI_MAX_CONTROLLERS	8
#define	XHCI_RING_TRBS		256
/* RFLAGS.IF - saved and restored around the event ring guard. */
#define	XHCI_RFLAGS_IF		(1ULL << 9)
#define	XHCI_CAP_LENGTH		0x00
#define	XHCI_HCSPARAMS1		0x04
#define	XHCI_DBOFF			0x14
#define	XHCI_RTSOFF			0x18
#define	XHCI_USBCMD			0x00
#define	XHCI_USBSTS			0x04
#define	XHCI_CRCR			0x18
#define	XHCI_DCBAAP			0x30
#define	XHCI_CONFIG			0x38
#define	XHCI_PORTSC_BASE		0x400
#define	XHCI_PORTSC_STRIDE		0x10
#define	XHCI_IMAN			0x20
#define	XHCI_ERSTSZ			0x28
#define	XHCI_ERSTBA			0x30
#define	XHCI_ERDP			0x38
#define	XHCI_USBCMD_RUN		0x01
#define	XHCI_USBCMD_HCRST	0x02
#define	XHCI_USBCMD_INTE	0x04
#define	XHCI_USBSTS_HCH		0x01
#define	XHCI_USBSTS_EINT	0x08
#define	XHCI_USBSTS_CNR		0x800
#define	XHCI_IMAN_IP		0x01
#define	XHCI_IMAN_IE		0x02
#define	XHCI_TRB_CYCLE		1
#define	XHCI_TRB_TC			2
#define	XHCI_TRB_TYPE_SHIFT	10
#define	XHCI_TRB_TYPE_LINK	6
#define	XHCI_TRB_TYPE_NORMAL	1
#define	XHCI_TRB_TYPE_SETUP	2
#define	XHCI_TRB_TYPE_DATA	3
#define	XHCI_TRB_TYPE_STATUS	4
#define	XHCI_TRB_TYPE_ENABLE_SLOT	9
#define	XHCI_TRB_TYPE_DISABLE_SLOT	10
#define	XHCI_TRB_TYPE_ADDRESS_DEVICE	11
#define	XHCI_TRB_TYPE_CONFIGURE_ENDPOINT	12
#define	XHCI_TRB_TYPE_EVALUATE_CONTEXT	13
#define	XHCI_TRB_TYPE_TRANSFER_EVENT	32
#define	XHCI_TRB_TYPE_COMMAND_EVENT	33
#define	XHCI_TRB_TYPE_PORT_STATUS_EVENT	34
#define	XHCI_TRB_BSR			(1U << 9)
#define	XHCI_TRB_IOC			(1U << 5)
#define	XHCI_TRB_IDT			(1U << 6)
#define	XHCI_TRB_DIR_IN		(1U << 16)
#define	XHCI_TRT_OUT			(2U << 16)
#define	XHCI_TRT_IN			(3U << 16)
#define	XHCI_TRB_EP_SHIFT		16
#define	XHCI_TRB_SLOT_SHIFT		24
#define	XHCI_CC_SUCCESS		1
#define	XHCI_CC_SHORT_PACKET	13
#define	XHCI_PORTSC_CCS		1
#define	XHCI_PORTSC_PED		2
#define	XHCI_PORTSC_PR			(1U << 4)
#define	XHCI_PORTSC_CSC		(1U << 17)
#define	XHCI_PORTSC_SPEED_SHIFT	10
#define	XHCI_CTX_ENTRIES_SHIFT	27
#define	XHCI_SLOT_SPEED_SHIFT	20
#define	XHCI_SLOT_PORT_SHIFT		16
#define	XHCI_EP_TYPE_SHIFT		3
#define	XHCI_EP_MAX_PACKET_SHIFT	16
#define	XHCI_EP_DEQUEUE_CYCLE	1

typedef struct {
	u64	parameter;
	u32	status;
	u32	control;
} __attribute__((packed)) xhci_trb_t;

typedef struct {
	xhci_trb_t	*trbs;
	u64		phys;
	u16		index;
	u8		cycle;
} xhci_ring_t;

typedef struct {
	u64	address;
	u32	size;
	u32	reserved;
} __attribute__((packed)) xhci_erst_t;

typedef struct {
	xhci_ring_t	ring;
	usb_complete_t	complete;
	void		*complete_arg;
	void		*buffer;
	u32		length;
	u8		active;
} xhci_endpoint_t;

typedef struct {
	usb_device_t	*usb;
	void		*output_ctx;
	void		*input_ctx;
	u64		output_phys;
	u64		input_phys;
	u32		active_endpoints;
	xhci_endpoint_t	eps[32];
} xhci_device_t;

typedef struct {
	pci_device_t	*pci;
	device_t	nb_dev;
	resource_t	*mmio_res;
	volatile u8	*cap;
	volatile u8	*op;
	volatile u8	*runtime;
	volatile u8	*doorbell;
	u64		*dcbaa;
	u64		dcbaa_phys;
	u64		*scratch;
	u64		scratch_phys;
	void		**scratch_bufs;
	u32		scratch_count;
	xhci_trb_t	*events;
	xhci_erst_t	*erst;
	u64		events_phys;
	u64		erst_phys;
	u16		event_index;
	u8		event_cycle;
	u8		max_slots;
	u8		max_ports;
	u8		scanned;
	u8		drain_depth;
	u8		rescan;
	xhci_ring_t	command_ring;
	xhci_device_t	*slots[256];
	resource_t		*irq_res;
	void			*irq_cookie;
	usb_controller_t	usb;
	u8			irq_msi;
} xhci_state_t;

static xhci_state_t	*xhci_states[XHCI_MAX_CONTROLLERS];

static int	xhci_port_connected(void *priv, u8 port, u8 *speed);
static int	xhci_port_reset(void *priv, u8 port);
static int	xhci_address_device(void *priv, usb_device_t *dev);
static int	xhci_update_ep0(void *priv, usb_device_t *dev);
static int	xhci_configure_device(void *priv, usb_device_t *dev);
static int	xhci_configure_interface(void *priv, usb_device_t *dev,
		    u8 interface_number, u8 alternate);
static int	xhci_remove_device(void *priv, usb_device_t *dev);
static int	xhci_control(void *priv, usb_device_t *dev,
	    const usb_setup_t *setup, void *data, u16 length, u32 timeout);
static int	xhci_bulk(void *priv, usb_device_t *dev, usb_endpoint_t *ep,
	    void *data, u32 *length, u32 timeout);
static int	xhci_bulk_submit(void *priv, usb_device_t *dev,
	    usb_endpoint_t *ep, void *data, u32 length, usb_complete_t complete,
	    void *arg);
static int	xhci_interrupt(void *priv, usb_device_t *dev,
	    usb_endpoint_t *ep, void *data, u32 length, usb_complete_t complete,
	    void *arg);
static void	xhci_handle_transfer_event(xhci_state_t *state,
		    const xhci_trb_t *event);
static void	xhci_device_free(xhci_state_t *state, xhci_device_t *device);
static void	xhci_state_destroy(xhci_state_t *state);
static void	xhci_slot_release(xhci_state_t *state, u8 slot_id,
		    xhci_device_t *device);
static void	xhci_poll(void *arg);
static int	xhci_intr(void *arg);
static int	xhci_wait32(volatile u32 *reg, u32 mask, u32 value, u32 limit);
static u64	xhci_phys(void *ptr);
static int	xhci_drain_enter(xhci_state_t *state, u64 *flags);
static void	xhci_drain_exit(xhci_state_t *state, u64 flags);
static void	xhci_drain_ring(xhci_state_t *state);

static const usb_controller_ops_t xhci_usb_ops = {
	.port_connected = xhci_port_connected,
	.port_reset = xhci_port_reset,
	.address_device = xhci_address_device,
	.update_ep0 = xhci_update_ep0,
	.configure_device = xhci_configure_device,
	.configure_interface = xhci_configure_interface,
	.remove_device = xhci_remove_device,
	.control = xhci_control,
	.bulk = xhci_bulk,
	.bulk_submit = xhci_bulk_submit,
	.interrupt = xhci_interrupt,
};

static int
xhci_ring_create(xhci_ring_t *ring)
{
	ring->trbs = kmem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
	if (ring->trbs == NULL) {
		return (-1);
	}
	memset(ring->trbs, 0, PAGE_SIZE);
	ring->phys = xhci_phys(ring->trbs);
	if (ring->phys == 0) {
		kmem_free(ring->trbs);
		return (-1);
	}
	ring->index = 0;
	ring->cycle = 1;
	ring->trbs[XHCI_RING_TRBS - 1].parameter = ring->phys;
	ring->trbs[XHCI_RING_TRBS - 1].control =
	    (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
	    XHCI_TRB_CYCLE;
	return (0);
}

static u64
xhci_ring_put(xhci_ring_t *ring, u64 parameter, u32 status, u32 control)
{
	u64	phys;

	phys = ring->phys + (u64)ring->index * sizeof(xhci_trb_t);
	ring->trbs[ring->index].parameter = parameter;
	ring->trbs[ring->index].status = status;
	ring->trbs[ring->index].control = control | ring->cycle;
	__asm__ volatile("" ::: "memory");
	ring->index++;
	if (ring->index == XHCI_RING_TRBS - 1) {
		ring->trbs[ring->index].control =
		    (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
		    ring->cycle;
		ring->index = 0;
		ring->cycle ^= 1;
	}
	return (phys);
}

static void
xhci_intr_ack(xhci_state_t *state)
{
	volatile u32	*iman;
	u32		value;

	*(volatile u32 *)(state->op + XHCI_USBSTS) = XHCI_USBSTS_EINT;
	iman = (volatile u32 *)(state->runtime + XHCI_IMAN);
	value = *iman;
	*iman = value | XHCI_IMAN_IP | XHCI_IMAN_IE;
}

static int
xhci_event_get(xhci_state_t *state, xhci_trb_t *event)
{
	xhci_trb_t	*source;

	source = &state->events[state->event_index];
	if ((source->control & XHCI_TRB_CYCLE) != state->event_cycle) {
		return (0);
	}
	xhci_intr_ack(state);
	*event = *source;
	__asm__ volatile("" ::: "memory");
	state->event_index++;
	if (state->event_index == XHCI_RING_TRBS) {
		state->event_index = 0;
		state->event_cycle ^= 1;
	}
	*(volatile u32 *)(state->runtime + XHCI_ERDP) =
	    (u32)(state->events_phys + (u64)state->event_index *
	    sizeof(*source) | 8);
	*(volatile u32 *)(state->runtime + XHCI_ERDP + 4) =
	    (state->events_phys + (u64)state->event_index *
	    sizeof(*source)) >> 32;
	return (1);
}

static int
xhci_wait_event(xhci_state_t *state, u64 trb_phys, u8 type, u8 slot,
    u32 limit, u32 *residual)
{
	xhci_trb_t	event;
	u32		count;

	for (count = 0; count < limit; count++) {
		if (!xhci_event_get(state, &event)) {
			__asm__ volatile("pause");
			continue;
		}
		if (((event.control >> XHCI_TRB_TYPE_SHIFT) & 0x3F) != type ||
		    (slot != 0 && (event.control >> XHCI_TRB_SLOT_SHIFT) != slot) ||
		    (trb_phys != 0 && event.parameter != trb_phys)) {
			if (((event.control >> XHCI_TRB_TYPE_SHIFT) & 0x3F) ==
			    XHCI_TRB_TYPE_TRANSFER_EVENT) {
				xhci_handle_transfer_event(state, &event);
			}
			continue;
		}
		if (residual != NULL) {
			*residual = event.status & 0xFFFFFF;
		}
		return ((((event.status >> 24) & 0xFF) == XHCI_CC_SUCCESS ||
		    ((event.status >> 24) & 0xFF) == XHCI_CC_SHORT_PACKET) ?
		    0 : -1);
	}
	return (-1);
}

static int
xhci_command_locked(xhci_state_t *state, u64 parameter, u32 status, u32 control,
    u8 *slot)
{
	u64	trb;
	xhci_trb_t	event;
	u32		count;

	trb = xhci_ring_put(&state->command_ring, parameter, status, control);
	*(volatile u32 *)state->doorbell = 0;
	for (count = 0; count < 1000000; count++) {
		if (!xhci_event_get(state, &event)) {
			__asm__ volatile("pause");
			continue;
		}
		if (((event.control >> XHCI_TRB_TYPE_SHIFT) & 0x3F) !=
		    XHCI_TRB_TYPE_COMMAND_EVENT || event.parameter != trb) {
			if (((event.control >> XHCI_TRB_TYPE_SHIFT) & 0x3F) ==
			    XHCI_TRB_TYPE_TRANSFER_EVENT) {
				xhci_handle_transfer_event(state, &event);
			}
			continue;
		}
		if (((event.status >> 24) & 0xFF) != XHCI_CC_SUCCESS) {
			usb_log_printf("xhci: command type=%u cc=0x%x "
			    "slot=%u\n",
			    (control >> XHCI_TRB_TYPE_SHIFT) & 0x3F,
			    (event.status >> 24) & 0xFF,
			    (event.control >> XHCI_TRB_SLOT_SHIFT) & 0xFF);
			return (-1);
		}
		if (slot != NULL) {
			*slot = event.control >> XHCI_TRB_SLOT_SHIFT;
		}
		return (0);
	}
	usb_log_printf("xhci: command type=%u timed out\n",
	    (control >> XHCI_TRB_TYPE_SHIFT) & 0x3F);
	return (-1);
}

static int
xhci_command(xhci_state_t *state, u64 parameter, u32 status, u32 control,
    u8 *slot)
{
	u64	flags;
	int	status_code;

	(void)xhci_drain_enter(state, &flags);
	status_code = xhci_command_locked(state, parameter, status, control,
	    slot);
	xhci_drain_exit(state, flags);
	return (status_code);
}

static u32 *
xhci_context(void *base, u8 size, u8 index)
{
	return ((u32 *)((u8 *)base + (u32)size * index));
}

static u8
xhci_context_size(xhci_state_t *state)
{
	return ((*(volatile u32 *)(state->cap + 0x10) & 4) ? 64 : 32);
}

static int
xhci_port_connected(void *priv, u8 port, u8 *speed)
{
	xhci_state_t	*state;
	u32		value;

	state = priv;
	if (state == NULL || port == 0 || port > state->max_ports) {
		return (0);
	}
	value = *(volatile u32 *)(state->op + XHCI_PORTSC_BASE +
	    (u32)(port - 1) * XHCI_PORTSC_STRIDE);
	if ((value & XHCI_PORTSC_CCS) == 0) {
		return (0);
	}
	*speed = (value >> XHCI_PORTSC_SPEED_SHIFT) & 0xF;
	return (1);
}

static int
xhci_port_reset(void *priv, u8 port)
{
	xhci_state_t	*state;
	volatile u32	*reg;
	u32		value;

	state = priv;
	if (state == NULL || port == 0 || port > state->max_ports) {
		return (-1);
	}
	reg = (volatile u32 *)(state->op + XHCI_PORTSC_BASE +
	    (u32)(port - 1) * XHCI_PORTSC_STRIDE);
	value = *reg;
	usb_log_printf("xhci: port %u reset before=%x\n", port, value);
	*reg = value | XHCI_PORTSC_PR;
	if (xhci_wait32(reg, XHCI_PORTSC_PR, 0, 1000000) != 0) {
		usb_log_printf("xhci: port %u reset timed out\n", port);
		return (-1);
	}
	usb_log_printf("xhci: port %u reset after=%x\n", port, *reg);
	return ((*reg & XHCI_PORTSC_PED) ? 0 : -1);
}

static int
xhci_endpoint_ring(xhci_device_t *device, u8 ep_id)
{
	if (ep_id >= 32) {
		return (-1);
	}
	if (device->eps[ep_id].ring.trbs == NULL &&
	    xhci_ring_create(&device->eps[ep_id].ring) != 0) {
		return (-1);
	}
	return (0);
}

static void
xhci_device_free(xhci_state_t *state, xhci_device_t *device)
{
	u8	ep_id;

	(void)state;
	if (device == NULL) {
		return;
	}
	for (ep_id = 1; ep_id < 32; ep_id++) {
		if (device->eps[ep_id].ring.trbs != NULL) {
			kmem_free(device->eps[ep_id].ring.trbs);
		}
	}
	kmem_free(device->output_ctx);
	kmem_free(device->input_ctx);
	kmem_free(device);
}

static void
xhci_slot_release(xhci_state_t *state, u8 slot_id, xhci_device_t *device)
{
	if (slot_id == 0 || slot_id > state->max_slots) {
		xhci_device_free(state, device);
		return;
	}
	state->dcbaa[slot_id] = 0;
	state->slots[slot_id] = NULL;
	(void)xhci_command(state, 0, 0,
	    (XHCI_TRB_TYPE_DISABLE_SLOT << XHCI_TRB_TYPE_SHIFT) |
	    ((u32)slot_id << XHCI_TRB_SLOT_SHIFT), NULL);
	xhci_device_free(state, device);
}

static int
xhci_address_device(void *priv, usb_device_t *dev)
{
	xhci_state_t	*state;
	xhci_device_t	*device;
	u32		*input;
	u32		*slot;
	u32		*ep0;
	u32		dev_addr;
	u32		ep0_state;
	u8		ctx_size;
	u8		slot_id;

	state = priv;
	if (state == NULL || dev == NULL || xhci_command(state, 0, 0,
	    XHCI_TRB_TYPE_ENABLE_SLOT << XHCI_TRB_TYPE_SHIFT, &slot_id) != 0 ||
	    slot_id == 0) {
		return (-1);
	}
	device = kmem_calloc(1, sizeof(*device));
	if (device == NULL) {
		xhci_slot_release(state, slot_id, NULL);
		return (-1);
	}
	ctx_size = xhci_context_size(state);
	device->output_ctx = kmem_alloc_aligned(33 * ctx_size,
	    64);
	device->input_ctx = kmem_alloc_aligned(34 * ctx_size,
	    64);
	if (device->output_ctx == NULL ||
	    device->input_ctx == NULL || xhci_endpoint_ring(device, 1) != 0) {
		xhci_slot_release(state, slot_id, device);
		return (-1);
	}
	memset(device->output_ctx, 0, 33 * ctx_size);
	memset(device->input_ctx, 0, 34 * ctx_size);
	device->output_phys = xhci_phys(device->output_ctx);
	device->input_phys = xhci_phys(device->input_ctx);
	if (device->output_phys == 0 || device->input_phys == 0) {
		xhci_slot_release(state, slot_id, device);
		return (-1);
	}
	input = xhci_context(device->input_ctx, ctx_size, 0);
	input[1] = 3;
	slot = xhci_context(device->input_ctx, ctx_size, 1);
	slot[0] = ((u32)dev->speed << XHCI_SLOT_SPEED_SHIFT) |
	    (1U << XHCI_CTX_ENTRIES_SHIFT);
	slot[1] = (u32)dev->port << XHCI_SLOT_PORT_SHIFT;
	ep0 = xhci_context(device->input_ctx, ctx_size, 2);
	ep0[1] = (3U << 1) | (4U << XHCI_EP_TYPE_SHIFT) |
	    ((dev->speed >= USB_SPEED_SUPER ? 512U :
	    (dev->speed == USB_SPEED_HIGH ? 64U : 8U)) <<
	    XHCI_EP_MAX_PACKET_SHIFT);
	ep0[2] = (u32)(device->eps[1].ring.phys | XHCI_EP_DEQUEUE_CYCLE);
	ep0[3] = device->eps[1].ring.phys >> 32;
	ep0[4] = 8U << 16;
	state->dcbaa[slot_id] = device->output_phys;
	if (xhci_command(state, device->input_phys, 0,
	    (XHCI_TRB_TYPE_ADDRESS_DEVICE << XHCI_TRB_TYPE_SHIFT) |
	    ((u32)slot_id << XHCI_TRB_SLOT_SHIFT), NULL) != 0) {
		usb_log_printf("xhci: port %u address device failed "
		    "(slot %u)\n", dev->port, slot_id);
		usb_log_flush();
		xhci_slot_release(state, slot_id, device);
		return (-1);
	}
	device->usb = dev;
	state->slots[slot_id] = device;
	dev->slot_id = slot_id;
	dev->address = slot_id;
	ep0_state = xhci_context(device->output_ctx, ctx_size, 1)[0] & 7;
	dev_addr = xhci_context(device->output_ctx, ctx_size, 0)[3] & 0xFF;
	usb_log_printf("xhci: address ok slot=%u speed=%u ctx=%u sp=%u "
	    "ep0state=%u "
	    "addr=%u ring=%llx outdeq=%llx\n", slot_id, dev->speed, ctx_size,
	    state->scratch_count, ep0_state, dev_addr,
	    (unsigned long long)device->eps[1].ring.phys,
	    (unsigned long long)xhci_context(device->output_ctx, ctx_size,
	    1)[2] | ((unsigned long long)xhci_context(device->output_ctx,
	    ctx_size, 1)[3] << 32));
	return (0);
}

static int
xhci_remove_device(void *priv, usb_device_t *dev)
{
	xhci_state_t	*state;
	xhci_device_t	*device;
	u8		slot_id;

	state = priv;
	if (state == NULL || dev == NULL) {
		return (-1);
	}
	slot_id = dev->slot_id;
	if (slot_id == 0 || slot_id > state->max_slots ||
	    (device = state->slots[slot_id]) == NULL) {
		return (-1);
	}
	dev->slot_id = 0;
	xhci_slot_release(state, slot_id, device);
	return (0);
}

static int
xhci_update_ep0(void *priv, usb_device_t *dev)
{
	xhci_state_t	*state;
	xhci_device_t	*device;
	u32		*input;
	u32		*ep0;
	u8		ctx_size;

	state = priv;
	if (state == NULL || dev == NULL || dev->slot_id == 0 ||
	    (device = state->slots[dev->slot_id]) == NULL) {
		return (-1);
	}
	ctx_size = xhci_context_size(state);
	memset(device->input_ctx, 0, 34 * ctx_size);
	input = xhci_context(device->input_ctx, ctx_size, 0);
	input[1] = 2;
	ep0 = xhci_context(device->input_ctx, ctx_size, 2);
	ep0[1] = (3U << 1) | (4U << XHCI_EP_TYPE_SHIFT) |
	    ((u32)dev->max_packet_size0 << XHCI_EP_MAX_PACKET_SHIFT);
	ep0[2] = (u32)(device->eps[1].ring.phys | XHCI_EP_DEQUEUE_CYCLE);
	ep0[3] = device->eps[1].ring.phys >> 32;
	ep0[4] = (u32)dev->max_packet_size0 << 16;
	return (xhci_command(state, device->input_phys, 0,
	    (XHCI_TRB_TYPE_EVALUATE_CONTEXT << XHCI_TRB_TYPE_SHIFT) |
	    ((u32)dev->slot_id << XHCI_TRB_SLOT_SHIFT), NULL));
}

static u8
xhci_endpoint_id(usb_endpoint_t *ep)
{
	return ((u8)(((ep->address & 0x0F) << 1) | ((ep->address >> 7) & 1)));
}

static u8
xhci_endpoint_type(usb_endpoint_t *ep)
{
	u8	type;

	type = ep->attributes & 3;
	if (type == USB_ENDPOINT_XFER_BULK) {
		return ((ep->address & USB_DIR_IN) ? 6 : 2);
	}
	if (type == USB_ENDPOINT_XFER_INT) {
		return ((ep->address & USB_DIR_IN) ? 7 : 3);
	}
	return (0);
}

static int
xhci_endpoint_supported(usb_endpoint_t *ep)
{
	u8	type;

	if (ep == NULL) {
		return (0);
	}
	type = ep->attributes & 3;
	return (type == USB_ENDPOINT_XFER_BULK ||
	    type == USB_ENDPOINT_XFER_INT);
}

static int
xhci_interface_endpoints(usb_device_t *dev, u8 interface_number,
    u8 alternate, u32 *mask)
{
	usb_endpoint_t	*endpoint;
	usb_interface_t	*interface;
	u32		result;
	u8		endpoint_id;
	u8		index;
	u8		iface;

	if (dev == NULL || mask == NULL) {
		return (-1);
	}
	result = 0;
	for (iface = 0; iface < dev->interface_count; iface++) {
		interface = &dev->interfaces[iface];
		if (interface->number != interface_number ||
		    interface->alternate != alternate) {
			continue;
		}
		for (index = 0; index < interface->endpoint_count; index++) {
			endpoint = &interface->endpoints[index];
			if (!xhci_endpoint_supported(endpoint)) {
				usb_log_printf("xhci: skip unsupported ep "
				    "%02x on interface %u alt %u\n",
				    endpoint->address, interface_number,
				    alternate);
				continue;
			}
			endpoint_id = xhci_endpoint_id(endpoint);
			if (endpoint_id == 0 || endpoint_id >= 32) {
				return (-1);
			}
			if ((result & (1U << endpoint_id)) != 0) {
				return (-1);
			}
			result |= 1U << endpoint_id;
		}
	}
	*mask = result;
	return (0);
}

static int
xhci_fill_endpoint_contexts(xhci_device_t *device, usb_device_t *dev,
    u8 ctx_size, u32 mask)
{
	usb_endpoint_t	*endpoint;
	usb_interface_t	*interface;
	u32		*ep;
	u8		endpoint_id;
	u8		index;
	u8		iface;

	if (device == NULL || dev == NULL) {
		return (-1);
	}
	for (iface = 0; iface < dev->interface_count; iface++) {
		interface = &dev->interfaces[iface];
		for (index = 0; index < interface->endpoint_count; index++) {
			endpoint = &interface->endpoints[index];
			endpoint_id = xhci_endpoint_id(endpoint);
			if (endpoint_id == 0 || endpoint_id >= 32 ||
			    (mask & (1U << endpoint_id)) == 0) {
				continue;
			}
			if (!xhci_endpoint_supported(endpoint) ||
			    xhci_endpoint_ring(device, endpoint_id) != 0) {
				return (-1);
			}
			ep = xhci_context(device->input_ctx, ctx_size,
			    endpoint_id + 1);
			ep[0] = endpoint->interval;
			ep[1] = (3U << 1) |
			    ((u32)xhci_endpoint_type(endpoint) <<
			    XHCI_EP_TYPE_SHIFT) |
			    ((u32)endpoint->max_packet_size <<
			    XHCI_EP_MAX_PACKET_SHIFT);
			ep[2] = (u32)(device->eps[endpoint_id].ring.phys |
			    XHCI_EP_DEQUEUE_CYCLE);
			ep[3] = device->eps[endpoint_id].ring.phys >> 32;
			ep[4] = (u32)endpoint->max_packet_size << 16;
		}
	}
	return (0);
}

static void
xhci_endpoint_ring_reset(xhci_device_t *device, u8 endpoint_id)
{
	xhci_endpoint_t	*endpoint;

	if (device == NULL || endpoint_id >= 32) {
		return;
	}
	endpoint = &device->eps[endpoint_id];
	if (endpoint->ring.trbs == NULL) {
		return;
	}
	memset(endpoint->ring.trbs, 0, PAGE_SIZE);
	endpoint->ring.index = 0;
	endpoint->ring.cycle = 1;
	endpoint->ring.trbs[XHCI_RING_TRBS - 1].parameter =
	    endpoint->ring.phys;
	endpoint->ring.trbs[XHCI_RING_TRBS - 1].control =
	    (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
	    XHCI_TRB_CYCLE;
	endpoint->active = 0;
	endpoint->complete = NULL;
	endpoint->complete_arg = NULL;
	endpoint->buffer = NULL;
	endpoint->length = 0;
}

static int
xhci_configure_endpoints(xhci_state_t *state, usb_device_t *dev,
    u32 add_mask, u32 drop_mask)
{
	xhci_device_t	*device;
	u32		*input;
	u32		*slot;
	u8		ctx_size;
	u8		endpoint_id;
	int		ret;

	if (state == NULL || dev == NULL || dev->slot_id == 0 ||
	    (device = state->slots[dev->slot_id]) == NULL) {
		return (-1);
	}
	if (add_mask == 0 && drop_mask == 0) {
		return (0);
	}
	ctx_size = xhci_context_size(state);
	memset(device->input_ctx, 0, 34 * ctx_size);
	input = xhci_context(device->input_ctx, ctx_size, 0);
	input[0] = drop_mask;
	input[1] = 1U | add_mask;
	if (xhci_fill_endpoint_contexts(device, dev, ctx_size,
	    add_mask) != 0) {
		return (-1);
	}
	slot = xhci_context(device->input_ctx, ctx_size, 1);
	memcpy(slot, xhci_context(device->output_ctx, ctx_size, 0), ctx_size);
	slot[0] &= ~(0x1FU << XHCI_CTX_ENTRIES_SHIFT);
	slot[0] |= 31U << XHCI_CTX_ENTRIES_SHIFT;
	usb_log_printf("xhci: configure ep slot=%u add=%x drop=%x\n",
	    dev->slot_id, add_mask, drop_mask);
	ret = xhci_command(state, device->input_phys, 0,
	    (XHCI_TRB_TYPE_CONFIGURE_ENDPOINT << XHCI_TRB_TYPE_SHIFT) |
	    ((u32)dev->slot_id << XHCI_TRB_SLOT_SHIFT), NULL);
	if (ret != 0) {
		return (ret);
	}
	for (endpoint_id = 1; endpoint_id < 32; endpoint_id++) {
		if ((drop_mask & (1U << endpoint_id)) != 0) {
			xhci_endpoint_ring_reset(device, endpoint_id);
		}
	}
	return (0);
}

static int
xhci_configure_device(void *priv, usb_device_t *dev)
{
	xhci_state_t	*state;
	xhci_device_t	*device;
	u32		add;
	u32		mask;
	u8		index;
	int		ret;

	state = priv;
	if (state == NULL || dev == NULL || dev->slot_id == 0 ||
	    (device = state->slots[dev->slot_id]) == NULL) {
		return (-1);
	}
	add = 0;
	for (index = 0; index < dev->interface_count; index++) {
		if (dev->interfaces[index].alternate != 0) {
			continue;
		}
		if (xhci_interface_endpoints(dev,
		    dev->interfaces[index].number, 0, &mask) != 0) {
			return (-1);
		}
		add |= mask;
	}
	if (add == 0) {
		device->active_endpoints = 0;
		return (0);
	}
	ret = xhci_configure_endpoints(state, dev, add, 0);
	if (ret == 0) {
		device->active_endpoints = add;
	}
	return (ret);
}

static int
xhci_configure_interface(void *priv, usb_device_t *dev,
    u8 interface_number, u8 alternate)
{
	xhci_state_t	*state;
	xhci_device_t	*device;
	u32		add_mask;
	u32		drop_mask;
	u32		mask;
	u32		new_active;
	u32		old_mask;
	u8		index;
	int		ret;

	state = priv;
	if (state == NULL || dev == NULL || dev->slot_id == 0 ||
	    (device = state->slots[dev->slot_id]) == NULL) {
		return (-1);
	}
	if (xhci_interface_endpoints(dev, interface_number, alternate,
	    &add_mask) != 0) {
		return (-1);
	}
	old_mask = 0;
	for (index = 0; index < dev->interface_count; index++) {
		if (dev->interfaces[index].number != interface_number) {
			continue;
		}
		if (xhci_interface_endpoints(dev, interface_number,
		    dev->interfaces[index].alternate, &mask) != 0) {
			return (-1);
		}
		old_mask |= mask;
	}
	old_mask &= device->active_endpoints;
	drop_mask = old_mask & ~add_mask;
	if (drop_mask == 0 && add_mask == old_mask) {
		return (0);
	}
	ret = xhci_configure_endpoints(state, dev, add_mask, drop_mask);
	if (ret != 0) {
		return (ret);
	}
	new_active = (device->active_endpoints & ~old_mask) | add_mask;
	device->active_endpoints = new_active;
	return (0);
}

static int
xhci_transfer_wait(xhci_state_t *state, xhci_endpoint_t *endpoint,
    u64 event_trb, u8 slot, u32 timeout, u32 *length)
{
	u32	residual;

	if (xhci_wait_event(state, event_trb, XHCI_TRB_TYPE_TRANSFER_EVENT,
	    slot, timeout == 0 ? 1000000 : timeout * 1000, &residual) != 0) {
		return (-1);
	}
	if (length != NULL && residual <= *length) {
		*length -= residual;
	}
	(void)endpoint;
	return (0);
}

static int
xhci_normal_transfer_locked(xhci_state_t *state, usb_device_t *dev,
    usb_endpoint_t *ep, void *data, u32 *length, u32 timeout)
{
	xhci_device_t	*device;
	xhci_endpoint_t	*endpoint;
	u64		phys;
	u64		last;
	u32		left;
	u32		chunk;
	u8		ep_id;

	if (dev->slot_id == 0 || (device = state->slots[dev->slot_id]) == NULL ||
	    (ep_id = xhci_endpoint_id(ep)) == 0 || ep_id >= 32 ||
	    *length == 0) {
		return (-1);
	}
	endpoint = &device->eps[ep_id];
	left = *length;
	phys = xhci_phys(data);
	if (phys == 0) {
		return (-1);
	}
	last = 0;
	while (left != 0) {
		chunk = PAGE_SIZE - (phys & (PAGE_SIZE - 1));
		if (chunk > left) {
			chunk = left;
		}
		last = xhci_ring_put(&endpoint->ring, phys, chunk,
		    XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT);
		left -= chunk;
		phys = xhci_phys((u8 *)data + (*length - left));
		if (left != 0 && phys == 0) {
			return (-1);
		}
	}
	endpoint->ring.trbs[(last - endpoint->ring.phys) / sizeof(xhci_trb_t)].
	    control |= XHCI_TRB_IOC;
	*(volatile u32 *)(state->doorbell + (u32)dev->slot_id * 4) =
	    (u32)ep_id;
	return (xhci_transfer_wait(state, endpoint, last, dev->slot_id, timeout,
	    length));
}

static int
xhci_normal_transfer(xhci_state_t *state, usb_device_t *dev,
    usb_endpoint_t *ep, void *data, u32 *length, u32 timeout)
{
	u64	flags;
	int	status;

	if (state == NULL) {
		return (-1);
	}
	(void)xhci_drain_enter(state, &flags);
	status = xhci_normal_transfer_locked(state, dev, ep, data, length,
	    timeout);
	xhci_drain_exit(state, flags);
	return (status);
}

static int
xhci_control_locked(xhci_state_t *state, usb_device_t *dev,
    const usb_setup_t *setup, void *data, u16 length, u32 timeout)
{
	xhci_device_t	*device;
	xhci_endpoint_t	*endpoint;
	u64		phys;
	u64		last;
	u32		control;
	u32		done;

	if (dev == NULL || setup == NULL || dev->slot_id == 0 ||
	    (device = state->slots[dev->slot_id]) == NULL) {
		return (-1);
	}
	endpoint = &device->eps[1];
	control = (XHCI_TRB_TYPE_SETUP << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_IDT;
	control |= XHCI_TRB_IOC;
	if (length != 0) {
		control |= (setup->bmRequestType & USB_DIR_IN) ?
		    XHCI_TRT_IN : XHCI_TRT_OUT;
	}
	(void)xhci_ring_put(&endpoint->ring, *(const u64 *)setup, 8, control);
	last = 0;
	if (length != 0) {
		phys = xhci_phys(data);
		if (phys == 0) {
			return (-1);
		}
		(void)xhci_ring_put(&endpoint->ring, phys, length,
		    (XHCI_TRB_TYPE_DATA << XHCI_TRB_TYPE_SHIFT) |
		    ((setup->bmRequestType & USB_DIR_IN) ? XHCI_TRB_DIR_IN : 0));
	}
	last = xhci_ring_put(&endpoint->ring, 0, 0,
	    (XHCI_TRB_TYPE_STATUS << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_IOC |
	    ((length != 0 && (setup->bmRequestType & USB_DIR_IN)) ? 0 :
	    XHCI_TRB_DIR_IN));
	__asm__ volatile("sfence" ::: "memory");
	*(volatile u32 *)(state->doorbell + (u32)dev->slot_id * 4) =
	    1U;
	done = length;
	if (xhci_transfer_wait(state, endpoint, last, dev->slot_id, timeout,
	    &done) != 0) {
		usb_log_printf("xhci: slot %u control transfer failed "
		    "req=0x%x\n",
		    dev->slot_id, setup->bRequest);
		return (-1);
	}
	return (0);
}

static int
xhci_control(void *priv, usb_device_t *dev, const usb_setup_t *setup,
    void *data, u16 length, u32 timeout)
{
	xhci_state_t	*state;
	u64		flags;
	int		status;

	state = priv;
	if (state == NULL) {
		return (-1);
	}
	(void)xhci_drain_enter(state, &flags);
	status = xhci_control_locked(state, dev, setup, data, length, timeout);
	xhci_drain_exit(state, flags);
	return (status);
}

static int
xhci_bulk(void *priv, usb_device_t *dev, usb_endpoint_t *ep, void *data,
    u32 *length, u32 timeout)
{
	return (xhci_normal_transfer((xhci_state_t *)priv, dev, ep, data, length,
    timeout));
}

static int
xhci_interrupt_locked(xhci_state_t *state, usb_device_t *dev,
    usb_endpoint_t *ep, void *data, u32 length, usb_complete_t complete,
    void *arg)
{
	xhci_device_t	*device;
	xhci_endpoint_t	*endpoint;
	u64		trb;
	u8		ep_id;

	if (dev == NULL || ep == NULL || data == NULL ||
	    complete == NULL || dev->slot_id == 0 ||
	    (device = state->slots[dev->slot_id]) == NULL ||
	    (ep_id = xhci_endpoint_id(ep)) == 0 || ep_id >= 32) {
		return (-1);
	}
	endpoint = &device->eps[ep_id];
	if (endpoint->active) {
		return (-1);
	}
	trb = xhci_ring_put(&endpoint->ring, xhci_phys(data), length,
	    (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_IOC);
	if (trb == 0) {
		return (-1);
	}
	endpoint->complete = complete;
	endpoint->complete_arg = arg;
	endpoint->buffer = data;
	endpoint->length = length;
	endpoint->active = 1;
	*(volatile u32 *)(state->doorbell + (u32)dev->slot_id * 4) =
	    (u32)ep_id;
	return (0);
}

static int
xhci_interrupt(void *priv, usb_device_t *dev, usb_endpoint_t *ep,
    void *data, u32 length, usb_complete_t complete, void *arg)
{
	xhci_state_t	*state;
	u64		flags;
	int		status;

	state = priv;
	if (state == NULL) {
		return (-1);
	}
	(void)xhci_drain_enter(state, &flags);
	status = xhci_interrupt_locked(state, dev, ep, data, length, complete,
	    arg);
	xhci_drain_exit(state, flags);
	return (status);
}

static int
xhci_bulk_submit(void *priv, usb_device_t *dev, usb_endpoint_t *ep,
    void *data, u32 length, usb_complete_t complete, void *arg)
{
	return (xhci_interrupt(priv, dev, ep, data, length, complete, arg));
}

static void
xhci_handle_transfer_event(xhci_state_t *state, const xhci_trb_t *event)
{
	xhci_device_t	*device;
	xhci_endpoint_t	*endpoint;
	u32		residual;
	u8		ep_id;
	u8		slot;

	slot = event->control >> XHCI_TRB_SLOT_SHIFT;
	ep_id = (event->control >> XHCI_TRB_EP_SHIFT) & 0x1F;
	if (slot == 0 || ep_id == 0 || ep_id >= 32 ||
	    (device = state->slots[slot]) == NULL) {
		return;
	}
	endpoint = &device->eps[ep_id];
	if (!endpoint->active || endpoint->complete == NULL) {
		return;
	}
	residual = event->status & 0xFFFFFF;
	endpoint->active = 0;
	endpoint->complete(device->usb, endpoint->buffer,
	    residual <= endpoint->length ? endpoint->length - residual : 0,
	    (((event->status >> 24) & 0xFF) == XHCI_CC_SUCCESS ||
	    ((event->status >> 24) & 0xFF) == XHCI_CC_SHORT_PACKET) ? 0 : -1,
	    endpoint->complete_arg);
}

static void
xhci_poll(void *arg)
{
	xhci_state_t	*state;
	u64		flags;
	u32		value;
	u8		port;

	state = arg;
	if (state == NULL) {
		return;
	}
	if (!xhci_drain_enter(state, &flags)) {
		state->rescan = 1;
		xhci_drain_exit(state, flags);
		return;
	}
	usb_controller_scan(&state->usb);
	for (port = 1; port <= state->max_ports; port++) {
		value = *(volatile u32 *)(state->op + XHCI_PORTSC_BASE +
		    (u32)(port - 1) * XHCI_PORTSC_STRIDE);
		if ((value & XHCI_PORTSC_CSC) == 0) {
			continue;
		}
		usb_log_printf("xhci: port %u csc (portsc=%x)\n", port,
		    value);
		usb_controller_port_retry(&state->usb, port);
		value |= XHCI_PORTSC_CSC;
		*(volatile u32 *)(state->op + XHCI_PORTSC_BASE +
		    (u32)(port - 1) * XHCI_PORTSC_STRIDE) =
		    value;
		usb_controller_scan(&state->usb);
	}
	xhci_drain_ring(state);
	xhci_drain_exit(state, flags);
}

static int
xhci_intr(void *arg)
{
	xhci_state_t	*state;
	volatile u32	*status;

	state = arg;
	if (state == NULL) {
		return (-1);
	}
	status = (volatile u32 *)(state->op + XHCI_USBSTS);
	if ((*status & XHCI_USBSTS_EINT) == 0) {
		if (state->irq_msi) {
			return (0);
		}
		return (-1);
	}
	xhci_intr_ack(state);
	xhci_poll(state);
	return (0);
}

static int
xhci_drain_enter(xhci_state_t *state, u64 *flags)
{
	int	outermost;

	__asm__ volatile("pushfq; popq %0; cli" : "=r"(*flags) :: "memory");
	outermost = (state->drain_depth == 0);
	state->drain_depth++;
	if ((*flags & XHCI_RFLAGS_IF) != 0) {
		__asm__ volatile("sti" ::: "memory");
	}
	return (outermost);
}

static void
xhci_drain_ring(xhci_state_t *state)
{
	xhci_trb_t	event;
	u32		value;
	u8		port;

	while (xhci_event_get(state, &event)) {
		switch ((event.control >> XHCI_TRB_TYPE_SHIFT) & 0x3F) {
		case XHCI_TRB_TYPE_PORT_STATUS_EVENT:
			port = (u8)((event.control >> 24) & 0xFF);
			if (port >= 1 && port <= state->max_ports) {
				value = *(volatile u32 *)(state->op +
				    XHCI_PORTSC_BASE + (u32)(port - 1) *
				    XHCI_PORTSC_STRIDE);
				if (value & XHCI_PORTSC_CSC) {
					usb_log_printf("xhci: port %u csc "
					    "evt (portsc=%x)\n", port, value);
					usb_controller_port_retry(&state->usb,
					    port);
					value |= XHCI_PORTSC_CSC;
					*(volatile u32 *)(state->op +
					    XHCI_PORTSC_BASE +
					    (u32)(port - 1) *
					    XHCI_PORTSC_STRIDE) =
					    value;
				}
			}
			usb_controller_scan(&state->usb);
			break;
		case XHCI_TRB_TYPE_TRANSFER_EVENT:
			xhci_handle_transfer_event(state, &event);
			break;
		default:
			break;
		}
	}
}

static void
xhci_drain_exit(xhci_state_t *state, u64 flags)
{
	if (state->drain_depth > 1) {
		__asm__ volatile("cli" ::: "memory");
		state->drain_depth--;
		if ((flags & XHCI_RFLAGS_IF) != 0) {
			__asm__ volatile("sti" ::: "memory");
		}
		return;
	}
	for (;;) {
		__asm__ volatile("cli" ::: "memory");
		if (state->rescan == 0) {
			break;
		}
		state->rescan = 0;
		if ((flags & XHCI_RFLAGS_IF) != 0) {
			__asm__ volatile("sti" ::: "memory");
		}
		xhci_drain_ring(state);
	}
	state->drain_depth = 0;
	if ((flags & XHCI_RFLAGS_IF) != 0) {
		__asm__ volatile("sti" ::: "memory");
	}
}

static int
xhci_wait32(volatile u32 *reg, u32 mask, u32 value, u32 limit)
{
	u32	count;

	for (count = 0; count < limit; count++) {
		if ((*reg & mask) == value) {
			return (0);
		}
		__asm__ volatile("pause");
	}
	return (-1);
}

static int
xhci_halt(xhci_state_t *state)
{
	volatile u32	*cmd;
	volatile u32	*status;

	cmd = (volatile u32 *)(state->op + XHCI_USBCMD);
	status = (volatile u32 *)(state->op + XHCI_USBSTS);
	*cmd &= ~XHCI_USBCMD_RUN;
	return (xhci_wait32(status, XHCI_USBSTS_HCH, XHCI_USBSTS_HCH,
	    1000000));
}

static int
xhci_reset(xhci_state_t *state)
{
	volatile u32	*cmd;
	volatile u32	*status;

	cmd = (volatile u32 *)(state->op + XHCI_USBCMD);
	status = (volatile u32 *)(state->op + XHCI_USBSTS);
	if (xhci_halt(state) != 0 ||
	    xhci_wait32(status, XHCI_USBSTS_CNR, 0, 1000000) != 0) {
		return (-1);
	}
	*cmd |= XHCI_USBCMD_HCRST;
	if (xhci_wait32(cmd, XHCI_USBCMD_HCRST, 0, 1000000) != 0) {
		return (-1);
	}
	return (xhci_wait32(status, XHCI_USBSTS_CNR, 0, 1000000));
}

static u64
xhci_phys(void *ptr)
{
	u64	phys;

	phys = pmap_extract((u64)ptr & ~((u64)PAGE_SIZE - 1));
	if (phys == 0) {
		return (0);
	}
	return (phys | ((u64)ptr & (PAGE_SIZE - 1)));
}

static int
xhci_setup_rings(xhci_state_t *state)
{
	volatile u32	*iman;
	volatile u64	*reg64;
	void		*buf;
	u32		hcs2;
	u32		value;
	u32		num_sp;
	u32		sp_index;

	state->dcbaa = kmem_alloc_aligned((state->max_slots + 1) * 8,
	    64);
	state->command_ring.trbs = kmem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
	state->events = kmem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
	state->erst = kmem_alloc_aligned(64, 64);
	if (state->dcbaa == NULL || state->command_ring.trbs == NULL ||
	    state->events == NULL || state->erst == NULL) {
		return (-1);
	}
	memset(state->dcbaa, 0, (state->max_slots + 1) * 8);
	memset(state->command_ring.trbs, 0, PAGE_SIZE);
	memset(state->events, 0, PAGE_SIZE);
	memset(state->erst, 0, 64);
	state->dcbaa_phys = xhci_phys(state->dcbaa);
	hcs2 = *(volatile u32 *)(state->cap + 0x08);
	num_sp = (((hcs2 >> 27) & 0x1F) << 5) | ((hcs2 >> 21) & 0x1F);
	if (num_sp != 0) {
		state->scratch = kmem_alloc_aligned(num_sp * 8, 64);
		state->scratch_bufs = kmem_alloc(num_sp * sizeof(void *));
		if (state->scratch == NULL || state->scratch_bufs == NULL) {
			return (-1);
		}
		memset(state->scratch, 0, num_sp * 8);
		memset(state->scratch_bufs, 0, num_sp * sizeof(void *));
		state->scratch_count = num_sp;
		state->scratch_phys = xhci_phys(state->scratch);
		if (state->scratch_phys == 0) {
			return (-1);
		}
		for (sp_index = 0; sp_index < num_sp; sp_index++) {
			buf = kmem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
			if (buf == NULL) {
				return (-1);
			}
			memset(buf, 0, PAGE_SIZE);
			state->scratch_bufs[sp_index] = buf;
			state->scratch[sp_index] = xhci_phys(buf);
			if (state->scratch[sp_index] == 0) {
				return (-1);
			}
		}
		state->dcbaa[0] = state->scratch_phys;
	}
	state->command_ring.phys = xhci_phys(state->command_ring.trbs);
	state->events_phys = xhci_phys(state->events);
	state->erst_phys = xhci_phys(state->erst);
	if (state->dcbaa_phys == 0 || state->command_ring.phys == 0 ||
	    state->events_phys == 0 || state->erst_phys == 0) {
		return (-1);
	}
	state->command_ring.index = 0;
	state->command_ring.cycle = 1;
	state->command_ring.trbs[XHCI_RING_TRBS - 1].parameter =
	    state->command_ring.phys;
	state->command_ring.trbs[XHCI_RING_TRBS - 1].control =
	    (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
	    XHCI_TRB_CYCLE;
	state->erst->address = state->events_phys;
	state->erst->size = XHCI_RING_TRBS;
	state->event_cycle = 1;
	reg64 = (volatile u64 *)(state->op + XHCI_DCBAAP);
	*(volatile u32 *)reg64 = (u32)state->dcbaa_phys;
	*(volatile u32 *)((volatile u8 *)reg64 + 4) =
	    state->dcbaa_phys >> 32;
	reg64 = (volatile u64 *)(state->op + XHCI_CRCR);
	*(volatile u32 *)reg64 =
	    (u32)(state->command_ring.phys | XHCI_TRB_CYCLE);
	*(volatile u32 *)((volatile u8 *)reg64 + 4) =
	    state->command_ring.phys >> 32;
	*(volatile u32 *)(state->op + XHCI_CONFIG) = state->max_slots;
	iman = (volatile u32 *)(state->runtime + XHCI_IMAN);
	*iman = XHCI_IMAN_IE;
	*(volatile u32 *)(state->runtime + XHCI_ERSTSZ) = 1;
	*(volatile u32 *)(state->runtime + XHCI_ERSTBA) =
	    (u32)state->erst_phys;
	*(volatile u32 *)(state->runtime + XHCI_ERSTBA + 4) =
	    state->erst_phys >> 32;
	*(volatile u32 *)(state->runtime + XHCI_ERDP) =
	    (u32)state->events_phys;
	*(volatile u32 *)(state->runtime + XHCI_ERDP + 4) =
	    state->events_phys >> 32;
	value = *(volatile u32 *)(state->op + XHCI_USBCMD);
	*(volatile u32 *)(state->op + XHCI_USBCMD) = value |
	    XHCI_USBCMD_RUN | XHCI_USBCMD_INTE;
	return (xhci_wait32((volatile u32 *)(state->op + XHCI_USBSTS),
	    XHCI_USBSTS_HCH, 0, 1000000));
}

static int
xhci_pci_probe(pci_device_t *pdev, const pci_match_t *match)
{
	xhci_state_t	*state;
	pci_bar_t	bar;
	u32		hcs1;
	u32		caplength;
	u32		dboff;
	u32		rtsoff;
	int		index, rid;

	(void)match;
	if (pci_read_bar(pdev, 0, &bar) != 0 || bar.is_io ||
	    bar.base == 0 || bar.size < 0x1000) {
		return (-1);
	}
	for (index = 0; index < XHCI_MAX_CONTROLLERS; index++) {
		if (xhci_states[index] == NULL) {
			break;
		}
	}
	if (index == XHCI_MAX_CONTROLLERS) {
		return (-1);
	}
	state = kmem_calloc(1, sizeof(*state));
	if (state == NULL) {
		return (-1);
	}
	pci_enable_memory_space(pdev);
	pci_enable_bus_mastering(pdev);
	state->pci = pdev;
	state->nb_dev = pdev->nb_device;
	state->cap = pmap_map_mmio(bar.base, bar.size);
	if (state->cap == NULL) {
		kmem_free(state);
		return (-1);
	}
	caplength = *(volatile u8 *)(state->cap + XHCI_CAP_LENGTH);
	if (caplength < 0x20 || caplength >= bar.size) {
		kmem_free(state);
		return (-1);
	}
	state->op = state->cap + caplength;
	hcs1 = *(volatile u32 *)(state->cap + XHCI_HCSPARAMS1);
	state->max_slots = hcs1 & 0xFF;
	state->max_ports = (hcs1 >> 24) & 0xFF;
	dboff = *(volatile u32 *)(state->cap + XHCI_DBOFF) & ~3U;
	rtsoff = *(volatile u32 *)(state->cap + XHCI_RTSOFF) & ~0x1FU;
	state->doorbell = state->cap + dboff;
	state->runtime = state->cap + rtsoff;
	if (state->max_slots == 0 || state->max_ports == 0 ||
	    state->max_slots > 255 || state->max_ports > USB_MAX_PORTS ||
	    dboff >= bar.size || rtsoff >= bar.size ||
	    xhci_reset(state) != 0 || xhci_setup_rings(state) != 0) {
		kmem_free(state);
		return (-1);
	}
	state->usb.bus_device = device_add_child(state->nb_dev, "usb", -1);
	if (state->usb.bus_device == NULL || usb_controller_init(&state->usb,
	    &xhci_usb_ops, state, state->usb.bus_device, state->max_ports) != 0) {
		(void)xhci_halt(state);
		xhci_state_destroy(state);
		return (-1);
	}
	rid = 0;
	state->irq_res = bus_alloc_resource_any(state->nb_dev, SYS_RES_IRQ,
	    &rid, RF_ACTIVE);
	if (state->irq_res == NULL || bus_setup_intr(state->nb_dev,
	    state->irq_res, xhci_intr, state, &state->irq_cookie) != 0) {
		if (state->irq_res != NULL) {
			bus_release_resource(state->nb_dev, SYS_RES_IRQ,
			    state->irq_res->rid, state->irq_res);
			state->irq_res = NULL;
		}
		usb_controller_fini(&state->usb);
		(void)xhci_halt(state);
		xhci_state_destroy(state);
		return (-1);
	}
	state->irq_msi = (u8)(bus_intr_is_msi(state->irq_cookie) != 0);
	pdev->driver_data = state;
	xhci_states[index] = state;
	usb_log_printf("xhci: initial port scan (slots=%u ports=%u)\n",
	    state->max_slots, state->max_ports);
	xhci_poll(state);
	usb_log_flush();
	return (0);
}

static void
xhci_state_destroy(xhci_state_t *state)
{
	int	index;

	if (state == NULL) {
		return;
	}
	if (state->scratch_bufs != NULL) {
		for (index = 0; index < (int)state->scratch_count; index++) {
			kmem_free(state->scratch_bufs[index]);
		}
		kmem_free(state->scratch_bufs);
	}
	if (state->scratch != NULL) {
		kmem_free(state->scratch);
	}
	kmem_free(state->erst);
	kmem_free(state->events);
	kmem_free(state->command_ring.trbs);
	kmem_free(state->dcbaa);
	kmem_free(state);
}

static void
xhci_pci_remove(pci_device_t *pdev)
{
	xhci_state_t	*state;
	int		index;

	state = (xhci_state_t *)pdev->driver_data;
	if (state == NULL) {
		return;
	}
	if (state->irq_cookie != NULL) {
		bus_teardown_intr(state->nb_dev, state->irq_res,
		    state->irq_cookie);
		state->irq_cookie = NULL;
	}
	if (state->irq_res != NULL) {
		bus_release_resource(state->nb_dev, SYS_RES_IRQ,
		    state->irq_res->rid, state->irq_res);
		state->irq_res = NULL;
	}
	(void)xhci_halt(state);
	for (index = 0; index < XHCI_MAX_CONTROLLERS; index++) {
		if (xhci_states[index] == state) {
			xhci_states[index] = NULL;
		}
	}
	xhci_state_destroy(state);
}

static const pci_match_t xhci_matches[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, 0x0C, 0x03, 0x30 },
};

static pci_driver_t xhci_pci_driver = {
	.name = "xhci",
	.matches = xhci_matches,
	.match_count = 1,
	.probe = xhci_pci_probe,
	.remove = xhci_pci_remove,
};

static devclass_t xhci_devclass = {
	.name = "xhci",
	.maxunit = XHCI_MAX_CONTROLLERS,
};

PCI_DRIVER_MODULE(xhci, xhci_pci_driver, xhci_devclass,
    NEWBUS_PASS_STORAGE, NEWBUS_ORDER_EARLY);

int
xhci_pci_register(void)
{
	return (0);
}
