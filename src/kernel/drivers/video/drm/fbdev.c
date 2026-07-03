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
 * and/or other materials provided with the distribution.
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

#include <drm/drm.h>
#include <drm/fbdev.h>
#include <drm/kms/crtc.h>
#include <drm/kms/plane.h>
#include <drm/kms/framebuffer.h>
#include <mm/vm/pmap.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define PAGE_SIZE 4096

static u8 *g_hw_base;
static u32 g_hw_pitch;
static u32 g_hw_width;
static u32 g_hw_height;
static u8 g_hw_bpp;

static int fbdev_probe(const void *boot_info) {
  const drm_fbdev_boot_t *boot = (const drm_fbdev_boot_t *)boot_info;
  if (!boot) {
    return -1;
  }
  if (!boot->hw_address || !boot->pitch || !boot->width || !boot->height ||
      !boot->bpp) {
    return -1;
  }
  return 0;
}

static void fbdev_map_hw(u64 addr, u64 size) {
  u64 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
  for (u64 i = 0; i < pages; i++) {
    pmap_enter(addr + i * PAGE_SIZE, addr + i * PAGE_SIZE,
               PTE_PRESENT | PTE_RW);
  }
}

static int fbdev_init(const void *boot_info) {
  const drm_fbdev_boot_t *boot = (const drm_fbdev_boot_t *)boot_info;
  if (fbdev_probe(boot) != 0) {
    return -1;
  }

  u64 size = (u64)boot->pitch * boot->height;
  fbdev_map_hw(boot->hw_address, size);

  g_hw_base = (u8 *)boot->hw_address;
  g_hw_pitch = boot->pitch;
  g_hw_width = boot->width;
  g_hw_height = boot->height;
  g_hw_bpp = boot->bpp;

  /* Publish geometry so the KMS layer knows the active mode. */
  drm_crtc_set_mode_geometry(boot->width, boot->height, boot->pitch,
                             boot->bpp, boot->hw_address);

  drivers_log("[FBDEV] hw fb mapped: %ux%u\n", boot->width, boot->height);
  return 0;
}

static void fbdev_blit_full(const drm_framebuffer_t *src, u32 src_y) {
  u32 line_bytes, copy, y;

  line_bytes = src->width * (u32)(src->bpp / 8);
  copy = line_bytes < src->pitch ? line_bytes : src->pitch;
  if (copy > g_hw_pitch) {
    copy = g_hw_pitch;
  }
  for (y = 0; y < src->height; y++) {
    memcpy(g_hw_base + (u64)y * g_hw_pitch,
           src->gem->data + (u64)(y + src_y) * src->pitch, copy);
  }
}

static void fbdev_blit_rect(const drm_framebuffer_t *src, u32 src_y,
                            u32 x, u32 y, u32 w, u32 h) {
  u32 bpp_bytes, x2, y2, copy, ry;

  if (x >= g_hw_width || y >= g_hw_height || w == 0 || h == 0) {
    return;
  }
  bpp_bytes = (u32)(src->bpp / 8);
  x2 = x + w;
  y2 = y + h;
  if (x2 > g_hw_width) {
    x2 = g_hw_width;
  }
  if (y2 > g_hw_height) {
    y2 = g_hw_height;
  }
  copy = (x2 - x) * bpp_bytes;
  for (ry = y; ry < y2; ry++) {
    memcpy(g_hw_base + (u64)ry * g_hw_pitch + (u64)x * bpp_bytes,
           src->gem->data + (u64)(ry + src_y) * src->pitch +
               (u64)x * bpp_bytes,
           copy);
  }
}

static int fbdev_atomic_commit(const drm_kms_state_t *state) {
  drm_framebuffer_t *src;
  drm_id_t fb_id;
  u32 dx, dy, dw, dh;

  if (!state || !g_hw_base) {
    return -1;
  }
  /* Use the primary plane for scanout. */
  fb_id = (drm_id_t)state->plane_props[0][DRM_PROP_PLANE_FB_ID];
  if (fb_id == DRM_ID_NONE) {
    return 0;
  }
  src = drm_framebuffer_get(fb_id);
  if (!src || !src->gem || !src->gem->data) {
    return -1;
  }
  if (src->width != g_hw_width || src->height != g_hw_height ||
      src->bpp != g_hw_bpp) {
    return -1;
  }
  u32 src_y = (u32)state->plane_props[0][DRM_PROP_PLANE_SRC_Y];

  dx = (u32)state->plane_props[0][DRM_PROP_PLANE_DIRTY_X];
  dy = (u32)state->plane_props[0][DRM_PROP_PLANE_DIRTY_Y];
  dw = (u32)state->plane_props[0][DRM_PROP_PLANE_DIRTY_W];
  dh = (u32)state->plane_props[0][DRM_PROP_PLANE_DIRTY_H];
  if (dw > 0 && dh > 0 && (dw < g_hw_width || dh < g_hw_height)) {
    fbdev_blit_rect(src, src_y, dx, dy, dw, dh);
  } else {
    fbdev_blit_full(src, src_y);
  }
  return 0;
}

static const drm_driver_t g_fbdev_driver = {
    .name = "fbdev",
    .priority = 10,
    .probe = fbdev_probe,
    .init = fbdev_init,
    .atomic_commit = fbdev_atomic_commit,
    .shutdown = NULL,
};

const drm_driver_t *drm_fbdev_driver_get(void) { return &g_fbdev_driver; }
