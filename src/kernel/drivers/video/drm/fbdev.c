/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/fbdev.h>
#include <drm/kms/crtc.h>
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

  drivers_log("[FBDEV] hw fb mapped: %ux%u\n",
      boot->width, boot->height);
  return 0;
}

static int fbdev_present(const drm_framebuffer_t *src) {
  if (!src || !src->gem || !src->gem->data || !g_hw_base) {
    return -1;
  }
  if (src->width != g_hw_width || src->height != g_hw_height ||
      src->bpp != g_hw_bpp) {
    return -1;
  }
  u32 line_bytes = src->width * (u32)(src->bpp / 8);
  u32 copy = line_bytes < src->pitch ? line_bytes : src->pitch;
  if (copy > g_hw_pitch) {
    copy = g_hw_pitch;
  }
  u32 src_y = src->src_y;
  for (u32 y = 0; y < src->height; y++) {
    memcpy(g_hw_base + y * g_hw_pitch,
           src->gem->data + (u64)(y + src_y) * src->pitch, copy);
  }
  return 0;
}

static int fbdev_present_rect(const drm_framebuffer_t *src, u32 x, u32 y,
                              u32 w, u32 h) {
  if (!src || !src->gem || !src->gem->data || !g_hw_base) {
    return -1;
  }
  if (x >= g_hw_width || y >= g_hw_height || w == 0 || h == 0) {
    return 0;
  }
  u32 bpp_bytes = (u32)(src->bpp / 8);
  u32 x2 = x + w, y2 = y + h;
  if (x2 > g_hw_width) x2 = g_hw_width;
  if (y2 > g_hw_height) y2 = g_hw_height;
  u32 copy = (x2 - x) * bpp_bytes;
  u32 src_y = src->src_y;
  for (u32 ry = y; ry < y2; ry++) {
    memcpy(g_hw_base + ry * g_hw_pitch + x * bpp_bytes,
           src->gem->data + (u64)(ry + src_y) * src->pitch + x * bpp_bytes,
           copy);
  }
  return 0;
}

static const drm_driver_t g_fbdev_driver = {
    .name = "fbdev",
    .priority = 10,
    .probe = fbdev_probe,
    .init = fbdev_init,
    .present = fbdev_present,
    .present_rect = fbdev_present_rect,
    .shutdown = NULL,
};

const drm_driver_t *drm_fbdev_driver_get(void) { return &g_fbdev_driver; }
