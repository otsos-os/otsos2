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
$define %type ethernet_header_t as packed struct with dst/src mac + ethertype

$define %func ethernet_input as procedure with args netdev_t *, const u8 *, u16
$define %func ethernet_output as function with args net_iface_t *, const u8 *, u16, const u8 *, u16

*/

/* !SPACE!

$space %export ethernet_input, ethernet_output

*/

#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <kernel/net/netdev/netdev.h>
#include <kernel/net/net.h>
#include <mlibc/mlibc.h>

#define	ETHERNET_ADDR_LEN	6
#define	ETHERNET_HEADER_LEN	14
#define	ETHERNET_MTU		1500
#define	ETHERNET_FRAME_MIN	64
#define	ETHERNET_FRAME_MAX	1514

#define	ETHERTYPE_IPV4		0x0800
#define	ETHERTYPE_ARP		0x0806
#define	ETHERTYPE_IPV6		0x86DD

#define	ETHERNET_BCAST_ADDR	"\xFF\xFF\xFF\xFF\xFF\xFF"

typedef struct {
	u8	dst[ETHERNET_ADDR_LEN];
	u8	src[ETHERNET_ADDR_LEN];
	u16	ethertype;
} __attribute__((packed)) ethernet_header_t;

void	ethernet_input(netdev_t *ndev, const u8 *frame, u16 len);
int	ethernet_output(net_iface_t *iface, const u8 *dst_mac,
    u16 ethertype, const u8 *payload, u16 payload_len);

#endif
