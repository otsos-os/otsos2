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
$define %type tcp_header_t as packed struct with TCP wire fields

$define %func tcp_input as function with args net_iface_t *, u32, u32, const u8 *, u16
$define %func tcp_output as function with args net_iface_t *, u32, u16, u16, u32, u32, u16, u16, const u8 *, u16
$define %func tcp_output_opt as function with args net_iface_t *, u32, u16, u16, u32, u32, u16, u16, const u8 *, u16, const u8 *, u16
$define %func tcp_checksum as function with args u32, u32, const u8 *, u16
$define %func tcp_opt_get_mss as function with args const u8 *, u16, u16 *

*/

/* !SPACE!

$space %export tcp_input, tcp_output, tcp_output_opt, tcp_checksum
$space %export tcp_opt_get_mss

*/

#ifndef NET_TCP_H
#define NET_TCP_H

#include <kernel/net/net.h>
#include <mlibc/mlibc.h>

#define	TCP_HEADER_LEN		20
#define	TCP_DATA_OFFSET_MIN	5

/*
 * Option area is capped at 40 bytes by the 4-bit data offset field
 * (15 words * 4 - 20).  We only ever emit MSS + padding, but the input
 * parser must tolerate anything a peer sends up to that ceiling.
 */
#define	TCP_OPT_MAX_LEN		40

#define	TCP_OPT_END		0
#define	TCP_OPT_NOP		1
#define	TCP_OPT_MSS		2
#define	TCP_OPT_MSS_LEN		4

/*
 * RFC 1122 4.2.2.6: a peer that sends no MSS option must be assumed to
 * accept 536 bytes of payload.  Anything smaller than TCP_MSS_MIN is a
 * broken announcement and is clamped, otherwise a hostile or buggy peer
 * could force us into one-byte segments.
 */
#define	TCP_MSS_DEFAULT		536
#define	TCP_MSS_MIN		88

#define	TCP_FLAG_FIN		0x001
#define	TCP_FLAG_SYN		0x002
#define	TCP_FLAG_RST		0x004
#define	TCP_FLAG_PSH		0x008
#define	TCP_FLAG_ACK		0x010

typedef struct {
	u16	src_port;
	u16	dst_port;
	u32	seq;
	u32	ack;
	u16	offset_flags;
	u16	window;
	u16	checksum;
	u16	urgent;
} __attribute__((packed)) tcp_header_t;

int	tcp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    const u8 *data, u16 len);
int	tcp_output(net_iface_t *iface, u32 dst_ip, u16 src_port,
    u16 dst_port, u32 seq, u32 ack, u16 flags, u16 window,
    const u8 *data, u16 len);
int	tcp_output_opt(net_iface_t *iface, u32 dst_ip, u16 src_port,
    u16 dst_port, u32 seq, u32 ack, u16 flags, u16 window,
    const u8 *opts, u16 opt_len, const u8 *data, u16 len);
u16	tcp_checksum(u32 src_ip, u32 dst_ip, const u8 *segment,
    u16 len);
int	tcp_opt_get_mss(const u8 *opts, u16 opt_len, u16 *out_mss);

#endif
