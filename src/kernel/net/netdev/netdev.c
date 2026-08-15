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

$define %func netdev_register as function with args netdev_t *
$define %func netdev_unregister as procedure with args netdev_t *
$define %func netdev_count as function with args void
$define %func netdev_get as function with args int
$define %func netdev_by_name as function with args const char *
$define %func netdev_poll_all as procedure with args void
$define %func netdev_dump_all as procedure with args void

*/

/* !SPACE!

$space %export netdev_register, netdev_unregister, netdev_count
$space %export netdev_get, netdev_by_name
$space %export netdev_poll_all, netdev_dump_all

*/

#include <kernel/net/netdev/netdev.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static netdev_t		*g_devices[NETDEV_MAX_DEVICES];
static int		g_device_count;

int
netdev_register(netdev_t *ndev)
{
	int	i;

	if (!ndev || !ndev->ops || !ndev->ops->transmit ||
	    !ndev->ops->poll) {
		return (-1);
	}
	if (ndev->mtu == 0) {
		return (-1);
	}
	if (ndev->mtu_max == 0) {
		ndev->mtu_max = ndev->mtu;
	}
	if (g_device_count >= NETDEV_MAX_DEVICES) {
		drivers_log("[NETDEV] too many devices\n");
		return (-1);
	}
	for (i = 0; i < g_device_count; i++) {
		if (g_devices[i] == ndev ||
		    strcmp(g_devices[i]->name, ndev->name) == 0) {
			return (-1);
		}
	}

	ndev->rx_handler = NULL;
	g_devices[g_device_count] = ndev;
	ndev->index = g_device_count;
	g_device_count++;

	drivers_log("[NETDEV] %s registered "
	    "(%02x:%02x:%02x:%02x:%02x:%02x)\n",
	    ndev->name,
	    ndev->mac[0], ndev->mac[1], ndev->mac[2],
	    ndev->mac[3], ndev->mac[4], ndev->mac[5]);
	return (0);
}

void
netdev_unregister(netdev_t *ndev)
{
	int	i, j;

	if (!ndev) {
		return;
	}
	for (i = 0; i < g_device_count; i++) {
		if (g_devices[i] != ndev) {
			continue;
		}
		for (j = i; j + 1 < g_device_count; j++) {
			g_devices[j] = g_devices[j + 1];
			g_devices[j]->index = j;
		}
		g_device_count--;
		g_devices[g_device_count] = NULL;
		ndev->rx_handler = NULL;
		ndev->index = -1;
		return;
	}
}

int
netdev_count(void)
{
	return (g_device_count);
}

netdev_t *
netdev_get(int index)
{
	if (index < 0 || index >= g_device_count) {
		return (NULL);
	}
	return (g_devices[index]);
}

netdev_t *
netdev_by_name(const char *name)
{
	int	i;

	if (!name) {
		return (NULL);
	}
	for (i = 0; i < g_device_count; i++) {
		if (strcmp(g_devices[i]->name, name) == 0) {
			return (g_devices[i]);
		}
	}
	return (NULL);
}

void
netdev_poll_all(void)
{
	int	i;

	for (i = 0; i < g_device_count; i++) {
		if (g_devices[i]->flags & NETDEV_F_UP &&
		    g_devices[i]->ops && g_devices[i]->ops->poll) {
			g_devices[i]->ops->poll(g_devices[i]);
		}
	}
}

void
netdev_dump_all(void)
{
	int	i;

	drivers_log("[NETDEV] devices (%d):\n", g_device_count);
	for (i = 0; i < g_device_count; i++) {
		netdev_t	*ndev;

		ndev = g_devices[i];
		drivers_log("  %d: %s mac="
		    "%02x:%02x:%02x:%02x:%02x:%02x "
		    "mtu=%d flags=0x%x driver=%s\n",
		    i, ndev->name,
		    ndev->mac[0], ndev->mac[1], ndev->mac[2],
		    ndev->mac[3], ndev->mac[4], ndev->mac[5],
		    ndev->mtu, ndev->flags,
		    ndev->ops->name ? ndev->ops->name : "?");
	}
}
