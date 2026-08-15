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
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type netdev_t as struct with physical network device state
$define %type netdev_ops_t as struct with hardware driver callbacks
$define %type netdev_rx_handler_t as callback for raw frame receive

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

#ifndef NET_NETDEV_NETDEV_H
#define NET_NETDEV_NETDEV_H

#include <mlibc/mlibc.h>

#define	NETDEV_MAX_DEVICES	8
#define	NETDEV_NAME_LEN		16
#define	NETDEV_MAC_SIZE		6
#define	NETDEV_MTU_DEFAULT	1500

#define	NETDEV_F_UP		(1 << 0)
#define	NETDEV_F_RUNNING	(1 << 1)
#define	NETDEV_F_LOOPBACK	(1 << 2)
#define	NETDEV_F_PROMISC	(1 << 3)
#define	NETDEV_F_BROADCAST	(1 << 4)

struct netdev;

typedef void (*netdev_rx_handler_t)(struct netdev *ndev,
    const u8 *frame, u16 len);

typedef struct netdev_ops {
	const char	*name;
	int		(*transmit)(struct netdev *ndev,
    const u8 *frame, u16 len);
	int		(*poll)(struct netdev *ndev);
	int		(*is_link_up)(struct netdev *ndev);
} netdev_ops_t;

typedef struct netdev {
	char			name[NETDEV_NAME_LEN];
	u8			mac[NETDEV_MAC_SIZE];
	u16			mtu;
	u16			mtu_max;
	int			flags;
	int			index;
	void			*priv;
	netdev_ops_t		*ops;
	netdev_rx_handler_t	rx_handler;
	u64			tx_submitted;
	u64			tx_completed;
	u64			tx_dropped;
	u64			rx_completed;
	u64			rx_delivered;
	u64			rx_dropped;
} netdev_t;

int	netdev_register(netdev_t *ndev);
void	netdev_unregister(netdev_t *ndev);
int	netdev_count(void);
netdev_t *netdev_get(int index);
netdev_t *netdev_by_name(const char *name);
void	netdev_poll_all(void);
void	netdev_dump_all(void);

#endif
