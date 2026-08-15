/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type netdev_t as struct with physical network device state
$define %type netdev_ops_t as struct with hardware driver callbacks
$define %type net_iface_t as struct with logical network interface state
$define %type virtio_net_hdr_t as packed VirtIO network packet header
$define %type virtio_net_state_t as VirtIO network driver state
$define %type virtio_hw_t as shared VirtIO PCI transport state
$define %type virtio_vq_t as shared VirtIO split virtqueue state
$define %type pci_device_t as struct with PCI device info
$define %type pci_match_t as struct with PCI match criteria
$define %type pci_driver_t as struct with PCI driver

$define %func virtio_net_setup_queue as function with args virtio_vq_t *, virtio_hw_t *, u16
$define %func virtio_net_read_mac as function with args virtio_net_state_t *
$define %func virtio_net_read_link as procedure with args virtio_net_state_t *
$define %func virtio_net_post_rx as function with args virtio_net_state_t *, u16
$define %func virtio_net_reclaim_tx as procedure with args virtio_net_state_t *
$define %func virtio_net_destroy as procedure with args virtio_net_state_t *
$define %func virtio_net_ndev_transmit as function with args netdev_t *, const u8 *, u16
$define %func virtio_net_ndev_poll as function with args netdev_t *
$define %func virtio_net_ndev_is_link_up as function with args netdev_t *
$define %func virtio_net_pci_probe as function with args pci_device_t *, const pci_match_t *
$define %func virtio_net_pci_remove as procedure with args pci_device_t *
$define %func virtio_net_pci_register as function with args void

*/

/* !SPACE!

$space %internal virtio_net_setup_queue, virtio_net_read_mac, virtio_net_read_link
$space %internal virtio_net_post_rx, virtio_net_reclaim_tx
$space %internal virtio_net_destroy
$space %internal virtio_net_ndev_transmit, virtio_net_ndev_poll
$space %internal virtio_net_ndev_is_link_up
$space %internal virtio_net_pci_probe, virtio_net_pci_remove
$space %export virtio_net_pci_register

*/

#include <kernel/drivers/net/virtio-net/virtio_net.h>
#include <kernel/drivers/virtio/virtio_pci.h>
#include <kernel/drivers/virtio/virtio_queue.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/mm/kmem.h>
#include <kernel/pci/pci.h>
#include <kernel/net/net.h>
#include <kernel/net/netdev/netdev.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	VIRTIO_NET_DEVICE_ID		0x1041
#define	VIRTIO_NET_LEGACY_DEVICE_ID	0x1000
#define	VIRTIO_NET_QUEUE_SIZE		64
#define	VIRTIO_NET_RX_QUEUE		0
#define	VIRTIO_NET_TX_QUEUE		1
#define	VIRTIO_NET_F_MAC		(1u << 5)
#define	VIRTIO_NET_F_STATUS		(1u << 16)
#define	VIRTIO_NET_S_LINK_UP		(1u << 0)
#define	VIRTIO_NET_HDR_SIZE		12
#define	VIRTIO_NET_RX_BUFFER_SIZE	(VIRTIO_NET_HDR_SIZE + \
	VIRTIO_NET_ETH_FRAME_MAX)
#define	VIRTIO_NET_TX_BUFFER_SIZE	(VIRTIO_NET_HDR_SIZE + \
	VIRTIO_NET_ETH_FRAME_MAX)
#define	VIRTIO_NET_ETH_FRAME_MAX	1514
#define	VIRTIO_NET_RX_BUDGET		32
#define	VIRTIO_NET_MAX_STATES		8
#define	VIRTIO_ISR_QUEUE		1
#define	VIRTIO_ISR_CONFIG		2

typedef struct {
	u8	flags;
	u8	gso_type;
	u16	hdr_len;
	u16	gso_size;
	u16	csum_start;
	u16	csum_offset;
	u16	num_buffers;
} __attribute__((packed)) virtio_net_hdr_t;

typedef struct {
	virtio_hw_t		hw;
	virtio_vq_t		rx_vq;
	virtio_vq_t		tx_vq;
	u8			*rx_buffers;
	u8			*tx_buffers;
	u16			rx_slot[VIRTIO_NET_QUEUE_SIZE];
	u16			rx_posted;
	u8			ndev_registered;
	u8			iface_registered;
	u8			link_up;
	u8			rx_start_kick_pending;
	u8			irq_line;
	u8			irq_enabled;
	u8			polling;
	int			ready;
	device_t		nb_dev;
	resource_t		*irq_res;
	void			*irq_cookie;
	netdev_t		ndev;
	net_iface_t		iface;
} virtio_net_state_t;

static netdev_ops_t	virtio_net_ndev_ops;

static int	virtio_net_ndev_transmit(netdev_t *ndev,
		    const u8 *frame, u16 len);
static int	virtio_net_ndev_poll(netdev_t *ndev);
static int	virtio_net_ndev_is_link_up(netdev_t *ndev);

static int
virtio_net_setup_queue(virtio_vq_t *vq, virtio_hw_t *hw, u16 index)
{
	u16	qsize;

	virtio_hw_select_queue(hw, index);
	qsize = virtio_hw_get_queue_size(hw);
	if (qsize == 0) {
		return (-1);
	}

	if (qsize > VIRTIO_NET_QUEUE_SIZE) {
		qsize = VIRTIO_NET_QUEUE_SIZE;
	}
	if (virtio_vq_create(vq, qsize) != 0) {
		return (-1);
	}
	virtio_vq_bind(vq, hw, index);
	virtio_hw_set_queue_size(hw, qsize);
	virtio_hw_set_queue_desc(hw, vq->phys_desc);
	virtio_hw_set_queue_driver(hw, vq->phys_avail);
	virtio_hw_set_queue_device(hw, vq->phys_used);
	virtio_hw_enable_queue(hw);
	return (0);
}

static int
virtio_net_read_mac(virtio_net_state_t *st)
{
	u8	before, after;

	if ((st->hw.features & VIRTIO_NET_F_MAC) == 0) {
		return (-1);
	}

	do {
		before = virtio_hw_get_config_generation(&st->hw);
		if (virtio_hw_read_device_config(&st->hw, 0,
		    st->ndev.mac, 6) != 0) {
			return (-1);
		}
		after = virtio_hw_get_config_generation(&st->hw);
	} while (before != after);
	return (0);
}

static void
virtio_net_read_link(virtio_net_state_t *st)
{
	u16	status;

	if ((st->hw.features & VIRTIO_NET_F_STATUS) == 0) {
		st->link_up = 1;
		return;
	}
	if (virtio_hw_read_device_config(&st->hw, 6, &status,
	    sizeof(status)) != 0) {
		st->link_up = 0;
		return;
	}
	st->link_up = (status & VIRTIO_NET_S_LINK_UP) != 0;
}

static int
virtio_net_irq_state(void *arg)
{
	virtio_net_state_t	*st;
	u8			isr;

	st = (virtio_net_state_t *)arg;
	if (!st || !st->ready || !st->irq_enabled) {
		return (-1);
	}
	isr = virtio_hw_read_isr(&st->hw);
	if ((isr & (VIRTIO_ISR_QUEUE | VIRTIO_ISR_CONFIG)) == 0) {
		return (-1);
	}
	if (isr & VIRTIO_ISR_CONFIG) {
		virtio_net_read_link(st);
	}
	if (isr & VIRTIO_ISR_QUEUE) {
		virtio_net_ndev_poll(&st->ndev);
	}
	return (0);
}

static int
virtio_net_post_rx(virtio_net_state_t *st, u16 slot)
{
	virtio_vq_t	*vq;
	u16		head;

	vq = &st->rx_vq;
	if (slot >= vq->size) {
		return (-1);
	}
	head = virtio_vq_alloc_chain(vq, 1);
	if (head == (u16)-1) {
		return (-1);
	}
	virtio_vq_set_desc(vq, head, virtio_virt_to_phys(
	    st->rx_buffers + (u64)slot * VIRTIO_NET_RX_BUFFER_SIZE),
	    VIRTIO_NET_RX_BUFFER_SIZE, VIRTQ_DESC_F_WRITE, 0);
	st->rx_slot[head] = slot;
	virtio_vq_kick(vq, head);
	st->rx_posted++;
	return (0);
}

static void
virtio_net_reclaim_tx(virtio_net_state_t *st)
{
	u16	head;

	while ((head = virtio_vq_pop_used(&st->tx_vq, NULL)) !=
	    (u16)-1) {
		if (head >= st->tx_vq.size) {
			drivers_log("[VIRTIO_NET] invalid TX completion %u\n",
			    head);
			st->ndev.tx_dropped++;
			continue;
		}
		virtio_vq_free_chain(&st->tx_vq, head);
		st->ndev.tx_completed++;
	}
}

static void
virtio_net_destroy(virtio_net_state_t *st)
{
	if (!st) {
		return;
	}
	st->ready = 0;
	if (st->irq_cookie != NULL && st->nb_dev != NULL &&
	    st->irq_res != NULL) {
		bus_teardown_intr(st->nb_dev, st->irq_res,
		    st->irq_cookie);
		st->irq_cookie = NULL;
	}
	if (st->irq_res != NULL && st->nb_dev != NULL) {
		bus_release_resource(st->nb_dev, SYS_RES_IRQ,
		    st->irq_res->rid, st->irq_res);
		st->irq_res = NULL;
	}
	if (st->iface_registered) {
		net_iface_unregister(&st->iface);
	}
	if (st->ndev_registered) {
		netdev_unregister(&st->ndev);
	}
	virtio_pci_shutdown(&st->hw);
	virtio_vq_destroy(&st->rx_vq);
	virtio_vq_destroy(&st->tx_vq);
	if (st->rx_buffers) {
		kmem_free(st->rx_buffers);
	}
	if (st->tx_buffers) {
		kmem_free(st->tx_buffers);
	}
	kmem_free(st);
}

static int
virtio_net_ndev_transmit(netdev_t *ndev, const u8 *frame, u16 len)
{
	virtio_net_state_t	*st;
	virtio_net_hdr_t	*hdr;
	u16			head, slot;

	if (!ndev) {
		return (-1);
	}
	st = (virtio_net_state_t *)ndev->priv;
	if (!st || !st->ready || !frame || len < 60 ||
	    len > VIRTIO_NET_ETH_FRAME_MAX) {
		ndev->tx_dropped++;
		return (-1);
	}
	virtio_net_reclaim_tx(st);
	head = virtio_vq_alloc_chain(&st->tx_vq, 1);
	if (head == (u16)-1) {
		virtio_net_reclaim_tx(st);
		head = virtio_vq_alloc_chain(&st->tx_vq, 1);
	}
	if (head == (u16)-1) {
		ndev->tx_dropped++;
		return (-1);
	}
	slot = head;
	hdr = (virtio_net_hdr_t *)(st->tx_buffers +
	    (u64)slot * VIRTIO_NET_TX_BUFFER_SIZE);
	memset(hdr, 0, sizeof(*hdr));
	memcpy((u8 *)hdr + VIRTIO_NET_HDR_SIZE, frame, len);
	virtio_vq_set_desc(&st->tx_vq, head, virtio_virt_to_phys(hdr),
	    VIRTIO_NET_HDR_SIZE + len, 0, 0);
	virtio_vq_kick(&st->tx_vq, head);
	virtio_hw_notify_queue(&st->hw, VIRTIO_NET_TX_QUEUE);
	net_request_poll();
	ndev->tx_submitted++;
	return (0);
}

static int
virtio_net_ndev_poll(netdev_t *ndev)
{
	virtio_net_state_t	*st;
	virtio_net_hdr_t	*hdr;
	u32			used_len;
	u16			head, slot, frame_len;
	int			count, ret;

	if (!ndev) {
		return (-1);
	}
	st = (virtio_net_state_t *)ndev->priv;
	if (!st || !st->ready) {
		return (-1);
	}
	if (st->polling) {
		return (0);
	}

	st->polling = 1;
	count = 0;
	ret = 0;
	if (st->rx_start_kick_pending) {
		virtio_hw_notify_queue(&st->hw, VIRTIO_NET_RX_QUEUE);
		st->rx_start_kick_pending = 0;
	}
	while (count < VIRTIO_NET_RX_BUDGET &&
	    (head = virtio_vq_pop_used(&st->rx_vq, &used_len)) !=
	    (u16)-1) {
		if (head >= st->rx_vq.size || st->rx_posted == 0) {
			drivers_log("[VIRTIO_NET] invalid RX completion %u\n",
			    head);
			ndev->rx_dropped++;
			ret = -1;
			goto out;
		}
		ndev->rx_completed++;
		st->rx_posted--;
		slot = st->rx_slot[head];
		if (used_len >= VIRTIO_NET_HDR_SIZE &&
		    used_len <= VIRTIO_NET_RX_BUFFER_SIZE) {
			hdr = (virtio_net_hdr_t *)(st->rx_buffers +
			    (u64)slot * VIRTIO_NET_RX_BUFFER_SIZE);
			frame_len = (u16)(used_len - VIRTIO_NET_HDR_SIZE);
			if (ndev->rx_handler && frame_len >= 14) {
				ndev->rx_handler(ndev,
				    (u8 *)hdr + VIRTIO_NET_HDR_SIZE,
				    frame_len);
				ndev->rx_delivered++;
			} else {
				ndev->rx_dropped++;
			}
		} else {
			ndev->rx_dropped++;
		}
		virtio_vq_free_chain(&st->rx_vq, head);
		if (virtio_net_post_rx(st, slot) != 0) {
			ret = -1;
			goto out;
		}
		count++;
	}
	ret = count;

out:
	if (count > 0) {
		virtio_hw_notify_queue(&st->hw, VIRTIO_NET_RX_QUEUE);
	}
	if (count == VIRTIO_NET_RX_BUDGET) {
		net_request_poll();
	}
	virtio_net_reclaim_tx(st);
	st->polling = 0;
	return (ret);
}

static int
virtio_net_ndev_is_link_up(netdev_t *ndev)
{
	virtio_net_state_t	*st;

	st = (virtio_net_state_t *)ndev->priv;
	virtio_net_read_link(st);
	return (st->ready && st->link_up);
}

static int
virtio_net_pci_probe(pci_device_t *dev, const pci_match_t *match)
{
	virtio_net_state_t	*st;
	u64			buffer_size;
	int			rid;
	u16			i;

	(void)match;
	st = kmem_alloc(sizeof(*st));
	if (!st) {
		drivers_log("[VIRTIO_NET] state alloc failed\n");
		return (-1);
	}
	memset(st, 0, sizeof(*st));

	if (virtio_pci_init(&st->hw, dev, VIRTIO_NET_F_MAC |
	    VIRTIO_NET_F_STATUS) != 0 ||
	    virtio_net_setup_queue(&st->rx_vq, &st->hw,
	    VIRTIO_NET_RX_QUEUE) != 0 ||
	    virtio_net_setup_queue(&st->tx_vq, &st->hw,
	    VIRTIO_NET_TX_QUEUE) != 0) {
		drivers_log("[VIRTIO_NET] initialisation failed\n");
		virtio_net_destroy(st);
		return (-1);
	}

	buffer_size = (u64)VIRTIO_NET_QUEUE_SIZE *
	    VIRTIO_NET_RX_BUFFER_SIZE;
	st->rx_buffers = kmem_alloc_aligned(buffer_size, PAGE_SIZE);
	st->tx_buffers = kmem_alloc_aligned((u64)VIRTIO_NET_QUEUE_SIZE *
	    VIRTIO_NET_TX_BUFFER_SIZE, PAGE_SIZE);
	if (!st->rx_buffers || !st->tx_buffers) {
		drivers_log("[VIRTIO_NET] buffer allocation failed\n");
		virtio_net_destroy(st);
		return (-1);
	}
	memset(st->rx_buffers, 0, buffer_size);
	memset(st->tx_buffers, 0, (u64)VIRTIO_NET_QUEUE_SIZE *
	    VIRTIO_NET_TX_BUFFER_SIZE);

	for (i = 0; i < st->rx_vq.size; i++) {
		if (virtio_net_post_rx(st, i) != 0) {
			drivers_log("[VIRTIO_NET] RX queue fill "
			    "failed\n");
			virtio_net_destroy(st);
			return (-1);
		}
	}
	virtio_hw_set_status(&st->hw, VIRTIO_STATUS_ACKNOWLEDGE |
	    VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
	    VIRTIO_STATUS_DRIVER_OK);
	st->ready = 1;
	virtio_hw_notify_queue(&st->hw, VIRTIO_NET_RX_QUEUE);
	st->rx_start_kick_pending = 1;

	memset(&st->ndev, 0, sizeof(st->ndev));
	if (virtio_net_read_mac(st) == 0) {
		/* MAC already in st->ndev.mac from read_mac */
	} else {
		st->ndev.mac[0] = 0x02;
		st->ndev.mac[1] = 0x00;
		st->ndev.mac[2] = 0x00;
		st->ndev.mac[3] = 0x00;
		st->ndev.mac[4] = 0x00;
		st->ndev.mac[5] = (u8)(dev->slot << 3 | dev->function);
	}
	strcpy(st->ndev.name, "eth0");
	st->ndev.mtu = 1500;
	st->ndev.flags = NETDEV_F_UP | NETDEV_F_BROADCAST |
	    NETDEV_F_RUNNING;
	st->ndev.priv = st;
	st->ndev.ops = &virtio_net_ndev_ops;
	virtio_net_read_link(st);

	if (netdev_register(&st->ndev) != 0) {
		drivers_log("[VIRTIO_NET] netdev registration "
		    "failed\n");
		virtio_net_destroy(st);
		return (-1);
	}
	st->ndev_registered = 1;

	memset(&st->iface, 0, sizeof(st->iface));
	strcpy(st->iface.name, "virtio0");
	st->iface.flags = NET_IFF_UP;

	if (net_iface_register(&st->iface, &st->ndev) != 0) {
		drivers_log("[VIRTIO_NET] iface registration "
		    "failed\n");
		virtio_net_destroy(st);
		return (-1);
	}
	st->iface_registered = 1;
	dev->driver_data = st;
	st->nb_dev = dev->nb_device;

	st->irq_line = dev->irq_line;
	if (st->nb_dev != NULL && dev->irq_pin != 0 &&
	    dev->irq_line != 0xFF) {
		rid = 0;
		st->irq_res = bus_alloc_resource_any(st->nb_dev,
		    SYS_RES_IRQ, &rid, RF_ACTIVE);
		st->irq_enabled = 1;
		if (st->irq_res != NULL &&
		    bus_setup_intr(st->nb_dev, st->irq_res,
		    virtio_net_irq_state, st, &st->irq_cookie) == 0) {
			drivers_log("[VIRTIO_NET] INTx IRQ %u hooked\n",
			    st->irq_line);
		} else {
			st->irq_enabled = 0;
			drivers_log("[VIRTIO_NET] INTx IRQ setup "
			    "failed\n");
		}
	}

	drivers_log("[VIRTIO_NET] ready: "
	    "%02x:%02x:%02x:%02x:%02x:%02x on %s\n",
	    st->ndev.mac[0], st->ndev.mac[1],
	    st->ndev.mac[2], st->ndev.mac[3],
	    st->ndev.mac[4], st->ndev.mac[5],
	    st->ndev.name);
	return (0);
}

static void
virtio_net_pci_remove(pci_device_t *dev)
{
	virtio_net_state_t	*st;

	if (!dev) {
		return;
	}
	st = dev->driver_data;
	dev->driver_data = NULL;
	virtio_net_destroy(st);
}

static pci_match_t virtio_net_matches[] = {
	{
		.vendor_id	= VIRTIO_PCI_VENDOR_ID,
		.device_id	= VIRTIO_NET_DEVICE_ID,
		.class_code	= PCI_ANY_CLASS,
		.subclass	= PCI_ANY_SUBCLASS,
		.prog_if	= PCI_ANY_PROGIF,
	},
	{
		.vendor_id	= VIRTIO_PCI_VENDOR_ID,
		.device_id	= VIRTIO_NET_LEGACY_DEVICE_ID,
		.class_code	= PCI_ANY_CLASS,
		.subclass	= PCI_ANY_SUBCLASS,
		.prog_if	= PCI_ANY_PROGIF,
	},
};

static pci_driver_t virtio_net_pci_driver = {
	.name		= "virtio-net",
	.matches	= virtio_net_matches,
	.match_count	= 2,
	.probe		= virtio_net_pci_probe,
	.remove		= virtio_net_pci_remove,
};

static devclass_t virtio_net_devclass = {
	.name		= "virtio-net",
	.maxunit	= VIRTIO_NET_MAX_STATES,
};

PCI_DRIVER_MODULE(virtio_net, virtio_net_pci_driver, virtio_net_devclass,
    NEWBUS_PASS_NETWORK, NEWBUS_ORDER_MIDDLE);

static netdev_ops_t virtio_net_ndev_ops = {
	.name		= "virtio-net",
	.transmit	= virtio_net_ndev_transmit,
	.poll		= virtio_net_ndev_poll,
	.is_link_up	= virtio_net_ndev_is_link_up,
};

int
virtio_net_pci_register(void)
{
	drivers_log("[VIRTIO_NET] pci_register entry is deprecated; "
	    "using PCI_DRIVER_MODULE\n");
	return (0);
}
