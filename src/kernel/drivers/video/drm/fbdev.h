/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_FBDEV_H
#define DRM_FBDEV_H

#include <drm/drm.h>
#include <kernel/drivers/fs/vfs/vfs.h>

#define	DRM_FBIOGET_VSCREENINFO	0x4600
#define	DRM_FBIOPUT_VSCREENINFO	0x4601
#define	DRM_FBIOGET_FSCREENINFO	0x4602
#define	DRM_FBIOGETCMAP		0x4604
#define	DRM_FBIOPUTCMAP		0x4605
#define	DRM_FBIOPAN_DISPLAY	0x4606
#define	DRM_FBIOGET_CON2FBMAP	0x460F
#define	DRM_FBIOPUT_CON2FBMAP	0x4610
#define	DRM_FBIOBLANK		0x4611
#define	DRM_FBIOGET_VBLANK	0x80204612
#define	DRM_FBIO_WAITFORVSYNC	0x40044620

#define	DRM_FB_TYPE_PACKED_PIXELS	0
#define	DRM_FB_VISUAL_TRUECOLOR		2
#define	DRM_FB_ACCEL_NONE		0
#define	DRM_FB_ACTIVATE_TEST		2
#define	DRM_FB_ACTIVATE_MASK		15
#define	DRM_FB_VMODE_NONINTERLACED	0

const drm_driver_t *drm_fbdev_driver_get(void);

/* Boot info type used by fbdev. */
typedef struct {
  u64 hw_address;
  u32 pitch;
  u32 width;
  u32 height;
  u8 bpp;
} drm_fbdev_boot_t;

typedef struct drm_fbdev_info {
	u64	hw_address;
	u64	size;
	u32	pitch;
	u32	width;
	u32	height;
	u8	bpp;
} drm_fbdev_info_t;

int	drm_fbdev_is_ready(void);
int	drm_fbdev_get_info(drm_fbdev_info_t *info);
int	drm_fbdev_vnode_read(vnode_t *vn, void *buf, u64 count,
	    u64 offset);
int	drm_fbdev_vnode_write(vnode_t *vn, const void *buf, u64 count,
	    u64 offset);
int	drm_fbdev_vnode_ioctl(vnode_t *vn, u64 cmd, void *arg);
int	drm_fbdev_vnode_stat(vnode_t *vn, posix_stat_t *st);

#endif
