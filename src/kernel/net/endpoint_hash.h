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
$define %type net_endpoint_t as native network endpoint state
$define %type net_endpoint_walker_t as endpoint lookup iteration cursor
$define %type net_iface_t as struct with logical network interface state

$define %func net_endpoint_hash_init as procedure with args void
$define %func net_endpoint_hash_insert as procedure with args net_endpoint_t *
$define %func net_endpoint_hash_remove as procedure with args net_endpoint_t *
$define %func net_endpoint_hash_rebuild as procedure with args void
$define %func net_endpoint_find as function with args u32, u32, u16, u16, int
$define %func net_endpoint_find_listener as function with args u32, u16, int

*/

/* !SPACE!

$space %internal net_endpoint_hash_rebuild
$space %export net_endpoint_hash_init, net_endpoint_hash_insert
$space %export net_endpoint_hash_remove
$space %export net_endpoint_find, net_endpoint_find_listener

*/

#ifndef NET_ENDPOINT_HASH_H
#define NET_ENDPOINT_HASH_H

#include <kernel/net/endpoint.h>
#include <mlibc/mlibc.h>


#define	NET_ENDPOINT_HASH_SIZE		67
#define	NET_ENDPOINT_HASH_NONE		(-1)

struct net_endpoint_hash_bucket {
	int	head;
};

void	net_endpoint_hash_init(void);
void	net_endpoint_hash_insert(net_endpoint_t *ep);
void	net_endpoint_hash_remove(net_endpoint_t *ep);
net_endpoint_t *net_endpoint_find(u32 local_ip, u32 peer_ip,
    u16 local_port, u16 peer_port);
net_endpoint_t *net_endpoint_find_listener(u32 local_ip, u16 local_port);

#endif
