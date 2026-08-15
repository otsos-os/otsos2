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
$define %type net_stack_config_t as struct with global network policy
$define %type net_iface_config_t as struct with interface network policy

$define %func net_read_stack_config as function with args net_stack_config_t *
$define %func net_read_iface_config as function with args net_iface_t *, net_iface_config_t *
$define %func net_apply_iface_config as procedure with args net_iface_t *, const net_iface_config_t *, int
$define %func net_init as function with args void
$define %func net_is_initialized as function with args void
$define %func net_iface_register as function with args net_iface_t *, netdev_t *
$define %func net_iface_unregister as procedure with args net_iface_t *
$define %func net_iface_count as function with args void
$define %func net_iface_get as function with args int
$define %func net_iface_by_name as function with args const char *
$define %func net_iface_find_by_ndev as function with args netdev_t *
$define %func net_poll_all as procedure with args void
$define %func net_tick as procedure with args void
$define %func net_request_poll as procedure with args void
$define %func net_dump_ifaces as procedure with args void
$define %func net_cm_update as function with args u32
$define %func net_iface_set_ip as procedure with args net_iface_t *, u32
$define %func net_iface_set_netmask as procedure with args net_iface_t *, u32
$define %func net_iface_set_gw as procedure with args net_iface_t *, u32

*/

/* !SPACE!

$space %internal net_read_stack_config, net_read_iface_config
$space %internal net_apply_iface_config
$space %export net_init, net_is_initialized
$space %export net_iface_register, net_iface_unregister, net_iface_count
$space %export net_iface_get, net_iface_by_name
$space %export net_iface_find_by_ndev
$space %export net_poll_all, net_tick, net_request_poll, net_dump_ifaces
$space %export net_cm_update
$space %export net_iface_set_ip, net_iface_set_netmask, net_iface_set_gw

*/

#include <kernel/net/net.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/arp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/udp.h>
#include <kernel/net/endpoint.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/drivers/timer.h>
#include <kernel/cm/cm.h>
#include <kernel/api/errno.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static net_iface_t	*g_ifaces[NET_MAX_IFACES];
static int		g_iface_count;
static int		g_initialized;
static int		g_polling;
static u64		g_last_poll_tick;
static int		g_poll_requested;
static int		g_stack_enabled = 1;
static u32		g_poll_hz = NET_POLL_HZ_DEFAULT;
static u8		g_default_ttl = IPV4_TTL_DEFAULT;

#define	NET_POLL_HZ_MIN	1
#define	NET_POLL_HZ_MAX	1000
#define	NET_IPV4_TTL_MIN	1
#define	NET_IPV4_TTL_MAX	255
#define	NET_IPV4_MTU_MIN	68

typedef struct net_stack_config {
	int	enabled;
	u32	poll_hz;
	u8	default_ttl;
} net_stack_config_t;

typedef struct net_iface_config {
	int	enabled;
	u32	ip;
	u32	mask;
	u32	gw;
	u16	mtu;
} net_iface_config_t;

static int
net_read_stack_config(net_stack_config_t *config)
{
	u32	poll_hz;
	u32	default_ttl;

	if (!config) {
		return (-API_ERR_BAD_VALUE);
	}
	poll_hz = cm_get_u32_default("NETWORK", "Stack", "PollHz",
	    NET_POLL_HZ_DEFAULT);
	default_ttl = cm_get_u32_default("NETWORK", "Stack", "DefaultTtl",
	    IPV4_TTL_DEFAULT);
	if (poll_hz < NET_POLL_HZ_MIN || poll_hz > NET_POLL_HZ_MAX ||
	    default_ttl < NET_IPV4_TTL_MIN ||
	    default_ttl > NET_IPV4_TTL_MAX) {
		return (-API_ERR_BAD_VALUE);
	}
	config->enabled = cm_get_bool_default("NETWORK", "Stack",
	    "Enabled", 1);
	config->poll_hz = poll_hz;
	config->default_ttl = (u8)default_ttl;
	return (0);
}

static int
net_read_iface_config(net_iface_t *iface, net_iface_config_t *config)
{
	char	key[128];
	u32	mtu;
	u32	mtu_max;

	if (!iface || !iface->ndev || !config) {
		return (-API_ERR_BAD_VALUE);
	}
	if (strlen("Interfaces.") + strlen(iface->name) >= sizeof(key)) {
		return (-API_ERR_TOO_BIG);
	}

	strcpy(key, "Interfaces.");
	strcat(key, iface->name);

	config->enabled = cm_get_bool_default("NETWORK", key, "Enabled", 1);
	config->ip = cm_get_ipv4_default("NETWORK", key, "Address",
	    iface->ip_addr);
	config->mask = cm_get_ipv4_default("NETWORK", key, "Netmask",
	    iface->netmask);
	config->gw = cm_get_ipv4_default("NETWORK", key, "Gateway",
	    iface->gw_addr);
	mtu_max = iface->ndev->mtu_max;
	if (!(iface->flags & NET_IFF_LOOPBACK) && mtu_max > ETHERNET_MTU) {
		mtu_max = ETHERNET_MTU;
	}
	mtu = cm_get_u32_default("NETWORK", key, "Mtu", mtu_max);
	if (mtu < NET_IPV4_MTU_MIN || mtu > mtu_max || mtu > 0xFFFF) {
		return (-API_ERR_BAD_VALUE);
	}
	config->mtu = (u16)mtu;
	return (0);
}

static void
net_apply_iface_config(net_iface_t *iface, const net_iface_config_t *config,
    int stack_enabled)
{
	if (!iface || !iface->ndev || !config) {
		return;
	}
	iface->ndev->mtu = config->mtu;
	if (!stack_enabled || !config->enabled) {
		iface->flags &= ~NET_IFF_UP;
		return;
	}

	iface->flags |= NET_IFF_UP;
	iface->ip_addr = config->ip;
	iface->netmask = config->mask;
	iface->gw_addr = config->gw;
	drivers_log("[NET] %s configured %d.%d.%d.%d/%d.%d.%d.%d gw "
	    "%d.%d.%d.%d mtu %d\n", iface->name,
	    (config->ip >> 24) & 0xFF, (config->ip >> 16) & 0xFF,
	    (config->ip >> 8) & 0xFF, config->ip & 0xFF,
	    (config->mask >> 24) & 0xFF, (config->mask >> 16) & 0xFF,
	    (config->mask >> 8) & 0xFF, config->mask & 0xFF,
	    (config->gw >> 24) & 0xFF, (config->gw >> 16) & 0xFF,
	    (config->gw >> 8) & 0xFF, config->gw & 0xFF,
	    config->mtu);
}

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
	g_last_poll_tick = 0;
	g_poll_requested = 1;
	g_stack_enabled = 1;
	g_poll_hz = NET_POLL_HZ_DEFAULT;
	g_default_ttl = IPV4_TTL_DEFAULT;

	arp_cache_init();
	udp_init();
	net_endpoint_init();
	cm_register_consumer(CM_CONSUMER_NET, "net", net_cm_update);
	g_initialized = 1;

	drivers_log("[NET] network subsystem initialized\n");
	return (0);
}

void
net_request_poll(void)
{
	g_poll_requested = 1;
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
	if (cm_is_initialized()) {
		(void)net_cm_update(0);
	}

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

static void
net_core_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "net_core", 0) == NULL) {
		device_add_child(parent, "net_core", 0);
	}
}

static void
net_core_poll(void *arg)
{
	(void)arg;
	net_tick();
}

static int
net_core_attach(device_t dev)
{
	if (net_init() != 0) {
		return (-1);
	}
	bus_setup_poll(dev, NB_POLL_TIMER, net_core_poll, NULL, NULL);
	return (0);
}

static devclass_t net_core_devclass = {
	.name		= "net",
	.maxunit	= 1,
};

static driver_t net_core_driver = {
	.name		= "net_core",
	.identify	= net_core_identify,
	.probe		= NULL,
	.attach		= net_core_attach,
};

PSEUDO_DRIVER_MODULE(net_core, net_core_driver, net_core_devclass,
    NEWBUS_PASS_CORE, NEWBUS_ORDER_MIDDLE);

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

	if (!g_initialized || !g_stack_enabled || g_polling) {
		return;
	}
	g_polling = 1;
	arp_tick();
	net_endpoint_tick();
	for (i = 0; i < g_iface_count; i++) {
		netdev_t	*ndev;

		ndev = g_ifaces[i]->ndev;
		if (ndev && (g_ifaces[i]->flags & NET_IFF_UP) &&
		    (ndev->flags & NETDEV_F_UP) &&
		    ndev->ops && ndev->ops->poll) {
			ndev->ops->poll(ndev);
		}
	}
	g_polling = 0;
}

void
net_tick(void)
{
	u32	freq;
	u64	now, interval;

	if (!g_initialized || !g_stack_enabled) {
		return;
	}
	if (!timer_is_initialized()) {
		net_poll_all();
		return;
	}

	freq = timer_get_frequency();
	interval = freq / g_poll_hz;
	if (interval == 0) {
		interval = 1;
	}

	now = timer_get_ticks();
	if (!g_poll_requested && g_last_poll_tick != 0 &&
	    now - g_last_poll_tick < interval) {
		return;
	}

	g_poll_requested = 0;
	g_last_poll_tick = now;
	net_poll_all();
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

int
net_cm_update(u32 flags)
{
	net_iface_config_t	iface_config[NET_MAX_IFACES];
	net_stack_config_t	stack_config;
	int			i;
	int			ret;

	(void)flags;
	if (!g_initialized) {
		return (-API_ERR_NOT_FOUND);
	}

	ret = net_read_stack_config(&stack_config);
	if (ret != 0) {
		return (ret);
	}
	for (i = 0; i < g_iface_count; i++) {
		ret = net_read_iface_config(g_ifaces[i], &iface_config[i]);
		if (ret != 0) {
			return (ret);
		}
	}

	g_stack_enabled = stack_config.enabled;
	g_poll_hz = stack_config.poll_hz;
	g_default_ttl = stack_config.default_ttl;
	g_last_poll_tick = 0;
	g_poll_requested = 1;
	for (i = 0; i < g_iface_count; i++) {
		net_apply_iface_config(g_ifaces[i], &iface_config[i],
		    g_stack_enabled);
	}
	return (0);
}

int
net_stack_enabled(void)
{
	return (g_stack_enabled);
}

u8
net_default_ttl(void)
{
	return (g_default_ttl);
}

void
net_iface_set_ip(net_iface_t *iface, u32 ip)
{
	if (!iface) {
		return;
	}
	iface->ip_addr = ip;
}

void
net_iface_set_netmask(net_iface_t *iface, u32 mask)
{
	if (!iface) {
		return;
	}
	iface->netmask = mask;
}

void
net_iface_set_gw(net_iface_t *iface, u32 gw)
{
	if (!iface) {
		return;
	}
	iface->gw_addr = gw;
}
