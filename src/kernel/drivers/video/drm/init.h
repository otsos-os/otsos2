/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_INIT_H
#define DRM_INIT_H

#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>

int drm_boot_init_mb2(multiboot2_info_t *mb_info, const char *preferred_driver);
int drm_boot_init_mb1(multiboot_info_t *mb_info, const char *preferred_driver);

/* Return the saved fbdev boot info (for driver fallback after reinit). */
const void *drm_fbdev_get_boot_info(void);

#endif
