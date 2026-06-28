/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Split virtqueue implementation.
 *
 * A virtqueue is the communication channel between guest and device. The
 * guest offers buffers (descriptor chains) via the available ring; the
 * device processes them and returns them via the used ring. Each descriptor
 * carries a guest-physical address and length, plus flags indicating whether
 * the buffer is device-readable, device-writable, or chained to the next
 * descriptor.
 *
 * We use the split virtqueue layout (not packed) which is the baseline
 * transport. The descriptor table, available ring, and used ring are
 * allocated in page-aligned guest memory and their physical addresses are
 * written to the common config's queue_desc/queue_driver/queue_device fields.
 */

#ifndef VIRTIO_QUEUE_H
#define VIRTIO_QUEUE_H

#include <mlibc/mlibc.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio_hw.h>

/* Descriptor flags. */
#define VIRTQ_DESC_F_NEXT     1
#define VIRTQ_DESC_F_WRITE    2
#define VIRTQ_DESC_F_INDIRECT 4

/* Available ring flags. */
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

/* Used ring flags. */
#define VIRTQ_USED_F_NO_NOTIFY 1

/* On-wire descriptor (16 bytes). */
typedef struct {
  u64 addr;
  u32 len;
  u16 flags;
  u16 next;
} __attribute__((packed)) virtq_desc_t;

/* Available ring header + variable-length ring. */
typedef struct {
  u16 flags;
  u16 idx;
  u16 ring[];
} __attribute__((packed)) virtq_avail_t;

/* Used ring element (8 bytes). */
typedef struct {
  u32 id;
  u32 len;
} __attribute__((packed)) virtq_used_elem_t;

/* Used ring header + variable-length ring. */
typedef struct {
  u16 flags;
  u16 idx;
  virtq_used_elem_t ring[];
} __attribute__((packed)) virtq_used_t;

/*
 * virtio_vq_t — runtime state for a single virtqueue.
 *
 * The desc/avail/used pointers are kernel virtual addresses. The physical
 * addresses are stored in phys_desc/phys_avail/phys_used and written to the
 * common config during setup.
 */
typedef struct {
  u16 size;
  u16 free_head;
  u16 num_free;
  u16 avail_idx;
  u16 used_idx;

  virtq_desc_t *desc;
  virtq_avail_t *avail;
  virtq_used_t *used;

  u64 phys_desc;
  u64 phys_avail;
  u64 phys_used;

  void *backing;
  u64 backing_size;

  /* DMA scratch buffers for command/response. These are page-aligned
   * allocations from the identity-mapped kernel heap, guaranteeing
   * physical contiguity. Stack variables cannot be used as DMA targets
   * because they may cross page boundaries with non-contiguous physical
   * backing. */
  void *dma_cmd;
  void *dma_resp;
  u64 dma_cmd_phys;
  u64 dma_resp_phys;

  /* Transport reference for queue notification. */
  virtio_hw_t *hw;
  u16 queue_index;
} virtio_vq_t;

/* Convert kernel virtual address to physical address. */
u64 virtio_virt_to_phys(void *vaddr);

/*
 * Create a virtqueue of the given size. Allocates page-aligned memory for
 * the descriptor table, available ring, and used ring. Returns 0 on success.
 */
int virtio_vq_create(virtio_vq_t *vq, u16 queue_size);

/* Bind a virtqueue to its transport and queue index (needed for
 * device notification after kick). */
void virtio_vq_bind(virtio_vq_t *vq, virtio_hw_t *hw, u16 queue_index);

/* Destroy a virtqueue (frees backing memory). */
void virtio_vq_destroy(virtio_vq_t *vq);

/*
 * Allocate a chain of `n` descriptors. Returns the head descriptor index
 * or (u16)-1 if not enough free descriptors.
 */
u16 virtio_vq_alloc_chain(virtio_vq_t *vq, u16 n);

/*
 * Set up a descriptor at index `idx` with the given physical address,
 * length, and flags. If VIRTQ_DESC_F_NEXT is set, the next field is
 * populated with `next_idx`.
 */
void virtio_vq_set_desc(virtio_vq_t *vq, u16 idx, u64 addr, u32 len,
                        u16 flags, u16 next_idx);

/*
 * Submit a buffer chain to the device. Pushes the head descriptor index
 * into the available ring and advances avail->idx. Returns the available
 * ring position used.
 */
void virtio_vq_kick(virtio_vq_t *vq, u16 head_idx);

/*
 * Poll the used ring for a completed buffer. Returns the head descriptor
 * index of the next used buffer, or (u16)-1 if nothing is available.
 * Also advances used_idx.
 */
u16 virtio_vq_poll(virtio_vq_t *vq);

/*
 * Free a chain of descriptors starting at `head_idx`. Walks the next
 * pointers and returns all descriptors to the free list.
 */
void virtio_vq_free_chain(virtio_vq_t *vq, u16 head_idx);

/*
 * Send a command + receive a response on a virtqueue. This is a convenience
 * wrapper that allocates descriptors, chains them (out descriptors for cmd,
 * in descriptors for response), kicks the queue, polls for completion, and
 * frees the chain. Returns 0 on success, -1 on timeout or error.
 *
 * `cmd` / `cmd_size` : command data (device-readable)
 * `resp` / `resp_size`: response buffer (device-writable)
 */
int virtio_vq_send_recv(virtio_vq_t *vq, const void *cmd, u32 cmd_size,
                        void *resp, u32 resp_size);

#endif
