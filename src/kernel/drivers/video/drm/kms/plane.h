/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_KMS_PLANE_H
#define DRM_KMS_PLANE_H

#include <drm/drm.h>

#define DRM_PLANE_PRIMARY 0
#define DRM_PLANE_CURSOR  1
#define DRM_PLANE_OVERLAY 2

drm_id_t drm_plane_create(u32 type);
drm_plane_t *drm_plane_get(drm_id_t id);
drm_plane_t *drm_plane_get_primary(void);
drm_plane_t *drm_plane_find_by_fb(drm_id_t fb_id);

/* Bind fb / crtc on a plane (used by atomic commit). */
int drm_plane_set_fb(drm_plane_t *plane, drm_id_t fb_id);
int drm_plane_set_crtc(drm_plane_t *plane, drm_id_t crtc_id);

/* Internal: register the static primary plane. */
drm_id_t drm_plane_init_primary(void);

#endif
