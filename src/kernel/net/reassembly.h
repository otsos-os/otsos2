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

$define %func ipv4_reasm_init as procedure with args void
$define %func ipv4_reasm_input as function with args net_iface_t *, const u8 *, u16, u8
$define %func ipv4_reasm_tick as procedure with args void
$define %func ipv4_reasm_evict as procedure with args net_iface_t *

*/

/* !SPACE!

$space %export ipv4_reasm_init, ipv4_reasm_input, ipv4_reasm_tick
$space %export ipv4_reasm_evict

*/

#ifndef NET_IPV4_REASSEMBLY_H
#define NET_IPV4_REASSEMBLY_H

#include <kernel/net/net.h>
#include <kernel/net/ethernet.h>
#include <mlibc/mlibc.h>

#define	IPV4_REASM_MAX_FRAGMENTS		16
#define	IPV4_REASM_TIMEOUT_MS		30000
#define	IPV4_REASM_INLINE_MAX	(ETHERNET_MTU)

int	ipv4_reasm_input(net_iface_t *iface, const u8 *data, u16 hdr_len,
    u16 payload_len, u32 id, u32 src_ip, u32 dst_ip, u8 proto,
    u16 frag_flags, u16 frag_off, const u8 **out_datagram,
    u16 *out_len, u8 **out_buf);
void	ipv4_reasm_init(void);
void	ipv4_reasm_tick(void);
void	ipv4_reasm_evict(net_iface_t *iface);

#endif
