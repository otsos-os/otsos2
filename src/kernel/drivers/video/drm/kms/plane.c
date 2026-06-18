/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/object.h>
#include <drm/kms/framebuffer.h>
#include <drm/kms/plane.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

static drm_plane_t g_primary_plane;

drm_id_t drm_plane_create(u32 type) {
  drm_plane_t *p = (drm_plane_t *)kcalloc(sizeof(drm_plane_t), 1);
  if (!p) {
    return DRM_ID_NONE;
  }
  p->type = type;
  p->fb_id = DRM_ID_NONE;
  p->crtc_id = DRM_ID_NONE;
  drm_id_t id = drm_object_register(DRM_OBJECT_PLANE, p);
  if (id == DRM_ID_NONE) {
    kfree(p);
    return DRM_ID_NONE;
  }
  p->id = id;
  return id;
}

drm_plane_t *drm_plane_get(drm_id_t id) {
  return (drm_plane_t *)drm_object_get(id, DRM_OBJECT_PLANE);
}

drm_plane_t *drm_plane_get_primary(void) { return &g_primary_plane; }

drm_plane_t *drm_plane_find_by_fb(drm_id_t fb_id) {
  if (g_primary_plane.fb_id == fb_id) {
    return &g_primary_plane;
  }
  /* Overlay/cursor planes would be scanned here in the future. */
  return NULL;
}

int drm_plane_set_fb(drm_plane_t *plane, drm_id_t fb_id) {
  if (!plane) {
    return DRM_ERR_INVAL;
  }
  if (fb_id != DRM_ID_NONE && !drm_framebuffer_get(fb_id)) {
    return DRM_ERR_NOENT;
  }
  plane->fb_id = fb_id;
  plane->fb = (fb_id == DRM_ID_NONE) ? NULL : drm_framebuffer_get(fb_id);
  return DRM_OK;
}

int drm_plane_set_crtc(drm_plane_t *plane, drm_id_t crtc_id) {
  if (!plane) {
    return DRM_ERR_INVAL;
  }
  plane->crtc_id = crtc_id;
  return DRM_OK;
}

/* Used by kms_init to register the static primary plane. */
drm_id_t drm_plane_init_primary(void) {
  memset(&g_primary_plane, 0, sizeof(g_primary_plane));
  g_primary_plane.type = DRM_PLANE_PRIMARY;
  g_primary_plane.fb_id = DRM_ID_NONE;
  g_primary_plane.crtc_id = DRM_ID_NONE;
  drm_id_t id = drm_object_register(DRM_OBJECT_PLANE, &g_primary_plane);
  g_primary_plane.id = id;
  return id;
}
