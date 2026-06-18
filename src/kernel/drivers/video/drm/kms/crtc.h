/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_KMS_CRTC_H
#define DRM_KMS_CRTC_H

#include <drm/drm.h>

drm_id_t drm_crtc_create(void);
drm_crtc_t *drm_crtc_get(drm_id_t id);
drm_crtc_t *drm_crtc_get_primary(void);

int drm_crtc_set_mode(drm_crtc_t *crtc, u32 w, u32 h);
int drm_crtc_set_active(drm_crtc_t *crtc, int active);

/* Internal: register the static primary CRTC. */
drm_id_t drm_crtc_init_primary(void);

/* Display geometry as configured by the active driver. */
u32 drm_crtc_get_width(void);
u32 drm_crtc_get_height(void);
u32 drm_crtc_get_pitch(void);
u8 drm_crtc_get_bpp(void);
u64 drm_crtc_get_hw_address(void);

/* Called by the fbdev driver during init to publish the active mode geometry
 * before the KMS topology is created. */
void drm_crtc_set_mode_geometry(u32 w, u32 h, u32 pitch, u8 bpp, u64 hw_addr);

#endif
