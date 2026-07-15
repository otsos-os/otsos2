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
$define %type int as 32 bit signed
$define %type net_iface_t as struct with logical network interface state
$define %type netdev_t as struct with physical network device state

$define %func net_init as function with args void
$define %func net_is_initialized as function with args void
$define %func net_iface_register as function with args net_iface_t *, netdev_t *
$define %func net_iface_unregister as procedure with args net_iface_t *
$define %func net_iface_count as function with args void
$define %func net_iface_get as function with args int
$define %func net_iface_by_name as function with args const char *
$define %func net_iface_find_by_ndev as function with args netdev_t *
$define %func net_poll_all as procedure with args void
$define %func net_dump_ifaces as procedure with args void

*/

/* !SPACE!

$space %export net_init, net_is_initialized
$space %export net_iface_register, net_iface_unregister, net_iface_count
$space %export net_iface_get, net_iface_by_name
$space %export net_iface_find_by_ndev
$space %export net_poll_all, net_dump_ifaces

*/

#include <kernel/net/net.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/arp.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static net_iface_t	*g_ifaces[NET_MAX_IFACES];
static int		g_iface_count;
static int		g_initialized;
static int		g_polling;

int
net_init(void)
{
	int	i;

	if (g_initialized) {
		return (0);
	}

	for (i = 0; i < NET_MAX_IFACES; i++) {
		g_ifaces[i] = NULL;
	}
	g_iface_count = 0;
	g_polling = 0;

	arp_cache_init();

	g_initialized = 1;
	drivers_log("[NET] network subsystem initialized\n");
	return (0);
}

int
net_is_initialized(void)
{
	return (g_initialized);
}

int
net_iface_register(net_iface_t *iface, netdev_t *ndev)
{
	if (!iface || !ndev) {
		return (-1);
	}
	if (!g_initialized) {
		return (-1);
	}
	if (g_iface_count >= NET_MAX_IFACES) {
		drivers_log("[NET] too many interfaces\n");
		return (-1);
	}
	if (net_iface_find_by_ndev(ndev) || net_iface_by_name(iface->name)) {
		return (-1);
	}

	iface->ndev = ndev;
	ndev->rx_handler = ethernet_input;

	g_ifaces[g_iface_count] = iface;
	iface->index = g_iface_count;
	g_iface_count++;

	drivers_log("[NET] iface %s on %s "
	    "(%02x:%02x:%02x:%02x:%02x:%02x)\n",
	    iface->name, ndev->name,
	    ndev->mac[0], ndev->mac[1], ndev->mac[2],
	    ndev->mac[3], ndev->mac[4], ndev->mac[5]);
	return (0);
}

void
net_iface_unregister(net_iface_t *iface)
{
	int	i, j;

	if (!iface) {
		return;
	}
	for (i = 0; i < g_iface_count; i++) {
		if (g_ifaces[i] != iface) {
			continue;
		}
		if (iface->ndev && iface->ndev->rx_handler == ethernet_input) {
			iface->ndev->rx_handler = NULL;
		}
		for (j = i; j + 1 < g_iface_count; j++) {
			g_ifaces[j] = g_ifaces[j + 1];
			g_ifaces[j]->index = j;
		}
		g_iface_count--;
		g_ifaces[g_iface_count] = NULL;
		iface->ndev = NULL;
		iface->index = -1;
		return;
	}
}

int
net_iface_count(void)
{
	return (g_iface_count);
}

net_iface_t *
net_iface_get(int index)
{
	if (index < 0 || index >= g_iface_count) {
		return (NULL);
	}
	return (g_ifaces[index]);
}

net_iface_t *
net_iface_by_name(const char *name)
{
	int	i;

	if (!name) {
		return (NULL);
	}
	for (i = 0; i < g_iface_count; i++) {
		if (strcmp(g_ifaces[i]->name, name) == 0) {
			return (g_ifaces[i]);
		}
	}
	return (NULL);
}

net_iface_t *
net_iface_find_by_ndev(netdev_t *ndev)
{
	int	i;

	if (!ndev) {
		return (NULL);
	}
	for (i = 0; i < g_iface_count; i++) {
		if (g_ifaces[i]->ndev == ndev) {
			return (g_ifaces[i]);
		}
	}
	return (NULL);
}

void
net_poll_all(void)
{
	int	i;

	if (!g_initialized || g_polling) {
		return;
	}
	g_polling = 1;
	for (i = 0; i < g_iface_count; i++) {
		netdev_t	*ndev;

		ndev = g_ifaces[i]->ndev;
		if (ndev && (ndev->flags & NETDEV_F_UP) &&
		    ndev->ops && ndev->ops->poll) {
			ndev->ops->poll(ndev);
		}
	}
	g_polling = 0;
}

void
net_dump_ifaces(void)
{
	int	i;

	drivers_log("[NET] interfaces (%d):\n", g_iface_count);
	for (i = 0; i < g_iface_count; i++) {
		net_iface_t	*iface;
		netdev_t	*ndev;

		iface = g_ifaces[i];
		ndev = iface->ndev;
		drivers_log("  %d: %s dev=%s "
		    "mac=%02x:%02x:%02x:%02x:%02x:%02x "
		    "flags=0x%x mtu=%d\n",
		    i, iface->name,
		    ndev ? ndev->name : "?",
		    ndev ? ndev->mac[0] : 0,
		    ndev ? ndev->mac[1] : 0,
		    ndev ? ndev->mac[2] : 0,
		    ndev ? ndev->mac[3] : 0,
		    ndev ? ndev->mac[4] : 0,
		    ndev ? ndev->mac[5] : 0,
		    iface->flags,
		    ndev ? ndev->mtu : 0);
		if (iface->ip_addr) {
			drivers_log("      ip=%d.%d.%d.%d "
			    "mask=%d.%d.%d.%d\n",
			    (iface->ip_addr >> 24) & 0xFF,
			    (iface->ip_addr >> 16) & 0xFF,
			    (iface->ip_addr >> 8) & 0xFF,
			    iface->ip_addr & 0xFF,
			    (iface->netmask >> 24) & 0xFF,
			    (iface->netmask >> 16) & 0xFF,
			    (iface->netmask >> 8) & 0xFF,
			    iface->netmask & 0xFF);
		}
	}
}
