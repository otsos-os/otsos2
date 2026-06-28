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

#include <kernel/drivers/video/card/virtio-gpu/virtio_queue.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio_hw.h>
#include <kernel/mm/vm/pmap.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define VIRTIO_VQ_POLL_LIMIT 2000000

u64 virtio_virt_to_phys(void *vaddr) {
  u64 page_virt = (u64)vaddr & ~((u64)PAGE_SIZE - 1);
  u64 page_phys = pmap_extract(page_virt);
  if (page_phys == 0) {
    return (u64)vaddr - KERNEL_VMA;
  }
  return page_phys | ((u64)vaddr & (PAGE_SIZE - 1));
}

/* Calculate memory needed for each ring. */
static u32 desc_table_size(u16 qsize) {
  return (u32)qsize * sizeof(virtq_desc_t);
}

static u32 avail_ring_size(u16 qsize) {
  return sizeof(u16) * 2 + (u32)qsize * sizeof(u16);
}

static u32 used_ring_size(u16 qsize) {
  return sizeof(u16) * 2 + (u32)qsize * sizeof(virtq_used_elem_t);
}

int virtio_vq_create(virtio_vq_t *vq, u16 queue_size) {
  if (!vq || queue_size == 0) {
    return -1;
  }

  memset(vq, 0, sizeof(*vq));

  u32 desc_sz = desc_table_size(queue_size);
  u32 avail_sz = avail_ring_size(queue_size);
  u32 used_sz = used_ring_size(queue_size);

  /* Align: desc table 16-byte, avail 2-byte, used 4-byte. We page-align
   * everything for simplicity and to guarantee the device can access them. */
  u32 total = desc_sz + 16 + avail_sz + 16 + used_sz + 16;
  total = (total + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  void *mem = kmem_alloc_aligned(total, PAGE_SIZE);
  if (!mem) {
    com1_write_string("[VIRTQ] alloc failed\n");
    return -1;
  }
  memset(mem, 0, total);

  u64 mem_phys = virtio_virt_to_phys(mem);

  /* Lay out the three rings within the allocated region. */
  u32 offset = 0;

  vq->desc = (virtq_desc_t *)((u8 *)mem + offset);
  vq->phys_desc = mem_phys + offset;
  offset += (desc_sz + 15) & ~15u;

  vq->avail = (virtq_avail_t *)((u8 *)mem + offset);
  vq->phys_avail = mem_phys + offset;
  offset += (avail_sz + 15) & ~15u;

  vq->used = (virtq_used_t *)((u8 *)mem + offset);
  vq->phys_used = mem_phys + offset;

  vq->size = queue_size;
  vq->backing = mem;
  vq->backing_size = total;

  /* Allocate DMA scratch buffers (page-aligned, identity-mapped). */
  vq->dma_cmd = kmem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
  vq->dma_resp = kmem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
  if (!vq->dma_cmd || !vq->dma_resp) {
    com1_write_string("[VIRTQ] DMA scratch alloc failed\n");
    kmem_free(mem);
    if (vq->dma_cmd) kmem_free(vq->dma_cmd);
    if (vq->dma_resp) kmem_free(vq->dma_resp);
    memset(vq, 0, sizeof(*vq));
    return -1;
  }
  vq->dma_cmd_phys = virtio_virt_to_phys(vq->dma_cmd);
  vq->dma_resp_phys = virtio_virt_to_phys(vq->dma_resp);

  /* Build the free list: each descriptor's next points to the next one. */
  for (u16 i = 0; i < queue_size; i++) {
    vq->desc[i].next = (i + 1 < queue_size) ? (u16)(i + 1) : 0;
  }
  vq->free_head = 0;
  vq->num_free = queue_size;
  vq->avail_idx = 0;
  vq->used_idx = 0;

  return 0;
}

void virtio_vq_destroy(virtio_vq_t *vq) {
  if (!vq || !vq->backing) {
    return;
  }
  if (vq->dma_cmd) {
    kmem_free(vq->dma_cmd);
  }
  if (vq->dma_resp) {
    kmem_free(vq->dma_resp);
  }
  kmem_free(vq->backing);
  memset(vq, 0, sizeof(*vq));
}

void virtio_vq_bind(virtio_vq_t *vq, virtio_hw_t *hw, u16 queue_index) {
  if (!vq) {
    return;
  }
  vq->hw = hw;
  vq->queue_index = queue_index;
}

u16 virtio_vq_alloc_chain(virtio_vq_t *vq, u16 n) {
  if (!vq || n == 0 || vq->num_free < n) {
    return (u16)-1;
  }

  u16 head = vq->free_head;
  u16 prev = head;
  for (u16 i = 0; i < n; i++) {
    prev = vq->free_head;
    vq->free_head = vq->desc[vq->free_head].next;
    vq->num_free--;
  }
  return head;
}

void virtio_vq_set_desc(virtio_vq_t *vq, u16 idx, u64 addr, u32 len,
                        u16 flags, u16 next_idx) {
  if (!vq || idx >= vq->size) {
    return;
  }
  vq->desc[idx].addr = addr;
  vq->desc[idx].len = len;
  vq->desc[idx].flags = flags;
  vq->desc[idx].next = next_idx;
}

void virtio_vq_kick(virtio_vq_t *vq, u16 head_idx) {
  if (!vq) {
    return;
  }
  u16 avail_pos = vq->avail_idx % vq->size;
  vq->avail->ring[avail_pos] = head_idx;
  __asm__ volatile("" ::: "memory");
  vq->avail->idx = vq->avail_idx + 1;
  __asm__ volatile("" ::: "memory");
  vq->avail_idx++;
}

u16 virtio_vq_poll(virtio_vq_t *vq) {
  if (!vq) {
    return (u16)-1;
  }
  for (u64 i = 0; i < VIRTIO_VQ_POLL_LIMIT; i++) {
    __asm__ volatile("" ::: "memory");
    if (vq->used->idx != vq->used_idx) {
      u16 used_pos = vq->used_idx % vq->size;
      u16 head = (u16)vq->used->ring[used_pos].id;
      vq->used_idx++;
      return head;
    }
    __asm__ volatile("pause");
  }
  return (u16)-1;
}

void virtio_vq_free_chain(virtio_vq_t *vq, u16 head_idx) {
  if (!vq || head_idx >= vq->size) {
    return;
  }
  u16 idx = head_idx;
  while (1) {
    u16 flags = vq->desc[idx].flags;
    u16 next = vq->desc[idx].next;

    vq->desc[idx].next = vq->free_head;
    vq->desc[idx].flags = 0;
    vq->desc[idx].addr = 0;
    vq->desc[idx].len = 0;
    vq->free_head = idx;
    vq->num_free++;

    if (!(flags & VIRTQ_DESC_F_NEXT)) {
      break;
    }
    if (next >= vq->size) {
      break;
    }
    idx = next;
  }
}

int virtio_vq_send_recv(virtio_vq_t *vq, const void *cmd, u32 cmd_size,
                        void *resp, u32 resp_size) {
  if (!vq || !cmd || !resp || cmd_size == 0 || resp_size == 0) {
    return -1;
  }
  if (cmd_size > PAGE_SIZE || resp_size > PAGE_SIZE) {
    com1_printf("[VIRTQ] cmd/resp too large: cmd=%u resp=%u\n", cmd_size,
                resp_size);
    return -1;
  }

  /* We need at most 2 descriptors: one for cmd (out), one for resp (in). */
  if (vq->num_free < 2) {
    com1_write_string("[VIRTQ] not enough free descriptors\n");
    return -1;
  }

  u16 head = virtio_vq_alloc_chain(vq, 2);
  if (head == (u16)-1) {
    return -1;
  }

  /* Copy command data to the DMA scratch buffer (physically contiguous). */
  memcpy(vq->dma_cmd, cmd, cmd_size);
  memset(vq->dma_resp, 0, resp_size);

  u16 desc_cmd = head;
  u16 desc_resp = vq->desc[head].next;

  /* Command descriptor: device-readable, chains to response. */
  virtio_vq_set_desc(vq, desc_cmd, vq->dma_cmd_phys, cmd_size,
                     VIRTQ_DESC_F_NEXT, desc_resp);
  /* Response descriptor: device-writable, end of chain. */
  virtio_vq_set_desc(vq, desc_resp, vq->dma_resp_phys, resp_size,
                     VIRTQ_DESC_F_WRITE, 0);

  /* Memory barrier: ensure descriptor writes are visible before notification. */
  __asm__ volatile("" ::: "memory");

  virtio_vq_kick(vq, head);

  /* Notify the device that buffers are available on this queue. */
  if (vq->hw) {
    virtio_hw_notify_queue(vq->hw, vq->queue_index);
  }

  u16 used_head = virtio_vq_poll(vq);
  if (used_head == (u16)-1) {
    com1_printf("[VIRTQ] timeout: cmd_size=%u avail_idx=%u used_idx=%u used->idx=%u\n",
                cmd_size, vq->avail_idx, vq->used_idx, vq->used->idx);
    virtio_vq_free_chain(vq, head);
    return -1;
  }

  /* Copy response from DMA scratch buffer back to caller. */
  memcpy(resp, vq->dma_resp, resp_size);

  virtio_vq_free_chain(vq, used_head);
  return 0;
}
