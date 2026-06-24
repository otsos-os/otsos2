/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/object.h>
#include <drm/kms/crtc.h>
#include <drm/kms/plane.h>
#include <drm/kms/connector.h>
#include <mlibc/mlibc.h>

static drm_crtc_t g_primary_crtc;

/* Active mode geometry, populated by the fbdev driver at init. */
static u32 g_mode_w, g_mode_h, g_mode_pitch;
static u8 g_mode_bpp;
static u64 g_hw_address;

void drm_crtc_set_mode_geometry(u32 w, u32 h, u32 pitch, u8 bpp, u64 hw_addr) {
  g_mode_w = w;
  g_mode_h = h;
  g_mode_pitch = pitch;
  g_mode_bpp = bpp;
  g_hw_address = hw_addr;
}

u32 drm_crtc_get_width(void) { return g_mode_w; }
u32 drm_crtc_get_height(void) { return g_mode_h; }
u32 drm_crtc_get_pitch(void) { return g_mode_pitch; }
u8 drm_crtc_get_bpp(void) { return g_mode_bpp; }
u64 drm_crtc_get_hw_address(void) { return g_hw_address; }

drm_id_t drm_crtc_create(void) {
  drm_crtc_t *c = (drm_crtc_t *)kmem_calloc(sizeof(drm_crtc_t), 1);
  if (!c) {
    return DRM_ID_NONE;
  }
  drm_id_t id = drm_object_register(DRM_OBJECT_CRTC, c);
  if (id == DRM_ID_NONE) {
    kmem_free(c);
    return DRM_ID_NONE;
  }
  c->id = id;
  return id;
}

drm_crtc_t *drm_crtc_get(drm_id_t id) {
  return (drm_crtc_t *)drm_object_get(id, DRM_OBJECT_CRTC);
}

drm_crtc_t *drm_crtc_get_primary(void) { return &g_primary_crtc; }

int drm_crtc_set_mode(drm_crtc_t *crtc, u32 w, u32 h) {
  if (!crtc) {
    return DRM_ERR_INVAL;
  }
  /* Only the boot mode is supported on fbdev; reject mode changes. */
  if (w != g_mode_w || h != g_mode_h) {
    return DRM_ERR_RANGE;
  }
  return DRM_OK;
}

int drm_crtc_set_active(drm_crtc_t *crtc, int active) {
  if (!crtc) {
    return DRM_ERR_INVAL;
  }
  crtc->active = active ? 1 : 0;
  return DRM_OK;
}

/* Used by kms_init to wire up the static primary CRTC. */
drm_id_t drm_crtc_init_primary(void) {
  memset(&g_primary_crtc, 0, sizeof(g_primary_crtc));
  g_primary_crtc.active = 1;
  g_primary_crtc.mode_w = g_mode_w;
  g_primary_crtc.mode_h = g_mode_h;
  drm_id_t id = drm_object_register(DRM_OBJECT_CRTC, &g_primary_crtc);
  g_primary_crtc.id = id;
  return id;
}
