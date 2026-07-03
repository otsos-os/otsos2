/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <drm/drm.h>
#include <drm/object.h>
#include <drm/kms/crtc.h>
#include <drm/kms/framebuffer.h>
#include <drm/kms/plane.h>
#include <mlibc/mlibc.h>

static drm_plane_t g_planes[DRM_KMS_MAX_PLANES];
static u32 g_plane_count;

static u32 plane_alloc_index(void) {
  for (u32 i = 0; i < DRM_KMS_MAX_PLANES; i++) {
    if (g_planes[i].id == DRM_ID_NONE) {
      return i;
    }
  }
  return DRM_KMS_MAX_PLANES;
}

static void plane_sync_from_props(drm_plane_t *plane) {
  plane->fb_id = (drm_id_t)plane->props[DRM_PROP_PLANE_FB_ID];
  plane->crtc_id = (drm_id_t)plane->props[DRM_PROP_PLANE_CRTC_ID];
  plane->fb = (plane->fb_id == DRM_ID_NONE) ? NULL : drm_framebuffer_get(plane->fb_id);
  plane->crtc = (plane->crtc_id == DRM_ID_NONE) ? NULL : drm_crtc_get(plane->crtc_id);
}

drm_id_t drm_plane_create(u32 type) {
  u32 idx = plane_alloc_index();
  if (idx >= DRM_KMS_MAX_PLANES) {
    return DRM_ID_NONE;
  }
  drm_plane_t *p = &g_planes[idx];
  memset(p, 0, sizeof(*p));
  p->index = idx;
  p->type = type;
  p->fb_id = DRM_ID_NONE;
  p->crtc_id = DRM_ID_NONE;
  p->props[DRM_PROP_PLANE_FB_ID] = DRM_ID_NONE;
  p->props[DRM_PROP_PLANE_CRTC_ID] = DRM_ID_NONE;
  drm_id_t id = drm_object_register(DRM_OBJECT_PLANE, p, idx);
  if (id == DRM_ID_NONE) {
    memset(p, 0, sizeof(*p));
    return DRM_ID_NONE;
  }
  p->id = id;
  if (idx + 1 > g_plane_count) {
    g_plane_count = idx + 1;
  }
  return id;
}

drm_plane_t *drm_plane_get(drm_id_t id) {
  return (drm_plane_t *)drm_object_get(id, DRM_OBJECT_PLANE);
}

drm_plane_t *drm_plane_get_by_index(u32 index) {
  if (index >= DRM_KMS_MAX_PLANES) {
    return NULL;
  }
  return &g_planes[index];
}

u32 drm_plane_count(void) { return g_plane_count; }

drm_plane_t *drm_plane_get_primary(void) { return &g_planes[0]; }

drm_plane_t *drm_plane_find_by_fb(drm_id_t fb_id) {
  for (u32 i = 0; i < g_plane_count; i++) {
    if (g_planes[i].id != DRM_ID_NONE && g_planes[i].fb_id == fb_id) {
      return &g_planes[i];
    }
  }
  return NULL;
}

int drm_plane_set_fb(drm_plane_t *plane, drm_id_t fb_id) {
  if (!plane) {
    return DRM_ERR_INVAL;
  }
  if (fb_id != DRM_ID_NONE && !drm_framebuffer_get(fb_id)) {
    return DRM_ERR_NOENT;
  }
  plane->props[DRM_PROP_PLANE_FB_ID] = fb_id;
  plane_sync_from_props(plane);
  return DRM_OK;
}

int drm_plane_set_crtc(drm_plane_t *plane, drm_id_t crtc_id) {
  if (!plane) {
    return DRM_ERR_INVAL;
  }
  plane->props[DRM_PROP_PLANE_CRTC_ID] = crtc_id;
  plane_sync_from_props(plane);
  return DRM_OK;
}

drm_id_t drm_plane_init_primary(void) {
  memset(&g_planes[0], 0, sizeof(g_planes[0]));
  g_planes[0].type = DRM_PLANE_PRIMARY;
  g_planes[0].index = 0;
  g_planes[0].fb_id = DRM_ID_NONE;
  g_planes[0].crtc_id = DRM_ID_NONE;
  g_planes[0].props[DRM_PROP_PLANE_FB_ID] = DRM_ID_NONE;
  g_planes[0].props[DRM_PROP_PLANE_CRTC_ID] = DRM_ID_NONE;
  drm_id_t id = drm_object_register(DRM_OBJECT_PLANE, &g_planes[0], 0);
  g_planes[0].id = id;
  if (1 > g_plane_count) {
    g_plane_count = 1;
  }
  return id;
}

void drm_plane_reset_all(void) {
  memset(g_planes, 0, sizeof(g_planes));
  g_plane_count = 0;
}
