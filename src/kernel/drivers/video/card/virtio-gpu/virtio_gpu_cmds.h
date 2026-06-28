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
 * virtio-gpu command structures and protocol constants.
 *
 * All commands and responses on the controlq and cursorq share a fixed
 * header (virtio_gpu_ctrl_hdr). The type field identifies the command
 * (VIRTIO_GPU_CMD_*) or response (VIRTIO_GPU_RESP_*). Commands are sent
 * as device-readable descriptors; responses as device-writable descriptors.
 */

#ifndef VIRTIO_GPU_CMDS_H
#define VIRTIO_GPU_CMDS_H

#include <mlibc/mlibc.h>

/* --- 2D commands --- */
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO     0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D   0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF       0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT          0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH       0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D  0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING  0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING  0x107

/* --- Cursor commands --- */
#define VIRTIO_GPU_CMD_UPDATE_CURSOR        0x0300
#define VIRTIO_GPU_CMD_MOVE_CURSOR          0x0301

/* --- Success responses --- */
#define VIRTIO_GPU_RESP_OK_NODATA           0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO     0x1101

/* --- Error responses --- */
#define VIRTIO_GPU_RESP_ERR_UNSPEC          0x1200
#define VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY   0x1201
#define VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID  0x1202
#define VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID 0x1203
#define VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID  0x1204
#define VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER   0x1205

#define VIRTIO_GPU_FLAG_FENCE 1

/* --- Pixel formats --- */
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM  1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM  2
#define VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM  3
#define VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM  4
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM  67

#define VIRTIO_GPU_MAX_SCANOUTS 16

/* --- On-wire structures --- */

typedef struct {
  u32 type;
  u32 flags;
  u64 fence_id;
  u32 ctx_id;
  u8 ring_idx;
  u8 padding[3];
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

typedef struct {
  u32 x;
  u32 y;
  u32 width;
  u32 height;
} __attribute__((packed)) virtio_gpu_rect_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  u32 scanout;
  u32 padding;
} __attribute__((packed)) virtio_gpu_get_edid_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  struct {
    virtio_gpu_rect_t r;
    u32 enabled;
    u32 flags;
  } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute__((packed)) virtio_gpu_resp_display_info_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  u32 resource_id;
  u32 format;
  u32 width;
  u32 height;
} __attribute__((packed)) virtio_gpu_resource_create_2d_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  u32 resource_id;
  u32 padding;
} __attribute__((packed)) virtio_gpu_resource_unref_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  virtio_gpu_rect_t r;
  u32 scanout_id;
  u32 resource_id;
} __attribute__((packed)) virtio_gpu_set_scanout_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  virtio_gpu_rect_t r;
  u32 resource_id;
  u32 padding;
} __attribute__((packed)) virtio_gpu_resource_flush_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  virtio_gpu_rect_t r;
  u64 offset;
  u32 resource_id;
  u32 padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  u32 resource_id;
  u32 nr_entries;
} __attribute__((packed)) virtio_gpu_resource_attach_backing_t;

typedef struct {
  u64 addr;
  u32 length;
  u32 padding;
} __attribute__((packed)) virtio_gpu_mem_entry_t;

typedef struct {
  virtio_gpu_ctrl_hdr_t hdr;
  u32 resource_id;
  u32 padding;
} __attribute__((packed)) virtio_gpu_resource_detach_backing_t;

/* --- High-level command API --- */

#include <kernel/drivers/video/card/virtio-gpu/virtio_hw.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio_queue.h>

/*
 * All high-level commands send a request on the controlq and wait for the
 * response. They return 0 on success (VIRTIO_GPU_RESP_OK_*) or -1 on error.
 */

/* Query display configuration from the device. */
int virtio_gpu_cmd_get_display_info(virtio_hw_t *hw, virtio_vq_t *vq,
                                    virtio_gpu_resp_display_info_t *info);

/* Create a 2D resource on the host. */
int virtio_gpu_cmd_resource_create_2d(virtio_hw_t *hw, virtio_vq_t *vq,
                                      u32 resource_id, u32 format,
                                      u32 width, u32 height);

/* Destroy a resource. */
int virtio_gpu_cmd_resource_unref(virtio_hw_t *hw, virtio_vq_t *vq,
                                  u32 resource_id);

/* Link a resource to a scanout at the given rectangle. */
int virtio_gpu_cmd_set_scanout(virtio_hw_t *hw, virtio_vq_t *vq,
                               u32 scanout_id, u32 resource_id,
                               u32 x, u32 y, u32 width, u32 height);

/* Flush a rectangle of a resource to the display. */
int virtio_gpu_cmd_resource_flush(virtio_hw_t *hw, virtio_vq_t *vq,
                                  u32 resource_id,
                                  u32 x, u32 y, u32 width, u32 height);

/* Transfer a rectangle from guest backing to host resource. */
int virtio_gpu_cmd_transfer_to_host_2d(virtio_hw_t *hw, virtio_vq_t *vq,
                                       u32 resource_id,
                                       u32 x, u32 y, u32 width, u32 height,
                                       u64 offset);

/* Attach guest physical pages as backing store for a resource. */
int virtio_gpu_cmd_attach_backing(virtio_hw_t *hw, virtio_vq_t *vq,
                                  u32 resource_id,
                                  virtio_gpu_mem_entry_t *entries,
                                  u32 nr_entries);

/* Detach backing store from a resource. */
int virtio_gpu_cmd_detach_backing(virtio_hw_t *hw, virtio_vq_t *vq,
                                  u32 resource_id);

#endif
