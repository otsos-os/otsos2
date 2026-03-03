#ifndef DRM_DRIVER_H
#define DRM_DRIVER_H

#include <kernel/drivers/video/drm/types.h>

typedef struct {
  u64 hw_address;
  u32 pitch;
  u32 width;
  u32 height;
  u8 bpp;
} drm_boot_display_t;

typedef struct drm_driver {
  const char *name;
  int priority;
  int (*probe)(const drm_boot_display_t *boot);
  int (*init)(const drm_boot_display_t *boot, drm_framebuffer_t *out_hw_fb);
  int (*present)(const drm_framebuffer_t *src_fb,
                 const drm_framebuffer_t *dst_hw_fb);
  int (*present_rect)(const drm_framebuffer_t *src_fb,
                      const drm_framebuffer_t *dst_hw_fb,
                      const drm_rect_t *rect);
} drm_driver_t;

const drm_driver_t *drm_driver_select(const drm_boot_display_t *boot,
                                      const char *preferred_name);
const drm_driver_t *drm_driver_get_selected(void);
const char *drm_driver_get_selected_name(void);
u32 drm_driver_count(void);
const drm_driver_t *drm_driver_get_by_index(u32 index);
int drm_driver_get_selected_index(void);

#endif
