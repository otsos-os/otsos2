/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_FBDEV_H
#define DRM_FBDEV_H

#include <drm/drm.h>

const drm_driver_t *drm_fbdev_driver_get(void);

/* Boot info type used by fbdev. */
typedef struct {
  u64 hw_address;
  u32 pitch;
  u32 width;
  u32 height;
  u8 bpp;
} drm_fbdev_boot_t;

#endif
