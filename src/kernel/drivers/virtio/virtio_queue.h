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
$define %type u64 as 64 bit unsigned
$define %type virtq_desc_t as packed struct with descriptor entry
$define %type virtq_avail_t as packed struct with available ring header
$define %type virtq_used_elem_t as packed struct with used ring element
$define %type virtq_used_t as packed struct with used ring header
$define %type virtio_vq_t as struct with virtqueue runtime state
$define %type virtio_hw_t as struct with resolved transport state

$define %func virtio_virt_to_phys as function with args void *
$define %func virtio_vq_create as function with args virtio_vq_t *, u16
$define %func virtio_vq_destroy as procedure with args virtio_vq_t *
$define %func virtio_vq_bind as procedure with args virtio_vq_t *, virtio_hw_t *, u16
$define %func virtio_vq_alloc_chain as function with args virtio_vq_t *, u16
$define %func virtio_vq_set_desc as procedure with args virtio_vq_t *, u16, u64, u32, u16, u16
$define %func virtio_vq_kick as procedure with args virtio_vq_t *, u16
$define %func virtio_vq_poll as function with args virtio_vq_t *
$define %func virtio_vq_free_chain as procedure with args virtio_vq_t *, u16
$define %func virtio_vq_send_recv as function with args virtio_vq_t *, const void *, u32, void *, u32

*/

/* !SPACE!

$space %export virtio_virt_to_phys
$space %export virtio_vq_create, virtio_vq_destroy, virtio_vq_bind
$space %export virtio_vq_alloc_chain, virtio_vq_set_desc
$space %export virtio_vq_kick, virtio_vq_poll, virtio_vq_free_chain
$space %export virtio_vq_send_recv

*/

#ifndef VIRTIO_QUEUE_H
#define VIRTIO_QUEUE_H

#include <mlibc/mlibc.h>
#include <kernel/drivers/virtio/virtio_hw.h>

#define	VIRTQ_DESC_F_NEXT		1
#define	VIRTQ_DESC_F_WRITE		2
#define	VIRTQ_DESC_F_INDIRECT		4
#define	VIRTQ_AVAIL_F_NO_INTERRUPT	1
#define	VIRTQ_USED_F_NO_NOTIFY		1

typedef struct {
	u64	addr;
	u32	len;
	u16	flags;
	u16	next;
} __attribute__((packed)) virtq_desc_t;

typedef struct {
	u16	flags;
	u16	idx;
	u16	ring[];
} __attribute__((packed)) virtq_avail_t;

typedef struct {
	u32	id;
	u32	len;
} __attribute__((packed)) virtq_used_elem_t;

typedef struct {
	u16			flags;
	u16			idx;
	virtq_used_elem_t	ring[];
} __attribute__((packed)) virtq_used_t;

typedef struct {
	u16			size;
	u16			free_head;
	u16			num_free;
	u16			avail_idx;
	u16			used_idx;
	virtq_desc_t		*desc;
	virtq_avail_t		*avail;
	virtq_used_t		*used;
	u64			phys_desc;
	u64			phys_avail;
	u64			phys_used;
	void			*backing;
	u64			backing_size;
	void			*dma_cmd;
	void			*dma_resp;
	u64			dma_cmd_phys;
	u64			dma_resp_phys;
	virtio_hw_t		*hw;
	u16			queue_index;
} virtio_vq_t;

u64	virtio_virt_to_phys(void *vaddr);
int	virtio_vq_create(virtio_vq_t *vq, u16 queue_size);
void	virtio_vq_destroy(virtio_vq_t *vq);
void	virtio_vq_bind(virtio_vq_t *vq, virtio_hw_t *hw,
    u16 queue_index);
u16	virtio_vq_alloc_chain(virtio_vq_t *vq, u16 n);
void	virtio_vq_set_desc(virtio_vq_t *vq, u16 idx, u64 addr,
    u32 len, u16 flags, u16 next_idx);
void	virtio_vq_kick(virtio_vq_t *vq, u16 head_idx);
u16	virtio_vq_poll(virtio_vq_t *vq);
u16	virtio_vq_poll_used(virtio_vq_t *vq, u32 *len);
u16	virtio_vq_pop_used(virtio_vq_t *vq, u32 *len);
void	virtio_vq_free_chain(virtio_vq_t *vq, u16 head_idx);
int	virtio_vq_send_recv(virtio_vq_t *vq, const void *cmd,
    u32 cmd_size, void *resp, u32 resp_size);
#endif
