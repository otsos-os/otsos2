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
$define %type int as 32 bit signed
$define %type netdev_t as struct with physical network device state
$define %type netdev_ops_t as struct with hardware driver callbacks
$define %type net_iface_t as struct with logical network interface state

$define %func loopback_ndev_transmit as function with args netdev_t *, const u8 *, u16
$define %func loopback_ndev_poll as function with args netdev_t *
$define %func loopback_ndev_is_link_up as function with args netdev_t *
$define %func loopback_init as function with args void

*/

/* !SPACE!

$space %internal loopback_ndev_transmit, loopback_ndev_poll
$space %internal loopback_ndev_is_link_up
$space %export loopback_init

*/

#include <kernel/net/loopback.h>
#include <kernel/net/net.h>
#include <kernel/net/netdev/netdev.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

#define	LOOPBACK_MTU		65535
#define	LOOPBACK_MAC		"\x02\x00\x00\x00\x00\x01"

static netdev_t	g_loopback_ndev;
static net_iface_t	g_loopback_iface;
static int	g_loopback_initialized;

static int
loopback_ndev_transmit(netdev_t *ndev, const u8 *frame, u16 len)
{
	u8	*buf;

	if (!ndev || !frame || len == 0) {
		return (-1);
	}

	buf = kmem_alloc(len);
	if (!buf) {
		ndev->tx_dropped++;
		return (-1);
	}

	memcpy(buf, frame, len);
	ndev->tx_submitted++;

	if (ndev->rx_handler) {
		ndev->rx_handler(ndev, buf, len);
		ndev->rx_completed++;
		ndev->rx_delivered++;
	}

	kmem_free(buf);
	ndev->tx_completed++;
	return (0);
}

static int
loopback_ndev_poll(netdev_t *ndev)
{
	(void)ndev;
	return (0);
}

static int
loopback_ndev_is_link_up(netdev_t *ndev)
{
	(void)ndev;
	return (1);
}

static netdev_ops_t loopback_ndev_ops = {
	.name		= "loopback",
	.transmit	= loopback_ndev_transmit,
	.poll		= loopback_ndev_poll,
	.is_link_up	= loopback_ndev_is_link_up,
};

int
loopback_init(void)
{
	u8	lo_mac[6] = LOOPBACK_MAC;

	if (g_loopback_initialized) {
		return (0);
	}

	memset(&g_loopback_ndev, 0, sizeof(g_loopback_ndev));
	strcpy(g_loopback_ndev.name, "lo");
	memcpy(g_loopback_ndev.mac, lo_mac, 6);
	g_loopback_ndev.mtu = LOOPBACK_MTU;
	g_loopback_ndev.flags = NETDEV_F_UP | NETDEV_F_LOOPBACK |
	    NETDEV_F_BROADCAST;
	g_loopback_ndev.priv = NULL;
	g_loopback_ndev.ops = &loopback_ndev_ops;

	if (netdev_register(&g_loopback_ndev) != 0) {
		drivers_log("[LOOPBACK] netdev registration failed\n");
		return (-1);
	}

	memset(&g_loopback_iface, 0, sizeof(g_loopback_iface));
	strcpy(g_loopback_iface.name, "lo");
	g_loopback_iface.flags = NET_IFF_UP | NET_IFF_LOOPBACK;
	g_loopback_iface.ip_addr = 0x7F000001;
	g_loopback_iface.netmask = 0xFF000000;

	if (net_iface_register(&g_loopback_iface, &g_loopback_ndev) != 0) {
		drivers_log("[LOOPBACK] iface registration failed\n");
		netdev_unregister(&g_loopback_ndev);
		return (-1);
	}

	g_loopback_initialized = 1;
	drivers_log("[LOOPBACK] loopback interface lo ready\n");
	return (0);
}
