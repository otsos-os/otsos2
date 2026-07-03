/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/fbdev.h>
#include <drm/init.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <mlibc/stdio.h>

static drm_fbdev_boot_t g_boot;
static int g_boot_ready;

const void *drm_fbdev_get_boot_info(void) {
  if (!g_boot_ready) {
    return NULL;
  }
  return &g_boot;
}

static int boot_init_common(const drm_fbdev_boot_t *boot,
                            const char *preferred) {
  if (!boot) {
    return -1;
  }
  const drm_driver_t *drv = drm_driver_select(boot, preferred);
  if (!drv) {
    drivers_log("[DRM] no suitable driver\n");
    return -1;
  }
  g_boot = *boot;
  g_boot_ready = 1;
  return drm_init(drv, boot);
}

int drm_boot_init_mb2(multiboot2_info_t *mb_info, const char *preferred) {
  multiboot2_tag_framebuffer_t *fb_tag =
      (multiboot2_tag_framebuffer_t *)multiboot2_find_tag(
          mb_info, MULTIBOOT2_TAG_TYPE_FRAMEBUFFER);
  if (!fb_tag) {
    drivers_log("[DRM] MB2 framebuffer tag not found\n");
    return -1;
  }
  drm_fbdev_boot_t boot = {
      .hw_address = (u64)fb_tag->framebuffer_addr,
      .pitch = fb_tag->framebuffer_pitch,
      .width = fb_tag->framebuffer_width,
      .height = fb_tag->framebuffer_height,
      .bpp = fb_tag->framebuffer_bpp,
  };
  return boot_init_common(&boot, preferred);
}

int drm_boot_init_mb1(multiboot_info_t *mb_info, const char *preferred) {
  if ((mb_info->flags & MULTIBOOT_FLAG_FRAMEBUFFER) == 0 ||
      mb_info->framebuffer_addr == 0) {
    drivers_log("[DRM] MB1 framebuffer info not available\n");
    return -1;
  }
  drm_fbdev_boot_t boot = {
      .hw_address = (u64)mb_info->framebuffer_addr,
      .pitch = mb_info->framebuffer_pitch,
      .width = mb_info->framebuffer_width,
      .height = mb_info->framebuffer_height,
      .bpp = mb_info->framebuffer_bpp,
  };
  return boot_init_common(&boot, preferred);
}
