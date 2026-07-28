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
#include <drm/init.h>
#include <kernel/drivers/newbus/newbus.h>
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

static const drm_driver_t *
boot_select_driver(const drm_fbdev_boot_t *boot, const char *preferred)
{
  const drm_driver_t *drv;

  if (preferred && preferred[0]) {
    return drm_driver_select(boot, preferred);
  }

  drv = drm_driver_get_fbdev();
  if (drv && (!drv->probe || drv->probe(boot) == 0)) {
    return drv;
  }
  if (drv) {
    drivers_log("[DRM] boot framebuffer driver probe failed\n");
  }

  return drm_driver_select(boot, NULL);
}

static int boot_init_common(const drm_fbdev_boot_t *boot,
                            const char *preferred) {
  if (!boot) {
    return -1;
  }
  const drm_driver_t *drv = boot_select_driver(boot, preferred);
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

static void
drm_boot_identify(driver_t *driver, device_t parent)
{
  (void)driver;
  if (device_find_child(parent, "drm_boot", 0) == NULL) {
    device_add_child(parent, "drm_boot", 0);
  }
}

static int
drm_boot_probe(device_t dev)
{
  const newbus_bootinfo_t *boot;

  (void)dev;
  boot = newbus_get_bootinfo();
  if (boot == NULL) {
    return -1;
  }
  if (boot->magic == MULTIBOOT2_BOOTLOADER_MAGIC && boot->mb2 != NULL) {
    return 0;
  }
  if (boot->magic == MULTIBOOT_BOOTLOADER_MAGIC && boot->mb1 != NULL) {
    return 0;
  }
  return -1;
}

static int
drm_boot_attach(device_t dev)
{
  const newbus_bootinfo_t *boot;

  (void)dev;
  boot = newbus_get_bootinfo();
  if (boot == NULL) {
    return -1;
  }
  if (boot->magic == MULTIBOOT2_BOOTLOADER_MAGIC && boot->mb2 != NULL) {
    return drm_boot_init_mb2((multiboot2_info_t *)boot->mb2, NULL);
  }
  if (boot->magic == MULTIBOOT_BOOTLOADER_MAGIC && boot->mb1 != NULL) {
    return drm_boot_init_mb1((multiboot_info_t *)boot->mb1, NULL);
  }
  return -1;
}

static devclass_t drm_boot_devclass = {
    .name = "drm",
    .maxunit = 1,
};

static driver_t drm_boot_driver = {
    .name = "drm_boot",
    .identify = drm_boot_identify,
    .probe = drm_boot_probe,
    .attach = drm_boot_attach,
};

FIRMWARE_DRIVER_MODULE(drm_boot, drm_boot_driver, drm_boot_devclass,
    NEWBUS_PASS_DISPLAY, NEWBUS_ORDER_LATE);
