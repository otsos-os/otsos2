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

#include <kernel/net/ethernet.h>
#include <kernel/net/arp.h>
#include <kernel/net/ipv4.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

void
ethernet_input(netdev_t *ndev, const u8 *frame, u16 len)
{
	const ethernet_header_t	*eth;
	net_iface_t		*iface;
	u16			ethertype;

	if (!ndev || !frame || len < ETHERNET_HEADER_LEN) {
		return;
	}

	iface = net_iface_find_by_ndev(ndev);
	if (!iface) {
		return;
	}

	eth = (const ethernet_header_t *)frame;
	ethertype = __builtin_bswap16(eth->ethertype);

	switch (ethertype) {
	case ETHERTYPE_ARP:
		arp_input(iface, frame + ETHERNET_HEADER_LEN,
		    (u16)(len - ETHERNET_HEADER_LEN));
		break;

	case ETHERTYPE_IPV4:
		ipv4_input(iface, eth->src, frame +
		    ETHERNET_HEADER_LEN,
		    (u16)(len - ETHERNET_HEADER_LEN));
		break;

	default:
		break;
	}
}

int
ethernet_output(net_iface_t *iface, const u8 *dst_mac,
    u16 ethertype, const u8 *payload, u16 payload_len)
{
	u8			frame[ETHERNET_FRAME_MAX];
	ethernet_header_t	*eth;
	netdev_t		*ndev;

	if (!iface || !dst_mac || !payload) {
		return (-1);
	}
	if (payload_len > ETHERNET_FRAME_MAX - ETHERNET_HEADER_LEN) {
		return (-1);
	}

	ndev = iface->ndev;
	if (!ndev || !ndev->ops || !ndev->ops->transmit) {
		return (-1);
	}

	eth = (ethernet_header_t *)frame;
	memcpy(eth->dst, dst_mac, ETHERNET_ADDR_LEN);
	memcpy(eth->src, ndev->mac, ETHERNET_ADDR_LEN);
	eth->ethertype = __builtin_bswap16(ethertype);
	memcpy(frame + ETHERNET_HEADER_LEN, payload, payload_len);

	return (ndev->ops->transmit(ndev, frame,
	    (u16)(ETHERNET_HEADER_LEN + payload_len)));
}
