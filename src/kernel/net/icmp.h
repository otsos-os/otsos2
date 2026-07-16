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
$define %type net_iface_t as struct with network interface state
$define %type icmp_header_t as packed struct with ICMP type/code/checksum

$define %func icmp_input as function with args net_iface_t *, u32, const u8 *, u16
$define %func icmp_send_unreachable as function with args net_iface_t *, u32, u8, const u8 *, u16
$define %func icmp_get_unreach_sent as function with args void

*/

/* !SPACE!

$space %export icmp_input, icmp_send_unreachable
$space %export icmp_get_unreach_sent

*/

#ifndef NET_ICMP_H
#define NET_ICMP_H

#include <kernel/net/net.h>
#include <mlibc/mlibc.h>

#define	ICMP_TYPE_ECHO_REPLY		0
#define	ICMP_TYPE_ECHO_REQUEST		8
#define	ICMP_TYPE_DEST_UNREACH		3
#define	ICMP_TYPE_TIME_EXCEEDED		11

#define	ICMP_CODE_NET_UNREACH		0
#define	ICMP_CODE_HOST_UNREACH		1
#define	ICMP_CODE_PROT_UNREACH		2
#define	ICMP_CODE_PORT_UNREACH		3

typedef struct {
	u8	type;
	u8	code;
	u16	checksum;
	u16	id;
	u16	seq;
} __attribute__((packed)) icmp_header_t;

typedef struct {
	u8	type;
	u8	code;
	u16	checksum;
	u32	unused;
	u8	original_iphdr_and_data[];
} __attribute__((packed)) icmp_unreachable_t;

int	icmp_input(net_iface_t *iface, u32 src_ip,
    const u8 *data, u16 len);
int	icmp_send_unreachable(net_iface_t *iface, u32 dst_ip,
    u8 code, const u8 *original_ip_packet, u16 original_len);
int	icmp_get_unreach_sent(void);

#endif
