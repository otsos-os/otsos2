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

#include <kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static void init_hdr(virtio_gpu_ctrl_hdr_t *hdr, u32 type) {
  memset(hdr, 0, sizeof(*hdr));
  hdr->type = type;
}

static int check_response(virtio_gpu_ctrl_hdr_t *resp, u32 expected) {
  if (resp->type == expected) {
    return 0;
  }
  com1_printf("[VIRTIO_GPU] response type 0x%x (expected 0x%x)\n",
              resp->type, expected);
  return -1;
}

int virtio_gpu_cmd_get_display_info(virtio_hw_t *hw, virtio_vq_t *vq,
                                    virtio_gpu_resp_display_info_t *info) {
  virtio_gpu_ctrl_hdr_t req;
  init_hdr(&req, VIRTIO_GPU_CMD_GET_DISPLAY_INFO);

  memset(info, 0, sizeof(*info));
  int rc = virtio_vq_send_recv(vq, &req, sizeof(req), info, sizeof(*info));
  if (rc != 0) {
    return -1;
  }
  return check_response(&info->hdr, VIRTIO_GPU_RESP_OK_DISPLAY_INFO);
}

int virtio_gpu_cmd_resource_create_2d(virtio_hw_t *hw, virtio_vq_t *vq,
                                      u32 resource_id, u32 format,
                                      u32 width, u32 height) {
  virtio_gpu_resource_create_2d_t req;
  virtio_gpu_ctrl_hdr_t resp;
  init_hdr(&req.hdr, VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
  req.resource_id = resource_id;
  req.format = format;
  req.width = width;
  req.height = height;

  memset(&resp, 0, sizeof(resp));
  int rc = virtio_vq_send_recv(vq, &req, sizeof(req), &resp, sizeof(resp));
  if (rc != 0) {
    return -1;
  }
  return check_response(&resp, VIRTIO_GPU_RESP_OK_NODATA);
}

int virtio_gpu_cmd_resource_unref(virtio_hw_t *hw, virtio_vq_t *vq,
                                  u32 resource_id) {
  virtio_gpu_resource_unref_t req;
  virtio_gpu_ctrl_hdr_t resp;
  init_hdr(&req.hdr, VIRTIO_GPU_CMD_RESOURCE_UNREF);
  req.resource_id = resource_id;
  req.padding = 0;

  memset(&resp, 0, sizeof(resp));
  int rc = virtio_vq_send_recv(vq, &req, sizeof(req), &resp, sizeof(resp));
  if (rc != 0) {
    return -1;
  }
  return check_response(&resp, VIRTIO_GPU_RESP_OK_NODATA);
}

int virtio_gpu_cmd_set_scanout(virtio_hw_t *hw, virtio_vq_t *vq,
                               u32 scanout_id, u32 resource_id,
                               u32 x, u32 y, u32 width, u32 height) {
  virtio_gpu_set_scanout_t req;
  virtio_gpu_ctrl_hdr_t resp;
  init_hdr(&req.hdr, VIRTIO_GPU_CMD_SET_SCANOUT);
  req.r.x = x;
  req.r.y = y;
  req.r.width = width;
  req.r.height = height;
  req.scanout_id = scanout_id;
  req.resource_id = resource_id;

  memset(&resp, 0, sizeof(resp));
  int rc = virtio_vq_send_recv(vq, &req, sizeof(req), &resp, sizeof(resp));
  if (rc != 0) {
    return -1;
  }
  return check_response(&resp, VIRTIO_GPU_RESP_OK_NODATA);
}

int virtio_gpu_cmd_resource_flush(virtio_hw_t *hw, virtio_vq_t *vq,
                                  u32 resource_id,
                                  u32 x, u32 y, u32 width, u32 height) {
  virtio_gpu_resource_flush_t req;
  virtio_gpu_ctrl_hdr_t resp;
  init_hdr(&req.hdr, VIRTIO_GPU_CMD_RESOURCE_FLUSH);
  req.r.x = x;
  req.r.y = y;
  req.r.width = width;
  req.r.height = height;
  req.resource_id = resource_id;
  req.padding = 0;

  memset(&resp, 0, sizeof(resp));
  int rc = virtio_vq_send_recv(vq, &req, sizeof(req), &resp, sizeof(resp));
  if (rc != 0) {
    return -1;
  }
  return check_response(&resp, VIRTIO_GPU_RESP_OK_NODATA);
}

int virtio_gpu_cmd_transfer_to_host_2d(virtio_hw_t *hw, virtio_vq_t *vq,
                                       u32 resource_id,
                                       u32 x, u32 y, u32 width, u32 height,
                                       u64 offset) {
  virtio_gpu_transfer_to_host_2d_t req;
  virtio_gpu_ctrl_hdr_t resp;
  init_hdr(&req.hdr, VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
  req.r.x = x;
  req.r.y = y;
  req.r.width = width;
  req.r.height = height;
  req.offset = offset;
  req.resource_id = resource_id;
  req.padding = 0;

  memset(&resp, 0, sizeof(resp));
  int rc = virtio_vq_send_recv(vq, &req, sizeof(req), &resp, sizeof(resp));
  if (rc != 0) {
    return -1;
  }
  return check_response(&resp, VIRTIO_GPU_RESP_OK_NODATA);
}

/*
 * Attach backing uses a variable-length command: the attach_backing header
 * is followed by nr_entries mem_entry_t structs. All of this is device-
 * readable and goes in a single descriptor. The response is a ctrl_hdr.
 */
int virtio_gpu_cmd_attach_backing(virtio_hw_t *hw, virtio_vq_t *vq,
                                  u32 resource_id,
                                  virtio_gpu_mem_entry_t *entries,
                                  u32 nr_entries) {
  if (!entries || nr_entries == 0) {
    return -1;
  }

  u32 cmd_size = sizeof(virtio_gpu_resource_attach_backing_t) +
                 nr_entries * sizeof(virtio_gpu_mem_entry_t);
  u8 *cmd_buf = (u8 *)kmem_alloc(cmd_size);
  if (!cmd_buf) {
    return -1;
  }

  virtio_gpu_resource_attach_backing_t *cmd =
      (virtio_gpu_resource_attach_backing_t *)cmd_buf;
  init_hdr(&cmd->hdr, VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
  cmd->resource_id = resource_id;
  cmd->nr_entries = nr_entries;

  virtio_gpu_mem_entry_t *ents =
      (virtio_gpu_mem_entry_t *)(cmd_buf +
                                 sizeof(virtio_gpu_resource_attach_backing_t));
  memcpy(ents, entries, nr_entries * sizeof(virtio_gpu_mem_entry_t));

  virtio_gpu_ctrl_hdr_t resp;
  memset(&resp, 0, sizeof(resp));

  int rc = virtio_vq_send_recv(vq, cmd_buf, cmd_size, &resp, sizeof(resp));

  kmem_free(cmd_buf);
  if (rc != 0) {
    return -1;
  }
  return check_response(&resp, VIRTIO_GPU_RESP_OK_NODATA);
}

int virtio_gpu_cmd_detach_backing(virtio_hw_t *hw, virtio_vq_t *vq,
                                  u32 resource_id) {
  virtio_gpu_resource_detach_backing_t req;
  virtio_gpu_ctrl_hdr_t resp;
  init_hdr(&req.hdr, VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING);
  req.resource_id = resource_id;
  req.padding = 0;

  memset(&resp, 0, sizeof(resp));
  int rc = virtio_vq_send_recv(vq, &req, sizeof(req), &resp, sizeof(resp));
  if (rc != 0) {
    return -1;
  }
  return check_response(&resp, VIRTIO_GPU_RESP_OK_NODATA);
}
