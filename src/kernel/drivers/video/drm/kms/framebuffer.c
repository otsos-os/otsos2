/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/gem.h>
#include <drm/object.h>
#include <drm/kms/framebuffer.h>
#include <drm/kms/plane.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static drm_framebuffer_t g_framebuffers[DRM_KMS_MAX_FRAMEBUFFERS];
static u32 g_framebuffer_count;

static u32 framebuffer_alloc_index(void) {
  for (u32 i = 0; i < DRM_KMS_MAX_FRAMEBUFFERS; i++) {
    if (g_framebuffers[i].id == DRM_ID_NONE) {
      return i;
    }
  }
  return DRM_KMS_MAX_FRAMEBUFFERS;
}

drm_id_t drm_framebuffer_create(drm_handle_t gem, u32 width, u32 height,
                                u32 pitch, u8 bpp) {
  drm_gem_buffer_t *buf = drm_gem_lookup(gem);
  if (!buf || width == 0 || height == 0 || pitch == 0 || bpp == 0) {
    return DRM_ID_NONE;
  }
  if ((u64)pitch * height > buf->size) {
    drivers_log("[FB] gem buffer too small for framebuffer\n");
    return DRM_ID_NONE;
  }
  u32 idx = framebuffer_alloc_index();
  if (idx >= DRM_KMS_MAX_FRAMEBUFFERS) {
    return DRM_ID_NONE;
  }
  drm_framebuffer_t *fb = &g_framebuffers[idx];
  memset(fb, 0, sizeof(*fb));
  fb->index = idx;
  fb->width = width;
  fb->height = height;
  fb->pitch = pitch;
  fb->bpp = bpp;

  fb->gem = buf;
  buf->refcount++;

  drm_id_t id = drm_object_register(DRM_OBJECT_FRAMEBUFFER, fb, idx);
  if (id == DRM_ID_NONE) {
    buf->refcount--;
    memset(fb, 0, sizeof(*fb));
    return DRM_ID_NONE;
  }
  fb->id = id;
  if (idx + 1 > g_framebuffer_count) {
    g_framebuffer_count = idx + 1;
  }
  return id;
}

drm_framebuffer_t *drm_framebuffer_get(drm_id_t id) {
  return (drm_framebuffer_t *)drm_object_get(id, DRM_OBJECT_FRAMEBUFFER);
}

drm_framebuffer_t *drm_framebuffer_get_by_index(u32 index) {
  if (index >= DRM_KMS_MAX_FRAMEBUFFERS) {
    return NULL;
  }
  return &g_framebuffers[index];
}

u32 drm_framebuffer_count(void) { return g_framebuffer_count; }

int drm_framebuffer_destroy(drm_id_t id) {
  drm_framebuffer_t *fb = drm_framebuffer_get(id);
  if (!fb) {
    return DRM_ERR_NOENT;
  }
  /* Refuse if still bound to a plane. */
  if (drm_plane_find_by_fb(id) != NULL) {
    return DRM_ERR_BUSY;
  }
  if (fb->gem) {
    fb->gem->refcount--;
  }
  drm_object_unregister(id);
  memset(fb, 0, sizeof(*fb));
  return DRM_OK;
}

void drm_framebuffer_reset_all(void) {
  memset(g_framebuffers, 0, sizeof(g_framebuffers));
  g_framebuffer_count = 0;
}
