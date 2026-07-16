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
$define %type netdev_t as struct with physical network device state
$define %type net_iface_t as struct with logical network interface state

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
$define %func net_iface_set_ip as procedure with args net_iface_t *, u32
$define %func net_iface_set_netmask as procedure with args net_iface_t *, u32
$define %func net_iface_set_gw as procedure with args net_iface_t *, u32

*/

/* !SPACE!

$space %export net_init, net_is_initialized
$space %export net_iface_register, net_iface_unregister, net_iface_count
$space %export net_iface_get, net_iface_by_name
$space %export net_iface_find_by_ndev
$space %export net_poll_all, net_tick, net_request_poll, net_dump_ifaces
$space %export net_iface_set_ip, net_iface_set_netmask, net_iface_set_gw

*/

#ifndef NET_NET_H
#define NET_NET_H

#include <kernel/net/netdev/netdev.h>
#include <mlibc/mlibc.h>

#define	NET_MAX_IFACES		8
#define	NET_IFACE_NAME_LEN	16
#define	NET_POLL_HZ		100
#define	NET_TX_PENDING		(-2)

#define	NET_IFF_UP		(1 << 0)
#define	NET_IFF_RUNNING		(1 << 1)
#define	NET_IFF_LOOPBACK	(1 << 2)

typedef struct net_iface {
	char			name[NET_IFACE_NAME_LEN];
	netdev_t		*ndev;
	u32			ip_addr;
	u32			netmask;
	u32			gw_addr;
	int			flags;
	int			index;
} net_iface_t;

int	net_init(void);
int	net_is_initialized(void);
int	net_iface_register(net_iface_t *iface, netdev_t *ndev);
void	net_iface_unregister(net_iface_t *iface);
int	net_iface_count(void);
net_iface_t *net_iface_get(int index);
net_iface_t *net_iface_by_name(const char *name);
net_iface_t *net_iface_find_by_ndev(netdev_t *ndev);
void	net_poll_all(void);
void	net_tick(void);
void	net_request_poll(void);
void	net_dump_ifaces(void);
void	net_iface_set_ip(net_iface_t *iface, u32 ip);
void	net_iface_set_netmask(net_iface_t *iface, u32 mask);
void	net_iface_set_gw(net_iface_t *iface, u32 gw);

#endif
