#ifndef DRM_ATOMIC_H
#define DRM_ATOMIC_H

#include <kernel/drivers/video/drm/driver.h>
#include <kernel/drivers/video/drm/types.h>

int drm_atomic_init(const drm_driver_t *driver, const drm_boot_display_t *boot);
int drm_atomic_begin(void);
int drm_atomic_set_mode(u32 width, u32 height);
int drm_atomic_set_connector(int connected);
int drm_atomic_set_primary_fb(drm_framebuffer_t *fb);
void drm_atomic_mark_dirty(void);
void drm_atomic_mark_dirty_rect(u32 x, u32 y, u32 width, u32 height);
int drm_atomic_commit(void);

int drm_atomic_is_enabled(void);
int drm_atomic_is_ready(void);

const char *drm_atomic_get_driver_name(void);
u32 drm_atomic_get_width(void);
u32 drm_atomic_get_height(void);
u32 drm_atomic_get_pitch(void);
u8 drm_atomic_get_bpp(void);
u64 drm_atomic_get_hw_address(void);
drm_framebuffer_t *drm_atomic_get_draw_fb(void);

#endif
