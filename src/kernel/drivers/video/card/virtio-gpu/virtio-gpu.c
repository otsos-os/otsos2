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

#include <kernel/drivers/video/card/virtio-gpu/virtio-gpu.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio_hw.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio_queue.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.h>
#include <drm/drm.h>
#include <drm/kms/crtc.h>
#include <kernel/pci/pci.h>
#include <kernel/pci/utils/bar.h>
#include <kernel/mm/vm/pmap.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define VIRTIO_GPU_QUEUE_SIZE 64
#define VIRTIO_GPU_RESOURCE_ID 1
#define VIRTIO_GPU_SCANOUT_ID  0
#define VIRTIO_GPU_FALLBACK_W  1024
#define VIRTIO_GPU_FALLBACK_H  768
#define VIRTIO_GPU_BPP         32


typedef struct {
  virtio_hw_t hw;
  virtio_vq_t vqs[VIRTIO_GPU_NUM_VQS];
  u32 width;
  u32 height;
  u32 pitch;
  u8  bpp;
  u8 *backing;
  u64 backing_size;
  u32 backing_resource_id;

  int hw_ready;
  int display_ready;
} virtio_gpu_state_t;

static virtio_gpu_state_t g_state;


static int virtio_gpu_setup_queues(virtio_gpu_state_t *st) {
  virtio_hw_t *hw = &st->hw;

  for (int i = 0; i < VIRTIO_GPU_NUM_VQS; i++) {
    virtio_hw_select_queue(hw, (u16)i);
    u16 qsize = virtio_hw_get_queue_size(hw);
    if (qsize == 0) {
      qsize = VIRTIO_GPU_QUEUE_SIZE;
    }
    if (qsize > VIRTIO_GPU_QUEUE_SIZE) {
      qsize = VIRTIO_GPU_QUEUE_SIZE;
    }

    if (virtio_vq_create(&st->vqs[i], qsize) != 0) {
      com1_printf("[VIRTIO_GPU] vq %d create failed\n", i);
      return -1;
    }

    virtio_vq_bind(&st->vqs[i], hw, (u16)i);

    virtio_hw_set_queue_size(hw, qsize);
    virtio_hw_set_queue_desc(hw, st->vqs[i].phys_desc);
    virtio_hw_set_queue_driver(hw, st->vqs[i].phys_avail);
    virtio_hw_set_queue_device(hw, st->vqs[i].phys_used);
    virtio_hw_enable_queue(hw);

    com1_printf("[VIRTIO_GPU] vq %d: size=%u\n", i, qsize);
  }
  return 0;
}

static int virtio_gpu_query_display(virtio_gpu_state_t *st) {
  u32 crtc_w = drm_crtc_get_width();
  u32 crtc_h = drm_crtc_get_height();

  if (crtc_w > 0 && crtc_h > 0) {
    st->width = crtc_w;
    st->height = crtc_h;
    com1_printf("[VIRTIO_GPU] using KMS mode: %ux%u\n", st->width,
                st->height);
  } else {
    virtio_gpu_resp_display_info_t info;
    int rc = virtio_gpu_cmd_get_display_info(
        &st->hw, &st->vqs[VIRTIO_GPU_CONTROLQ], &info);
    if (rc != 0) {
      com1_write_string(
          "[VIRTIO_GPU] GET_DISPLAY_INFO failed\n");
      st->width = VIRTIO_GPU_FALLBACK_W;
      st->height = VIRTIO_GPU_FALLBACK_H;
      return 0;
    }

    for (int i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
      if (info.pmodes[i].enabled) {
        st->width = info.pmodes[i].r.width;
        st->height = info.pmodes[i].r.height;
        com1_printf("[VIRTIO_GPU] device display %d: %ux%u\n", i,
                    st->width, st->height);
        return 0;
      }
    }

    com1_write_string("[VIRTIO_GPU] scanout doesnt enabled\n");
    st->width = VIRTIO_GPU_FALLBACK_W;
    st->height = VIRTIO_GPU_FALLBACK_H;
  }
  return 0;
}

static int virtio_gpu_pci_probe(pci_device_t *dev, const pci_match_t *match) {
  (void)match;
  if (!dev) {
    return -1;
  }

  com1_write_string("[VIRTIO_GPU] probing PCI\n");
  memset(&g_state, 0, sizeof(g_state));
  g_state.backing_resource_id = VIRTIO_GPU_RESOURCE_ID;
  if (virtio_hw_init(&g_state.hw, dev) != 0) {
    com1_write_string("[VIRTIO_GPU] transport init failed\n");
    return -1;
  }
  if (virtio_gpu_setup_queues(&g_state) != 0) {
    com1_write_string("[VIRTIO_GPU] queue setup failed\n");
    return -1;
  }

  virtio_hw_set_status(&g_state.hw, VIRTIO_STATUS_ACKNOWLEDGE |
                        VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
                        VIRTIO_STATUS_DRIVER_OK);

  if (virtio_gpu_query_display(&g_state) != 0) {
    com1_write_string("[VIRTIO_GPU] display query failed\n");
    return -1;
  }

  g_state.pitch = g_state.width * (VIRTIO_GPU_BPP / 8);
  g_state.bpp = VIRTIO_GPU_BPP;
  g_state.hw_ready = 1;

  com1_printf("[VIRTIO_GPU] hardware ready: %ux%u x%u bpp\n",
              g_state.width, g_state.height, g_state.bpp);
  return 0;
}

static pci_match_t virtio_gpu_matches[] = {
    {.vendor_id = VIRTIO_VENDOR_ID,
     .device_id = VIRTIO_GPU_DEVICE_ID,
     .class_code = PCI_ANY_CLASS,
     .subclass = PCI_ANY_SUBCLASS,
     .prog_if = PCI_ANY_PROGIF},
};

static pci_driver_t virtio_gpu_pci_driver = {
    .name = "virtio-gpu",
    .matches = virtio_gpu_matches,
    .match_count = 1,
    .probe = virtio_gpu_pci_probe,
    .remove = NULL,
};

int drm_virtio_gpu_pci_register(void) {
  return pci_register_driver(&virtio_gpu_pci_driver);
}

int drm_virtio_gpu_is_ready(void) {
  return g_state.hw_ready;
}

static int attach_backing_store(virtio_gpu_state_t *st) {
  u64 fb_size = (u64)st->pitch * st->height;
  u64 aligned = (fb_size + PAGE_SIZE - 1) & ~((u64)PAGE_SIZE - 1);

  st->backing = (u8 *)kmem_alloc_aligned(aligned, PAGE_SIZE);
  if (!st->backing) {
    com1_write_string("[VIRTIO_GPU] backing alloc failed\n");
    return -1;
  }
  memset(st->backing, 0, aligned);
  st->backing_size = aligned;

  u64 backing_phys = virtio_virt_to_phys(st->backing);
  com1_printf("[VIRTIO_GPU] backing: virt=%p phys=%p size=%u\n",
              st->backing, (void *)backing_phys, (u32)aligned);

  virtio_gpu_mem_entry_t entry;
  entry.addr = backing_phys;
  entry.length = (u32)aligned;
  entry.padding = 0;

  int rc = virtio_gpu_cmd_attach_backing(&st->hw,
                                         &st->vqs[VIRTIO_GPU_CONTROLQ],
                                         st->backing_resource_id,
                                         &entry, 1);
  if (rc != 0) {
    com1_write_string("[VIRTIO_GPU] attach backing failed\n");
    kmem_free(st->backing);
    st->backing = NULL;
    return -1;
  }

  com1_write_string("[VIRTIO_GPU] backing attached\n");
  return 0;
}

int drm_virtio_gpu_display_init(void) {
  virtio_gpu_state_t *st = &g_state;

  if (!st->hw_ready) {
    com1_write_string("[VIRTIO_GPU] display_init: hw not ready\n");
    return -1;
  }
  if (st->display_ready) {
    return 0;
  }

  com1_write_string("[VIRTIO_GPU] display_init: creating 2D resource\n");
  if (virtio_gpu_cmd_resource_create_2d(&st->hw,
                                        &st->vqs[VIRTIO_GPU_CONTROLQ],
                                        st->backing_resource_id,
                                        VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM,
                                        st->width, st->height) != 0) {
    com1_write_string("[VIRTIO_GPU] resource create failed\n");
    return -1;
  }

  com1_write_string("[VIRTIO_GPU] display_init: attaching backing\n");
  if (attach_backing_store(st) != 0) {
    return -1;
  }

  com1_write_string("[VIRTIO_GPU] display_init: setting scanout\n");
  if (virtio_gpu_cmd_set_scanout(&st->hw,
                                 &st->vqs[VIRTIO_GPU_CONTROLQ],
                                 VIRTIO_GPU_SCANOUT_ID,
                                 st->backing_resource_id,
                                 0, 0, st->width, st->height) != 0) {
    com1_write_string("[VIRTIO_GPU] set scanout failed\n");
    return -1;
  }

  drm_crtc_set_mode_geometry(st->width, st->height, st->pitch, st->bpp, 0);

  com1_write_string("[VIRTIO_GPU] display_init: initial transfer+flush\n");
  if (virtio_gpu_cmd_transfer_to_host_2d(&st->hw,
                                      &st->vqs[VIRTIO_GPU_CONTROLQ],
                                      st->backing_resource_id,
                                      0, 0, st->width, st->height, 0) != 0) {
    com1_write_string("[VIRTIO_GPU] initial transfer failed\n");
    return -1;
  }
  if (virtio_gpu_cmd_resource_flush(&st->hw,
                                 &st->vqs[VIRTIO_GPU_CONTROLQ],
                                 st->backing_resource_id,
                                 0, 0, st->width, st->height) != 0) {
    com1_write_string("[VIRTIO_GPU] initial flush failed\n");
    return -1;
  }

  st->display_ready = 1;
  com1_write_string("[VIRTIO_GPU] display ready\n");
  return 0;
}

void drm_virtio_gpu_display_shutdown(void) {
  virtio_gpu_state_t *st = &g_state;
  if (!st->display_ready) {
    return;
  }

  virtio_gpu_cmd_set_scanout(&st->hw, &st->vqs[VIRTIO_GPU_CONTROLQ],
                             VIRTIO_GPU_SCANOUT_ID, 0,
                             0, 0, 0, 0);

  virtio_gpu_cmd_detach_backing(&st->hw, &st->vqs[VIRTIO_GPU_CONTROLQ],
                                st->backing_resource_id);
  virtio_gpu_cmd_resource_unref(&st->hw, &st->vqs[VIRTIO_GPU_CONTROLQ],
                                st->backing_resource_id);

  if (st->backing) {
    kmem_free(st->backing);
    st->backing = NULL;
  }

  st->display_ready = 0;
}

/* --- DRM driver interface --- */

static int vgpu_drm_probe(const void *boot_info) {
  (void)boot_info;
  return g_state.hw_ready ? 0 : -1;
}

static int vgpu_drm_init(const void *boot_info) {
  (void)boot_info;
  return drm_virtio_gpu_display_init();
}

static int vgpu_drm_present(const drm_framebuffer_t *src) {
  virtio_gpu_state_t *st = &g_state;
  if (!st->display_ready || !src || !src->gem || !src->gem->data) {
    return -1;
  }

  /* Copy the framebuffer data into the backing store. */
  u32 line_bytes = src->width * (u32)(src->bpp / 8);
  u32 copy = line_bytes < src->pitch ? line_bytes : src->pitch;
  if (copy > st->pitch) {
    copy = st->pitch;
  }
  for (u32 y = 0; y < src->height; y++) {
    memcpy(st->backing + (u64)y * st->pitch,
           src->gem->data + (u64)y * src->pitch, copy);
  }

  /* Transfer the entire backing to the host resource, then flush. */
  if (virtio_gpu_cmd_transfer_to_host_2d(&st->hw,
                                         &st->vqs[VIRTIO_GPU_CONTROLQ],
                                         st->backing_resource_id,
                                         0, 0, st->width, st->height, 0) != 0) {
    return -1;
  }
  if (virtio_gpu_cmd_resource_flush(&st->hw,
                                    &st->vqs[VIRTIO_GPU_CONTROLQ],
                                    st->backing_resource_id,
                                    0, 0, st->width, st->height) != 0) {
    return -1;
  }
  return 0;
}

static int vgpu_drm_present_rect(const drm_framebuffer_t *src, u32 x, u32 y,
                                 u32 w, u32 h) {
  virtio_gpu_state_t *st = &g_state;
  if (!st->display_ready || !src || !src->gem || !src->gem->data) {
    return -1;
  }
  if (x >= st->width || y >= st->height || w == 0 || h == 0) {
    return 0;
  }

  u32 x2 = x + w, y2 = y + h;
  if (x2 > st->width) x2 = st->width;
  if (y2 > st->height) y2 = st->height;
  u32 rw = x2 - x;
  u32 rh = y2 - y;

  u32 bpp_bytes = (u32)(src->bpp / 8);
  u32 src_line_bytes = rw * bpp_bytes;

  /* Copy only the dirty rectangle into the backing store. */
  for (u32 ry = 0; ry < rh; ry++) {
    memcpy(st->backing + (u64)(y + ry) * st->pitch + (u64)x * bpp_bytes,
           src->gem->data + (u64)(y + ry) * src->pitch + (u64)x * bpp_bytes,
           src_line_bytes);
  }

  /* Transfer and flush only the dirty rectangle. */
  u64 offset = (u64)y * st->pitch + (u64)x * bpp_bytes;
  if (virtio_gpu_cmd_transfer_to_host_2d(&st->hw,
                                         &st->vqs[VIRTIO_GPU_CONTROLQ],
                                         st->backing_resource_id,
                                         x, y, rw, rh, offset) != 0) {
    return -1;
  }
  if (virtio_gpu_cmd_resource_flush(&st->hw,
                                    &st->vqs[VIRTIO_GPU_CONTROLQ],
                                    st->backing_resource_id,
                                    x, y, rw, rh) != 0) {
    return -1;
  }
  return 0;
}

static void vgpu_drm_shutdown(void) {
  drm_virtio_gpu_display_shutdown();
}

static const drm_driver_t g_virtio_gpu_driver = {
    .name = "virtio-gpu",
    .priority = 50,
    .probe = vgpu_drm_probe,
    .init = vgpu_drm_init,
    .present = vgpu_drm_present,
    .present_rect = vgpu_drm_present_rect,
    .shutdown = vgpu_drm_shutdown,
};

const drm_driver_t *drm_virtio_gpu_driver_get(void) {
  return &g_virtio_gpu_driver;
}
