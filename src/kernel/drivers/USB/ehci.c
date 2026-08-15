/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the BSD
 * 2-Clause License are met. THIS SOFTWARE IS PROVIDED "AS IS".
 */

/* !DEFINES!

$define %type ehci_qtd_t as EHCI queue transfer descriptor
$define %type ehci_qh_t as EHCI queue head
$define %type ehci_request_t as deferred USB transfer
$define %type ehci_state_t as EHCI controller state
$define %func ehci_pci_register as function with args void

*/

/* !SPACE!

$space %internal ehci_wait, ehci_transfer, ehci_poll
$space %internal ehci_pci_probe, ehci_pci_remove
$space %export ehci_pci_register

*/

#include <kernel/drivers/USB/ehci.h>
#include <kernel/drivers/USB/usb.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/pci/pci.h>
#include <kernel/pci/utils/bar.h>
#include <mlibc/mlibc.h>

#define EHCI_MAX_CONTROLLERS	8
#define EHCI_MAX_REQUESTS	32
#define EHCI_DMA_MAX		0xffffffffULL

#define EHCI_CAPLENGTH		0x00
#define EHCI_HCSPARAMS		0x04
#define EHCI_HCCPARAMS		0x08
#define EHCI_USBCMD		0x00
#define EHCI_USBSTS		0x04
#define EHCI_USBINTR		0x08
#define EHCI_PERIODIC		0x14
#define EHCI_ASYNC		0x18
#define EHCI_CONFIGFLAG		0x40
#define EHCI_PORTSC		0x44
#define EHCI_CMD_RUN		0x00000001
#define EHCI_CMD_RESET		0x00000002
#define EHCI_CMD_PSE		0x00000010
#define EHCI_CMD_ASE		0x00000020
#define EHCI_STS_INT		0x00000001
#define EHCI_STS_ERR		0x00000002
#define EHCI_STS_PCD		0x00000004
#define EHCI_STS_HSE		0x00000010
#define EHCI_STS_HALT		0x00001000
#define EHCI_LINK_TERM		0x00000001
#define EHCI_LINK_QH		0x00000002
#define EHCI_QTD_ACTIVE		0x00000080
#define EHCI_QTD_HALTED		0x00000040
#define EHCI_QTD_DBE		0x00000020
#define EHCI_QTD_BABBLE		0x00000010
#define EHCI_QTD_XACT		0x00000008
#define EHCI_QTD_IOC		0x00008000
#define EHCI_PID_OUT		0
#define EHCI_PID_IN		1
#define EHCI_PID_SETUP		2
#define EHCI_PORT_CONNECT	0x00000001
#define EHCI_PORT_CSC		0x00000002
#define EHCI_PORT_ENABLE	0x00000004
#define EHCI_PORT_PEC		0x00000008
#define EHCI_PORT_RESET		0x00000100
#define EHCI_PORT_POWER		0x00001000
#define EHCI_PORT_OWNER		0x00002000

typedef struct {
	u32	next;
	u32	alt_next;
	u32	token;
	u32	buffer[5];
	u32	buffer_hi[5];
} __attribute__((packed, aligned(32))) ehci_qtd_t;

typedef struct {
	u32	hlink;
	u32	epchar;
	u32	epcap;
	u32	current;
	ehci_qtd_t overlay;
} __attribute__((packed, aligned(32))) ehci_qh_t;

typedef struct {
	usb_device_t	*dev;
	usb_endpoint_t	*ep;
	void		*data;
	usb_complete_t	complete;
	void		*arg;
	u32		length;
	u8		used;
	u8		periodic;
} ehci_request_t;

typedef struct {
	pci_device_t	*pci;
	device_t	nb_dev;
	volatile u8	*cap;
	volatile u8	*op;
	usb_dma_t	async_dma;
	usb_dma_t	periodic_dma;
	ehci_qh_t	*async_head;
	u32		*frame_list;
	ehci_request_t	requests[EHCI_MAX_REQUESTS];
	usb_controller_t usb;
	void		*irq_cookie;
	resource_t	*irq_res;
	u8		ports;
	u8		next_address;
	u8		busy;
} ehci_state_t;

static ehci_state_t *ehci_states[EHCI_MAX_CONTROLLERS];

static int ehci_transfer(ehci_state_t *, usb_device_t *, usb_endpoint_t *,
    const usb_setup_t *, void *, u32 *, u32, int);

static int
ehci_wait(volatile u32 *reg, u32 mask, u32 value, u32 limit)
{
	u32 i;

	for (i = 0; i < limit; i++) {
		if ((*reg & mask) == value)
			return (0);
		__asm__ volatile("pause");
	}
	return (-1);
}

static u32
ehci_qtd_token(u32 pid, u32 length, u32 toggle, int ioc)
{
	return ((length << 16) | (ioc ? EHCI_QTD_IOC : 0) |
	    (3U << 10) | (pid << 8) | EHCI_QTD_ACTIVE | (toggle << 31));
}

static void
ehci_qtd_buffer(ehci_qtd_t *qtd, u64 phys, u32 length)
{
	u32 i;

	qtd->buffer[0] = (u32)phys;
	qtd->buffer_hi[0] = phys >> 32;
	for (i = 1; i < 5 && length > i * 4096; i++) {
		qtd->buffer[i] = (u32)((phys & ~0xfffULL) + i * 4096);
		qtd->buffer_hi[i] = ((phys & ~0xfffULL) + i * 4096) >> 32;
	}
}

static int
ehci_transfer(ehci_state_t *st, usb_device_t *dev, usb_endpoint_t *ep,
    const usb_setup_t *setup, void *data, u32 *length, u32 timeout,
    int periodic)
{
	usb_dma_t dma, payload;
	ehci_qtd_t *td;
	ehci_qh_t *qh;
	u64 base;
	u32 count, i, pid, token, actual, command;
	u16 max_packet;
	u8 endpoint;

	memset(&dma, 0, sizeof(dma));
	memset(&payload, 0, sizeof(payload));
	if (*length > 5 * 4096 - 4095) {
		return (-1);
	}
	count = setup == NULL ? 1 : ((*length != 0) ? 3 : 2);
	if (usb_dma_alloc(&dma, sizeof(*qh) + count * sizeof(*td), 32,
	    EHCI_DMA_MAX) != 0)
		return (-1);
	if (usb_dma_alloc(&payload, *length + (setup != NULL ? 8 : 0), 4096,
	    EHCI_DMA_MAX) != 0) {
		usb_dma_free(&dma);
		return (-1);
	}
	if (*length != 0 && (setup == NULL ||
	    (setup->bmRequestType & USB_DIR_IN) == 0))
		memcpy((u8 *)payload.virt + (setup != NULL ? 8 : 0), data,
		    *length);
	qh = dma.virt;
	td = (ehci_qtd_t *)((u8 *)dma.virt + sizeof(*qh));
	base = dma.phys + sizeof(*qh);
	for (i = 0; i < count; i++) {
		td[i].next = (i + 1 < count) ?
		    (u32)(base + (i + 1) * sizeof(*td)) : EHCI_LINK_TERM;
		td[i].alt_next = EHCI_LINK_TERM;
	}
	if (setup != NULL) {
		memcpy(payload.virt, setup, sizeof(*setup));
		td[0].token = ehci_qtd_token(EHCI_PID_SETUP, 8, 0, 0);
		ehci_qtd_buffer(&td[0], payload.phys, 8);
		i = 1;
		if (*length != 0) {
			pid = (setup->bmRequestType & USB_DIR_IN) ? EHCI_PID_IN :
			    EHCI_PID_OUT;
			td[i].token = ehci_qtd_token(pid, *length, 1, 0);
			ehci_qtd_buffer(&td[i], payload.phys + 8, *length);
			i++;
		}
		pid = (*length != 0 &&
		    (setup->bmRequestType & USB_DIR_IN)) ? EHCI_PID_OUT :
		    EHCI_PID_IN;
		td[i].token = ehci_qtd_token(pid, 0, 1, 1);
	} else {
		pid = (ep->address & USB_DIR_IN) ? EHCI_PID_IN : EHCI_PID_OUT;
		td[0].token = ehci_qtd_token(pid, *length, 0, 1);
		ehci_qtd_buffer(&td[0], payload.phys, *length);
	}
	endpoint = ep == NULL ? 0 : ep->address & 0x0f;
	max_packet = ep == NULL ? dev->max_packet_size0 : ep->max_packet_size;
	qh->hlink = periodic ? EHCI_LINK_TERM :
	    ((u32)st->async_dma.phys | EHCI_LINK_QH);
	qh->epchar = dev->address | ((u32)endpoint << 8) |
	    ((u32)(dev->speed == USB_SPEED_HIGH ? 2 : dev->speed ==
	    USB_SPEED_LOW ? 1 : 0) << 12) | (1U << 14) |
	    ((u32)max_packet << 16);
	qh->epcap = periodic ? (1U << 0) : 0;
	qh->overlay.next = (u32)base;
	qh->overlay.alt_next = EHCI_LINK_TERM;
	if (periodic) {
		for (i = 0; i < 1024; i++)
			st->frame_list[i] = (u32)dma.phys | EHCI_LINK_QH;
		*(volatile u32 *)(st->op + EHCI_PERIODIC) =
		    (u32)st->periodic_dma.phys;
		command = *(volatile u32 *)(st->op + EHCI_USBCMD);
		*(volatile u32 *)(st->op + EHCI_USBCMD) = command | EHCI_CMD_PSE;
	} else {
		st->async_head->hlink = (u32)dma.phys | EHCI_LINK_QH;
	}
	__asm__ volatile("sfence" ::: "memory");
	for (i = 0; i < (timeout == 0 ? 1000000 : timeout * 1000); i++) {
		token = td[count - 1].token;
		if ((token & EHCI_QTD_ACTIVE) == 0)
			break;
		__asm__ volatile("pause");
	}
	if (periodic) {
		*(volatile u32 *)(st->op + EHCI_USBCMD) &= ~EHCI_CMD_PSE;
		for (i = 0; i < 1024; i++) st->frame_list[i] = EHCI_LINK_TERM;
	} else {
		st->async_head->hlink = (u32)st->async_dma.phys | EHCI_LINK_QH;
	}
	token = td[count - 1].token;
	actual = *length;
	if (setup == NULL)
		actual -= (td[0].token >> 16) & 0x7fff;
	if (*length != 0 && ((setup != NULL &&
	    (setup->bmRequestType & USB_DIR_IN)) || (setup == NULL &&
	    (ep->address & USB_DIR_IN))))
		memcpy(data, (u8 *)payload.virt + (setup != NULL ? 8 : 0),
		    actual);
	*length = actual;
	usb_dma_free(&payload);
	usb_dma_free(&dma);
	return ((token & (EHCI_QTD_ACTIVE | EHCI_QTD_HALTED | EHCI_QTD_DBE |
	    EHCI_QTD_BABBLE | EHCI_QTD_XACT)) == 0 ? 0 : -1);
}

static int
ehci_port_connected(void *arg, u8 port, u8 *speed)
{
	ehci_state_t *st;
	u32 value;

	st = arg;
	value = *(volatile u32 *)(st->op + EHCI_PORTSC + (port - 1) * 4);
	if ((value & EHCI_PORT_CONNECT) == 0 || (value & EHCI_PORT_OWNER))
		return (0);
	*speed = USB_SPEED_HIGH;
	return (1);
}

static int
ehci_port_reset(void *arg, u8 port)
{
	ehci_state_t *st;
	volatile u32 *reg;
	u32 value, i;

	st = arg;
	reg = (volatile u32 *)(st->op + EHCI_PORTSC + (port - 1) * 4);
	value = *reg & ~(EHCI_PORT_CSC | EHCI_PORT_PEC);
	*reg = value | EHCI_PORT_POWER | EHCI_PORT_RESET;
	for (i = 0; i < 100000; i++) __asm__ volatile("pause");
	*reg = (*reg & ~(EHCI_PORT_CSC | EHCI_PORT_PEC | EHCI_PORT_RESET)) |
	    EHCI_PORT_POWER;
	for (i = 0; i < 100000; i++) __asm__ volatile("pause");
	if ((*reg & EHCI_PORT_ENABLE) == 0) {
		*reg |= EHCI_PORT_OWNER;
		return (-1);
	}
	return (0);
}

static int
ehci_address(void *arg, usb_device_t *dev)
{
	ehci_state_t *st;
	usb_setup_t setup;
	u32 length;
	u8 address;

	st = arg;
	address = st->next_address++;
	if (address == 0 || address >= 127) address = st->next_address = 1;
	memset(&setup, 0, sizeof(setup));
	setup.bRequest = 5;
	setup.wValue = address;
	length = 0;
	dev->address = 0;
	if (ehci_transfer(st, dev, NULL, &setup, NULL, &length, 1000, 0) != 0)
		return (-1);
	dev->address = address;
	return (0);
}

static int ehci_noop(void *arg, usb_device_t *dev)
{ (void)arg; (void)dev; return (0); }
static int ehci_iface(void *a, usb_device_t *d, u8 i, u8 alt)
{ (void)a; (void)d; (void)i; (void)alt; return (0); }
static int ehci_control(void *a, usb_device_t *d, const usb_setup_t *s,
    void *b, u16 n, u32 t)
{ u32 l; l = n; return (ehci_transfer(a, d, NULL, s, b, &l, t, 0)); }
static int ehci_bulk(void *a, usb_device_t *d, usb_endpoint_t *e, void *b,
    u32 *n, u32 t)
{ return (ehci_transfer(a, d, e, NULL, b, n, t, 0)); }

static int
ehci_submit(void *arg, usb_device_t *dev, usb_endpoint_t *ep, void *data,
    u32 length, usb_complete_t complete, void *complete_arg)
{
	ehci_state_t *st;
	u32 i;

	st = arg;
	for (i = 0; i < EHCI_MAX_REQUESTS; i++) {
		if (st->requests[i].used) continue;
		st->requests[i].dev = dev; st->requests[i].ep = ep;
		st->requests[i].data = data; st->requests[i].length = length;
		st->requests[i].complete = complete;
		st->requests[i].arg = complete_arg;
		st->requests[i].periodic =
		    ((ep->attributes & 3) == USB_ENDPOINT_XFER_INT);
		st->requests[i].used = 1;
		return (0);
	}
	return (-1);
}

static void
ehci_poll(void *arg)
{
	ehci_state_t *st;
	ehci_request_t request;
	u32 status, port, length, i;
	int error;

	st = arg;
	if (__atomic_exchange_n(&st->busy, 1, __ATOMIC_ACQUIRE)) return;
	status = *(volatile u32 *)(st->op + EHCI_USBSTS);
	*(volatile u32 *)(st->op + EHCI_USBSTS) = status;
	if (status & EHCI_STS_PCD) {
		for (port = 1; port <= st->ports; port++)
			usb_controller_port_retry(&st->usb, port);
	}
	usb_controller_scan(&st->usb);
	for (i = 0; i < EHCI_MAX_REQUESTS; i++) {
		if (!st->requests[i].used) continue;
		request = st->requests[i]; st->requests[i].used = 0;
		length = request.length;
		error = ehci_transfer(st, request.dev, request.ep, NULL,
		    request.data, &length, 1000, request.periodic);
		request.complete(request.dev, request.data, length, error,
		    request.arg);
		break;
	}
	__atomic_store_n(&st->busy, 0, __ATOMIC_RELEASE);
}

static int
ehci_intr(void *arg)
{
	ehci_state_t	*st;
	u32		status;

	st = arg;
	status = *(volatile u32 *)(st->op + EHCI_USBSTS);
	if ((status & (EHCI_STS_INT | EHCI_STS_ERR | EHCI_STS_PCD |
	    EHCI_STS_HSE)) == 0) {
		return (-1);
	}
	ehci_poll(st);
	return (0);
}

static const usb_controller_ops_t ehci_ops = {
	.port_connected = ehci_port_connected, .port_reset = ehci_port_reset,
	.address_device = ehci_address, .update_ep0 = ehci_noop,
	.configure_device = ehci_noop, .configure_interface = ehci_iface,
	.remove_device = ehci_noop, .control = ehci_control, .bulk = ehci_bulk,
	.bulk_submit = ehci_submit, .interrupt = ehci_submit,
};

static int
ehci_pci_probe(pci_device_t *pdev, const pci_match_t *match)
{
	ehci_state_t *st;
	pci_bar_t bar;
	u32 caplen, hcc, eecp, value, i;
	int index, rid;

	(void)match;
	if (pci_read_bar(pdev, 0, &bar) != 0 || bar.is_io || bar.size < 0x100)
		return (-1);
	for (index = 0; index < EHCI_MAX_CONTROLLERS; index++)
		if (ehci_states[index] == NULL) break;
	if (index == EHCI_MAX_CONTROLLERS) return (-1);
	st = kmem_calloc(1, sizeof(*st));
	if (st == NULL) return (-1);
	pci_enable_memory_space(pdev); pci_enable_bus_mastering(pdev);
	st->pci = pdev; st->nb_dev = pdev->nb_device;
	st->cap = pmap_map_mmio(bar.base, bar.size);
	if (st->cap == NULL) goto fail;
	caplen = *(volatile u8 *)(st->cap + EHCI_CAPLENGTH);
	st->op = st->cap + caplen;
	st->ports = *(volatile u32 *)(st->cap + EHCI_HCSPARAMS) & 0xf;
	hcc = *(volatile u32 *)(st->cap + EHCI_HCCPARAMS);
	eecp = (hcc >> 8) & 0xff;
	if (eecp >= 0x40) {
		value = pci_cfg_read32(pdev->bus, pdev->slot, pdev->function, eecp);
		pci_cfg_write32(pdev->bus, pdev->slot, pdev->function, eecp,
		    value | (1U << 24));
		for (i = 0; i < 100000 && (pci_cfg_read32(pdev->bus, pdev->slot,
		    pdev->function, eecp) & (1U << 16)); i++) __asm__ volatile("pause");
	}
	*(volatile u32 *)(st->op + EHCI_USBCMD) = 0;
	if (ehci_wait((volatile u32 *)(st->op + EHCI_USBSTS), EHCI_STS_HALT,
	    EHCI_STS_HALT, 1000000) != 0) goto fail;
	*(volatile u32 *)(st->op + EHCI_USBCMD) = EHCI_CMD_RESET;
	if (ehci_wait((volatile u32 *)(st->op + EHCI_USBCMD), EHCI_CMD_RESET,
	    0, 1000000) != 0) goto fail;
	if (usb_dma_alloc(&st->async_dma, sizeof(ehci_qh_t), 32,
	    EHCI_DMA_MAX) != 0 || usb_dma_alloc(&st->periodic_dma, 4096, 4096,
	    EHCI_DMA_MAX) != 0) goto fail;
	st->async_head = st->async_dma.virt; st->frame_list = st->periodic_dma.virt;
	st->async_head->hlink = (u32)st->async_dma.phys | EHCI_LINK_QH;
	st->async_head->epchar = 1U << 15;
	st->async_head->overlay.next = EHCI_LINK_TERM;
	st->async_head->overlay.alt_next = EHCI_LINK_TERM;
	for (i = 0; i < 1024; i++) st->frame_list[i] = EHCI_LINK_TERM;
	*(volatile u32 *)(st->op + EHCI_ASYNC) = (u32)st->async_dma.phys;
	*(volatile u32 *)(st->op + EHCI_PERIODIC) = (u32)st->periodic_dma.phys;
	*(volatile u32 *)(st->op + EHCI_USBINTR) = EHCI_STS_INT | EHCI_STS_ERR |
	    EHCI_STS_PCD | EHCI_STS_HSE;
	*(volatile u32 *)(st->op + EHCI_CONFIGFLAG) = 1;
	*(volatile u32 *)(st->op + EHCI_USBCMD) = EHCI_CMD_RUN | EHCI_CMD_ASE;
	st->next_address = 1;
	st->usb.bus_device = device_add_child(st->nb_dev, "usb", -1);
	if (st->usb.bus_device == NULL || usb_controller_init(&st->usb, &ehci_ops,
	    st, st->usb.bus_device, st->ports) != 0) goto fail;
	rid = 0;
	st->irq_res = bus_alloc_resource_any(st->nb_dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (st->irq_res == NULL || bus_setup_intr(st->nb_dev, st->irq_res,
	    ehci_intr, st, &st->irq_cookie) != 0) goto fail;
	pdev->driver_data = st; ehci_states[index] = st;
	return (0);
fail:
	usb_dma_free(&st->periodic_dma); usb_dma_free(&st->async_dma);
	kmem_free(st); return (-1);
}

static void
ehci_pci_remove(pci_device_t *pdev)
{
	ehci_state_t *st;
	int i;

	st = pdev->driver_data; if (st == NULL) return;
	*(volatile u32 *)(st->op + EHCI_USBINTR) = 0;
	if (st->irq_cookie) bus_teardown_intr(st->nb_dev, st->irq_res,
	    st->irq_cookie);
	if (st->irq_res) bus_release_resource(st->nb_dev, SYS_RES_IRQ,
	    st->irq_res->rid, st->irq_res);
	usb_controller_fini(&st->usb);
	*(volatile u32 *)(st->op + EHCI_USBCMD) = 0;
	for (i = 0; i < EHCI_MAX_CONTROLLERS; i++)
		if (ehci_states[i] == st) ehci_states[i] = NULL;
	usb_dma_free(&st->periodic_dma); usb_dma_free(&st->async_dma);
	pdev->driver_data = NULL; kmem_free(st);
}

static const pci_match_t ehci_matches[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, 0x0c, 0x03, 0x20 },
};
static pci_driver_t ehci_driver = { .name = "ehci", .matches = ehci_matches,
	.match_count = 1, .probe = ehci_pci_probe, .remove = ehci_pci_remove };
static devclass_t ehci_devclass = { .name = "ehci", .maxunit = 8 };
PCI_DRIVER_MODULE(ehci, ehci_driver, ehci_devclass, NEWBUS_PASS_STORAGE,
    NEWBUS_ORDER_EARLY);
int
ehci_pci_register(void)
{
	return (0);
}
