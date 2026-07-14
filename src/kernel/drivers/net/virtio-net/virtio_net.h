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
$define %type virtio_net_rx_handler_t as function pointer for received Ethernet frames

$define %func virtio_net_pci_register as function with args void
$define %func virtio_net_is_ready as function with args void
$define %func virtio_net_transmit as function with args const void *, u16
$define %func virtio_net_poll as function with args void
$define %func virtio_net_set_rx_handler as function with args virtio_net_rx_handler_t, void *
$define %func virtio_net_get_mac as function with args u8 *

*/

/* !SPACE!

$space %export virtio_net_pci_register, virtio_net_is_ready
$space %export virtio_net_transmit, virtio_net_poll
$space %export virtio_net_set_rx_handler, virtio_net_get_mac

*/

#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include <mlibc/mlibc.h>

#define	VIRTIO_NET_MAC_SIZE	6
#define	VIRTIO_NET_ETH_FRAME_MAX	1514

typedef void (*virtio_net_rx_handler_t)(const u8 *frame, u16 len,
    void *arg);

int	virtio_net_pci_register(void);
int	virtio_net_is_ready(void);
int	virtio_net_transmit(const void *frame, u16 len);
int	virtio_net_poll(void);
int	virtio_net_set_rx_handler(virtio_net_rx_handler_t handler,
    void *arg);
int	virtio_net_get_mac(u8 mac[VIRTIO_NET_MAC_SIZE]);

#endif
