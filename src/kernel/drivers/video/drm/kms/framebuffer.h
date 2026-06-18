/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_KMS_FRAMEBUFFER_H
#define DRM_KMS_FRAMEBUFFER_H

#include <drm/drm.h>

/* Wrap a GEM buffer as a scanout framebuffer. Returns an id or DRM_ID_NONE. */
drm_id_t drm_framebuffer_create(drm_handle_t gem, u32 width, u32 height,
                                u32 pitch, u8 bpp);

/* Destroy a framebuffer. Fails if currently bound to a plane. */
int drm_framebuffer_destroy(drm_id_t id);

/* Look up by id (internal). */
drm_framebuffer_t *drm_framebuffer_get(drm_id_t id);

#endif
