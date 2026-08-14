/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the BSD
 * 2-Clause License are met. THIS SOFTWARE IS PROVIDED "AS IS".
 */

/* !DEFINES!

$define %type ohci_hcca_t as OHCI host controller communications area
$define %type ohci_ed_t as OHCI endpoint descriptor
$define %type ohci_td_t as OHCI transfer descriptor
$define %type ohci_state_t as OHCI controller state
$define %func ohci_pci_register as function with args void

*/

/* !SPACE!

$space %internal ohci_wait, ohci_transfer, ohci_poll
$space %internal ohci_pci_probe, ohci_pci_remove
$space %export ohci_pci_register

*/

#include <kernel/drivers/USB/ohci.h>
#include <kernel/drivers/USB/usb.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/pci/pci.h>
#include <kernel/pci/utils/bar.h>
#include <mlibc/mlibc.h>

#define OHCI_MAX	8
#define OHCI_MAX_REQ	32
#define OHCI_DMA_MAX	0xffffffffULL
#define OHCI_REV	0x00
#define OHCI_CONTROL	0x04
#define OHCI_CMD	0x08
#define OHCI_INT_STATUS	0x0c
#define OHCI_INT_ENABLE	0x10
#define OHCI_HCCA	0x18
#define OHCI_PERIODIC	0x14
#define OHCI_CTRL_HEAD	0x20
#define OHCI_CTRL_CURRENT	0x24
#define OHCI_BULK_HEAD	0x28
#define OHCI_BULK_CURRENT	0x2c
#define OHCI_DONE_HEAD	0x30
#define OHCI_FM_INTERVAL	0x34
#define OHCI_PERIODIC_START	0x40
#define OHCI_LS_THRESHOLD	0x44
#define OHCI_RH_DESC_A	0x48
#define OHCI_RH_STATUS	0x50
#define OHCI_RH_PORT	0x54
#define OHCI_CTRL_IR	0x00000080
#define OHCI_CTRL_OPERATIONAL	0x00000080
#define OHCI_CTRL_CLE	0x00000010
#define OHCI_CTRL_BLE	0x00000020
#define OHCI_CTRL_PLE	0x00000004
#define OHCI_CMD_RESET	0x00000001
#define OHCI_CMD_RESUME	0x00000002
#define OHCI_CMD_CLF	0x00000002
#define OHCI_CMD_BLF	0x00000004
#define OHCI_INT_WDH	0x00000002
#define OHCI_INT_SO	0x00000001
#define OHCI_INT_SF	0x00000004
#define OHCI_INT_UE	0x00000010
#define OHCI_INT_RHSC	0x00000040
#define OHCI_INT_MIE	0x80000000
#define OHCI_FI_DEFAULT	0x2edf
#define OHCI_PORT_CSC	0x00010000
#define OHCI_PORT_PESC	0x00020000
#define OHCI_PORT_OCIC	0x00040000
#define OHCI_PORT_PRSC	0x00100000
#define OHCI_PORT_CHANGE	(OHCI_PORT_CSC | OHCI_PORT_PESC | \
	OHCI_PORT_OCIC | OHCI_PORT_PRSC)
#define OHCI_ED_SKIP	0x00004000
#define OHCI_ED_DIR_OUT	0x00000800
#define OHCI_ED_DIR_IN	0x00001000
#define OHCI_ED_DIR_TD	0x00001800
#define OHCI_ED_SPEED	0x00002000
#define OHCI_TD_R	0x00040000
#define OHCI_TD_DP_SETUP	0x00000000
#define OHCI_TD_DP_IN	0x00100000
#define OHCI_TD_DP_OUT	0x00080000
#define OHCI_TD_T_CARRY	0x00000000
#define OHCI_TD_T_DATA0	0x01000000
#define OHCI_TD_T_DATA1	0x02000000
#define OHCI_TD_DI	0xe0000000
#define OHCI_TD_CC_MASK	0xf0000000
#define OHCI_CC_NOERROR	0
#define OHCI_CC_NOTACCESSED	0xf
#define OHCI_CC_STALL	4

typedef struct {
	u32 interrupt[32];
	u16 frame;
	u16 pad;
	u32 done;
} __attribute__((packed, aligned(256))) ohci_hcca_t;

typedef struct {
	u32 info;
	u32 tail;
	u32 head;
	u32 next;
} __attribute__((packed, aligned(16))) ohci_ed_t;

typedef struct {
	u32 info;
	u32 cbp;
	u32 next;
	u32 be;
} __attribute__((packed, aligned(16))) ohci_td_t;

typedef struct {
	usb_device_t *dev;
	usb_endpoint_t *ep;
	void *data;
	usb_dma_t desc_dma;
	usb_dma_t payload_dma;
	ohci_ed_t *ed;
	ohci_td_t *td;
	u32 length;
	usb_complete_t complete;
	void *arg;
	u8 used;
	u8 periodic;
} ohci_request_t;

typedef struct {
	pci_device_t *pci;
	device_t nb_dev;
	volatile u8 *regs;
	usb_dma_t hcca_dma;
	usb_dma_t list_dma;
	ohci_hcca_t *hcca;
	ohci_ed_t *control_ed;
	ohci_ed_t *bulk_ed;
	ohci_request_t requests[OHCI_MAX_REQ];
	usb_controller_t usb;
	void *poll_cookie;
	u8 ports;
	u8 next_address;
	u8 busy;
	u8 error_logs;
} ohci_state_t;

static ohci_state_t *ohci_states[OHCI_MAX];

static void
ohci_periodic_rebuild(ohci_state_t *st)
{
	ohci_request_t *request;
	u32 head, i;

	head = 0;
	for (i = OHCI_MAX_REQ; i != 0; i--) {
		request = &st->requests[i - 1];
		if (!request->used || request->ed == NULL)
			continue;
		request->ed->next = head;
		head = (u32)request->desc_dma.phys;
	}
	for (i = 0; i < 32; i++)
		st->hcca->interrupt[i] = head;
	__asm__ volatile("sfence" ::: "memory");
}

static int
ohci_wait(volatile u32 *reg, u32 mask, u32 value, u32 limit)
{
	u32 i;

	for (i = 0; i < limit; i++) {
		if ((*reg & mask) == value) return (0);
		__asm__ volatile("pause");
	}
	return (-1);
}

static int
ohci_transfer(ohci_state_t *st, usb_device_t *dev, usb_endpoint_t *ep,
    const usb_setup_t *setup, void *data, u32 *length, u32 timeout)
{
	usb_dma_t dma, payload;
	ohci_ed_t *ed;
	ohci_td_t *td;
	u64 td_phys, payload_phys;
	u32 info, i, count, cc, actual;
	u8 *payload_ptr;

	memset(&dma, 0, sizeof(dma));
	memset(&payload, 0, sizeof(payload));
	if (*length > 8192) {
		return (-1);
	}
	count = setup != NULL ? ((*length != 0) ? 3 : 2) : 1;
	if (usb_dma_alloc(&dma, sizeof(*ed) + (count + 1) * sizeof(*td), 16,
	    OHCI_DMA_MAX) != 0) return (-1);
	if ((*length != 0 || setup != NULL) && usb_dma_alloc(&payload,
	    *length + (setup != NULL ? 8 : 0), 4096,
	    OHCI_DMA_MAX) != 0) goto fail;
	ed = dma.virt;
	td = (ohci_td_t *)((u8 *)dma.virt + sizeof(*ed));
	td_phys = dma.phys + sizeof(*ed);
	payload_ptr = payload.virt;
	payload_phys = payload.phys;
	if (*length != 0 && (setup == NULL ||
	    (setup->bmRequestType & USB_DIR_IN) == 0))
		memcpy(payload_ptr + (setup != NULL ? 8 : 0), data, *length);
	if (setup != NULL) {
		memcpy(payload_ptr, setup, sizeof(*setup));
		info = OHCI_TD_DP_SETUP;
		td[0].info = info | OHCI_TD_T_DATA0 |
		    (OHCI_CC_NOTACCESSED << 28);
		td[0].cbp = (u32)payload_phys;
		td[0].be = (u32)(payload_phys + 7);
		td[0].next = count > 1 ? td_phys + sizeof(*td) : td_phys +
		    2 * sizeof(*td);
		if (*length != 0) {
			info = (setup->bmRequestType & USB_DIR_IN) ? OHCI_TD_DP_IN :
			    OHCI_TD_DP_OUT;
			td[1].info = info | OHCI_TD_T_DATA1 |
			    (OHCI_CC_NOTACCESSED << 28);
			td[1].cbp = (u32)(payload_phys + 8);
			td[1].be = (u32)(payload_phys + 8 + *length - 1);
			td[1].next = td_phys + 2 * sizeof(*td);
		}
		info = (setup->bmRequestType & USB_DIR_IN) ? OHCI_TD_DP_OUT :
		    OHCI_TD_DP_IN;
		td[count - 1].info = info | OHCI_TD_T_DATA1 |
		    (OHCI_CC_NOTACCESSED << 28);
		td[count - 1].cbp = 0; td[count - 1].be = 0;
	} else {
		info = (ep->address & USB_DIR_IN) ? OHCI_TD_DP_IN : OHCI_TD_DP_OUT;
		td[0].info = info | (ep->toggle ? OHCI_TD_T_DATA1 :
		    OHCI_TD_T_DATA0) |
		    (OHCI_CC_NOTACCESSED << 28);
		td[0].cbp = (u32)payload_phys;
		td[0].be = (u32)(payload_phys + *length - 1);
		td[0].next = 0;
	}
	for (i = 0; i < count; i++)
		td[i].next = (u32)(td_phys + (i + 1) * sizeof(*td));
	ed->head = td_phys; ed->tail = td_phys + count * sizeof(*td);
	ed->next = 0;
	ed->info = dev->address | ((u32)(ep == NULL ? 0 : ep->address & 0xf) << 7) |
	    ((u32)(ep == NULL ? dev->max_packet_size0 : ep->max_packet_size) << 16);
	if (ep != NULL && (ep->address & USB_DIR_IN)) ed->info |= OHCI_ED_DIR_IN;
	if (dev->speed == USB_SPEED_LOW) ed->info |= OHCI_ED_SPEED;
	if (setup != NULL) {
		*(volatile u32 *)(st->regs + OHCI_CONTROL) &= ~OHCI_CTRL_CLE;
		(void)ohci_wait((volatile u32 *)(st->regs + OHCI_CTRL_CURRENT),
		    0xffffffffU, 0, 100000);
		*(volatile u32 *)(st->regs + OHCI_CTRL_HEAD) = (u32)dma.phys;
		*(volatile u32 *)(st->regs + OHCI_CONTROL) |= OHCI_CTRL_CLE;
		*(volatile u32 *)(st->regs + OHCI_CMD) = OHCI_CMD_CLF;
	} else if ((ep->attributes & 3) == USB_ENDPOINT_XFER_INT) {
		for (i = 0; i < 32; i++)
			st->hcca->interrupt[i] = (u32)dma.phys;
		/* Keep the periodic schedule enabled while this ED is active. */
	} else {
		*(volatile u32 *)(st->regs + OHCI_CONTROL) &= ~OHCI_CTRL_BLE;
		(void)ohci_wait((volatile u32 *)(st->regs + OHCI_BULK_CURRENT),
		    0xffffffffU, 0, 100000);
		*(volatile u32 *)(st->regs + OHCI_BULK_HEAD) = (u32)dma.phys;
		*(volatile u32 *)(st->regs + OHCI_CONTROL) |= OHCI_CTRL_BLE;
		*(volatile u32 *)(st->regs + OHCI_CMD) = OHCI_CMD_BLF;
	}
	__asm__ volatile("sfence" ::: "memory");
	for (i = 0; i < (timeout == 0 ? 1000000 : timeout * 1000); i++) {
		if ((td[count - 1].info >> 28) != OHCI_CC_NOTACCESSED) break;
		__asm__ volatile("pause");
	}
	cc = OHCI_CC_NOERROR;
	for (i = 0; i < count; i++) {
		if ((td[i].info >> 28) != OHCI_CC_NOERROR) {
			cc = td[i].info >> 28;
			break;
		}
	}
	if (ep != NULL && (ep->attributes & 3) == USB_ENDPOINT_XFER_INT) {
		for (i = 0; i < 32; i++)
			st->hcca->interrupt[i] = 0;
	}
	if (ep != NULL && cc == OHCI_CC_NOERROR &&
	    (ep->attributes & 3) != USB_ENDPOINT_XFER_CONTROL)
		ep->toggle ^= 1;
	actual = *length;
	if (*length != 0 && ((setup != NULL &&
	    (setup->bmRequestType & USB_DIR_IN)) || (setup == NULL &&
	    (ep->address & USB_DIR_IN))))
		memcpy(data, payload_ptr + (setup != NULL ? 8 : 0), actual);
	*length = actual;
	if (cc != OHCI_CC_NOERROR && st->error_logs < 16) {
		st->error_logs++;
		usb_log_printf("ohci: transfer failed cc=%u ep=%u addr=%u "
		    "head=%x tail=%x td0=%x last=%x ctl=%x cur=%x\n", cc,
		    ep == NULL ? 0 : ep->address & 0x0f, dev->address,
		    ed->head, ed->tail, td[0].info, td[count - 1].info,
		    *(volatile u32 *)(st->regs + OHCI_CONTROL),
		    *(volatile u32 *)(st->regs + (setup != NULL ?
		    OHCI_CTRL_CURRENT : OHCI_BULK_CURRENT)));
	}
	usb_dma_free(&payload); usb_dma_free(&dma);
	return (cc == OHCI_CC_NOERROR ? 0 : -1);
fail:
	usb_dma_free(&payload); usb_dma_free(&dma); return (-1);
}

static int ohci_port_connected(void *arg, u8 port, u8 *speed)
{
	ohci_state_t *st; u32 value;
	st = arg; value = *(volatile u32 *)(st->regs + OHCI_RH_PORT + (port - 1) * 4);
	if ((value & 1) == 0) {
		return (0);
	}
	*speed = (value & (1U << 9)) ? USB_SPEED_LOW : USB_SPEED_FULL;
	return (1);
}

static int ohci_port_reset(void *arg, u8 port)
{
	ohci_state_t *st; volatile u32 *reg; u32 i;
	st = arg; reg = (volatile u32 *)(st->regs + OHCI_RH_PORT + (port - 1) * 4);
	usb_log_printf("ohci: port %u reset before=%x\n", port, *reg);
	*reg |= 1U << 4;
	for (i = 0; i < 100000; i++) __asm__ volatile("pause");
	*reg = (1U << 1) | (1U << 4);
	for (i = 0; i < 100000; i++) __asm__ volatile("pause");
	usb_log_printf("ohci: port %u reset after=%x\n", port, *reg);
	return ((*reg & (1U << 1)) != 0 ? 0 : -1);
}

static int ohci_address(void *arg, usb_device_t *dev)
{
	ohci_state_t *st; usb_setup_t setup; u32 length; u8 address;
	st = arg; address = st->next_address++; if (address >= 127) address = 1;
	memset(&setup, 0, sizeof(setup)); setup.bRequest = 5; setup.wValue = address;
	length = 0; if (ohci_transfer(st, dev, NULL, &setup, NULL, &length,
	    1000) != 0)
		return (-1);
	for (length = 0; length < 200000; length++)
		__asm__ volatile("pause");
	dev->address = address; return (0);
}
static int ohci_noop(void *a, usb_device_t *d) { (void)a; (void)d; return (0); }
static int ohci_iface(void *a, usb_device_t *d, u8 i, u8 x)
{ (void)a; (void)d; (void)i; (void)x; return (0); }
static int ohci_control(void *a, usb_device_t *d, const usb_setup_t *s, void *b,
    u16 n, u32 t) { u32 l = n; return (ohci_transfer(a, d, NULL, s, b, &l, t)); }
static int ohci_bulk(void *a, usb_device_t *d, usb_endpoint_t *e, void *b,
    u32 *n, u32 t) { return (ohci_transfer(a, d, e, NULL, b, n, t)); }
static int ohci_submit(void *arg, usb_device_t *d, usb_endpoint_t *e, void *b,
    u32 n, usb_complete_t c, void *x)
{
	ohci_request_t *request;
	ohci_state_t *st;
	u32 info, i;

	st = arg;
	for (i = 0; i < OHCI_MAX_REQ; i++) {
		if (st->requests[i].used && st->requests[i].dev == d &&
		    st->requests[i].ep == e)
			return (-1);
	}
	for (i = 0; i < OHCI_MAX_REQ; i++) if (!st->requests[i].used) {
		request = &st->requests[i];
		memset(request, 0, sizeof(*request));
		if (usb_dma_alloc(&request->desc_dma,
		    sizeof(ohci_ed_t) + 2 * sizeof(ohci_td_t), 16,
		    OHCI_DMA_MAX) != 0 || usb_dma_alloc(&request->payload_dma,
		    n, 4096, OHCI_DMA_MAX) != 0) {
			usb_dma_free(&request->payload_dma);
			usb_dma_free(&request->desc_dma);
			return (-1);
		}
		request->ed = request->desc_dma.virt;
		request->td = (ohci_td_t *)((u8 *)request->desc_dma.virt +
		    sizeof(ohci_ed_t));
		info = (e->address & USB_DIR_IN) ? OHCI_TD_DP_IN :
		    OHCI_TD_DP_OUT;
		request->td->info = info | (e->toggle ? OHCI_TD_T_DATA1 :
		    OHCI_TD_T_DATA0) | (OHCI_CC_NOTACCESSED << 28);
		request->td->cbp = (u32)request->payload_dma.phys;
		request->td->be = (u32)(request->payload_dma.phys + n - 1);
		request->td->next = (u32)(request->desc_dma.phys +
		    sizeof(ohci_ed_t) + sizeof(ohci_td_t));
		request->ed->info = d->address | ((u32)(e->address & 0x0f) << 7) |
		    ((u32)e->max_packet_size << 16) |
		    ((e->address & USB_DIR_IN) ? OHCI_ED_DIR_IN :
		    OHCI_ED_DIR_OUT);
		if (d->speed == USB_SPEED_LOW)
			request->ed->info |= OHCI_ED_SPEED;
		request->ed->head = request->td->next - sizeof(ohci_td_t);
		request->ed->tail = request->td->next;
		request->dev=d; request->ep=e; request->data=b;
		request->length=n; request->complete=c;
		request->arg=x; request->periodic=1; request->used=1;
		ohci_periodic_rebuild(st);
		return (0);
	}
	return (-1);
}
static void ohci_poll(void *arg)
{
	ohci_state_t *st = arg; u32 status, value, i;
	ohci_request_t completed;
	ohci_request_t *request;
	u32 cc;
	if (__atomic_exchange_n(&st->busy, 1, __ATOMIC_ACQUIRE)) return;
	status = *(volatile u32 *)(st->regs + OHCI_INT_STATUS);
	*(volatile u32 *)(st->regs + OHCI_INT_STATUS) = status;
	if (status & OHCI_INT_UE)
		usb_log_printf("ohci: poll status=%x rh=%x\n", status,
		    *(volatile u32 *)(st->regs + OHCI_RH_STATUS));
	if (status & OHCI_INT_RHSC) {
		for (i = 1; i <= st->ports; i++) {
			value = *(volatile u32 *)(st->regs + OHCI_RH_PORT +
			    (i - 1) * 4);
			if ((value & OHCI_PORT_CHANGE) == 0)
				continue;
			if (value & OHCI_PORT_CSC)
				usb_controller_port_retry(&st->usb, i);
			*(volatile u32 *)(st->regs + OHCI_RH_PORT +
			    (i - 1) * 4) = value & OHCI_PORT_CHANGE;
		}
	}
	usb_controller_scan(&st->usb);
	for (i = 0; i < OHCI_MAX_REQ; i++) {
		request = &st->requests[i];
		if (!request->used || request->td == NULL)
			continue;
		cc = request->td->info >> 28;
		if (cc == OHCI_CC_NOTACCESSED)
			continue;
		completed = *request;
		memset(request, 0, sizeof(*request));
		ohci_periodic_rebuild(st);
		if (cc == OHCI_CC_NOERROR &&
		    (completed.ep->address & USB_DIR_IN))
			memcpy(completed.data, completed.payload_dma.virt,
			    completed.length);
		if (cc == OHCI_CC_NOERROR)
			completed.ep->toggle ^= 1;
		usb_dma_free(&completed.payload_dma);
		usb_dma_free(&completed.desc_dma);
		completed.complete(completed.dev, completed.data,
		    cc == OHCI_CC_NOERROR ? completed.length : 0,
		    cc == OHCI_CC_NOERROR ? 0 : -1, completed.arg);
		break;
	}
	__atomic_store_n(&st->busy, 0, __ATOMIC_RELEASE);
}
static const usb_controller_ops_t ohci_ops = {
	.port_connected=ohci_port_connected, .port_reset=ohci_port_reset,
	.address_device=ohci_address, .update_ep0=ohci_noop, .configure_device=ohci_noop,
	.configure_interface=ohci_iface, .remove_device=ohci_noop, .control=ohci_control,
	.bulk=ohci_bulk, .bulk_submit=ohci_submit, .interrupt=ohci_submit,
};

static int ohci_pci_probe(pci_device_t *pdev, const pci_match_t *match)
{
	ohci_state_t *st; pci_bar_t bar; u32 desc, i; int slot;
	(void)match; if (pci_read_bar(pdev, 0, &bar) != 0 || bar.is_io || bar.size < 0x100)
		return (-1);
	for (slot=0; slot<OHCI_MAX && ohci_states[slot]!=NULL; slot++);
	if (slot == OHCI_MAX || (st=kmem_calloc(1,sizeof(*st))) == NULL) return (-1);
	pci_enable_memory_space(pdev); pci_enable_bus_mastering(pdev); st->pci=pdev;
	st->nb_dev=pdev->nb_device; st->regs=pmap_map_mmio(bar.base,bar.size);
	if (st->regs == NULL) goto fail;
	st->ports = *(volatile u32 *)(st->regs + OHCI_RH_DESC_A) & 0xff;
	if (st->ports == 0 || st->ports > USB_MAX_PORTS) goto fail;
	usb_log_printf("ohci: revision=%x ports=%u rhdesc=%x\n",
	    *(volatile u32 *)(st->regs + OHCI_REV), st->ports,
	    *(volatile u32 *)(st->regs + OHCI_RH_DESC_A));
	*(volatile u32 *)(st->regs + OHCI_CMD) = OHCI_CMD_RESET;
	if (ohci_wait((volatile u32 *)(st->regs + OHCI_CMD), OHCI_CMD_RESET, 0,
	    1000000) != 0) goto fail;
	if (usb_dma_alloc(&st->hcca_dma, 256, 256, OHCI_DMA_MAX) != 0 ||
	    usb_dma_alloc(&st->list_dma, 64, 16, OHCI_DMA_MAX) != 0) goto fail;
	st->hcca=st->hcca_dma.virt; st->control_ed=st->list_dma.virt;
	st->bulk_ed=(ohci_ed_t *)((u8 *)st->list_dma.virt+sizeof(ohci_ed_t));
	for (i=0;i<32;i++) st->hcca->interrupt[i]=0;
	*(volatile u32 *)(st->regs + OHCI_HCCA)=(u32)st->hcca_dma.phys;
	*(volatile u32 *)(st->regs + OHCI_FM_INTERVAL) = OHCI_FI_DEFAULT |
	    (6U << 16);
	*(volatile u32 *)(st->regs + OHCI_PERIODIC_START) =
	    (OHCI_FI_DEFAULT * 9) / 10;
	*(volatile u32 *)(st->regs + OHCI_CONTROL)=OHCI_CTRL_OPERATIONAL|OHCI_CTRL_PLE|
	    OHCI_CTRL_CLE|OHCI_CTRL_BLE;
	*(volatile u32 *)(st->regs + OHCI_INT_ENABLE)=OHCI_INT_WDH|OHCI_INT_UE|
	    OHCI_INT_RHSC|OHCI_INT_MIE;
	st->next_address=1; st->usb.bus_device=device_add_child(st->nb_dev,"usb",-1);
	if (st->usb.bus_device==NULL || usb_controller_init(&st->usb,&ohci_ops,st,
	    st->usb.bus_device,st->ports)!=0 || bus_setup_poll(st->nb_dev,NB_POLL_TIMER,
	    ohci_poll,st,&st->poll_cookie)!=0) goto fail;
	pdev->driver_data=st; ohci_states[slot]=st; return (0);
fail: usb_dma_free(&st->list_dma); usb_dma_free(&st->hcca_dma); kmem_free(st); return (-1);
}
static void ohci_pci_remove(pci_device_t *pdev)
{
	ohci_state_t *st=pdev->driver_data; int i;
	if (st==NULL) return; if (st->poll_cookie) bus_teardown_poll(st->nb_dev,st->poll_cookie);
	usb_controller_fini(&st->usb); *(volatile u32 *)(st->regs+OHCI_CONTROL)=0;
	for(i=0;i<OHCI_MAX;i++) if(ohci_states[i]==st) ohci_states[i]=NULL;
	usb_dma_free(&st->list_dma); usb_dma_free(&st->hcca_dma); pdev->driver_data=NULL; kmem_free(st);
}
static const pci_match_t ohci_matches[]={{PCI_ANY_ID,PCI_ANY_ID,0x0c,0x03,0x10}};
static pci_driver_t ohci_driver={.name="ohci",.matches=ohci_matches,.match_count=1,
	.probe=ohci_pci_probe,.remove=ohci_pci_remove};
static devclass_t ohci_devclass={.name="ohci",.maxunit=OHCI_MAX};
PCI_DRIVER_MODULE(ohci,ohci_driver,ohci_devclass,NEWBUS_PASS_STORAGE,NEWBUS_ORDER_EARLY);
int
ohci_pci_register(void)
{
	return (0);
}
