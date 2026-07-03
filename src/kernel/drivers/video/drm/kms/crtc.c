/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/object.h>
#include <drm/kms/crtc.h>
#include <drm/kms/plane.h>
#include <drm/kms/connector.h>
#include <mlibc/mlibc.h>

static drm_crtc_t g_crtcs[DRM_KMS_MAX_CRTCS];
static u32 g_crtc_count;

/* Active mode geometry, populated by the driver at init. */
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

static u32 crtc_alloc_index(void) {
  for (u32 i = 0; i < DRM_KMS_MAX_CRTCS; i++) {
    if (g_crtcs[i].id == DRM_ID_NONE) {
      return i;
    }
  }
  return DRM_KMS_MAX_CRTCS;
}

static void crtc_sync_from_props(drm_crtc_t *crtc) {
  crtc->active = (int)crtc->props[DRM_PROP_CRTC_ACTIVE];
  crtc->mode_w = (u32)crtc->props[DRM_PROP_CRTC_MODE_W];
  crtc->mode_h = (u32)crtc->props[DRM_PROP_CRTC_MODE_H];
}

drm_id_t drm_crtc_create(void) {
  u32 idx = crtc_alloc_index();
  if (idx >= DRM_KMS_MAX_CRTCS) {
    return DRM_ID_NONE;
  }
  drm_crtc_t *c = &g_crtcs[idx];
  memset(c, 0, sizeof(*c));
  c->index = idx;
  c->props[DRM_PROP_CRTC_ACTIVE] = 1;
  c->props[DRM_PROP_CRTC_MODE_W] = g_mode_w;
  c->props[DRM_PROP_CRTC_MODE_H] = g_mode_h;
  drm_id_t id = drm_object_register(DRM_OBJECT_CRTC, c, idx);
  if (id == DRM_ID_NONE) {
    memset(c, 0, sizeof(*c));
    return DRM_ID_NONE;
  }
  c->id = id;
  crtc_sync_from_props(c);
  if (idx + 1 > g_crtc_count) {
    g_crtc_count = idx + 1;
  }
  return id;
}

drm_crtc_t *drm_crtc_get(drm_id_t id) {
  return (drm_crtc_t *)drm_object_get(id, DRM_OBJECT_CRTC);
}

drm_crtc_t *drm_crtc_get_by_index(u32 index) {
  if (index >= DRM_KMS_MAX_CRTCS) {
    return NULL;
  }
  return &g_crtcs[index];
}

u32 drm_crtc_count(void) { return g_crtc_count; }

drm_crtc_t *drm_crtc_get_primary(void) { return &g_crtcs[0]; }

int drm_crtc_set_mode(drm_crtc_t *crtc, u32 w, u32 h) {
  if (!crtc) {
    return DRM_ERR_INVAL;
  }
  /* Only the boot mode is supported on simple drivers; reject mode changes. */
  if (w != g_mode_w || h != g_mode_h) {
    return DRM_ERR_RANGE;
  }
  crtc->props[DRM_PROP_CRTC_MODE_W] = w;
  crtc->props[DRM_PROP_CRTC_MODE_H] = h;
  crtc_sync_from_props(crtc);
  return DRM_OK;
}

int drm_crtc_set_active(drm_crtc_t *crtc, int active) {
  if (!crtc) {
    return DRM_ERR_INVAL;
  }
  crtc->props[DRM_PROP_CRTC_ACTIVE] = active ? 1 : 0;
  crtc_sync_from_props(crtc);
  return DRM_OK;
}

drm_id_t drm_crtc_init_primary(void) {
  memset(&g_crtcs[0], 0, sizeof(g_crtcs[0]));
  g_crtcs[0].index = 0;
  g_crtcs[0].active = 1;
  g_crtcs[0].mode_w = g_mode_w;
  g_crtcs[0].mode_h = g_mode_h;
  g_crtcs[0].props[DRM_PROP_CRTC_ACTIVE] = 1;
  g_crtcs[0].props[DRM_PROP_CRTC_MODE_W] = g_mode_w;
  g_crtcs[0].props[DRM_PROP_CRTC_MODE_H] = g_mode_h;
  drm_id_t id = drm_object_register(DRM_OBJECT_CRTC, &g_crtcs[0], 0);
  g_crtcs[0].id = id;
  if (1 > g_crtc_count) {
    g_crtc_count = 1;
  }
  return id;
}

void drm_crtc_reset_all(void) {
  memset(g_crtcs, 0, sizeof(g_crtcs));
  g_crtc_count = 0;
}
