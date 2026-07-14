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
$define %type udp_header_t as packed struct with UDP src/dst port + length + checksum

$define %func udp_input as function with args net_iface_t *, u32, const u8 *, u16

*/

/* !SPACE!

$space %export udp_input

*/

#include <kernel/net/udp.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

int
udp_input(net_iface_t *iface, u32 src_ip,
    const u8 *data, u16 len)
{
	const udp_header_t	*udp;
	u16			udp_len;

	(void)iface;
	(void)src_ip;
	if (!data || len < UDP_HEADER_LEN) {
		return (-1);
	}

	udp = (const udp_header_t *)data;
	udp_len = __builtin_bswap16(udp->length);
	if (udp_len < UDP_HEADER_LEN || udp_len > len) {
		return (-1);
	}

	drivers_log("[UDP] port %d -> %d (%d bytes)\n",
	    __builtin_bswap16(udp->src_port),
	    __builtin_bswap16(udp->dst_port),
	    udp_len - UDP_HEADER_LEN);

	return (0);
}
