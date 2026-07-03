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

  drm_framebuffer_t *fb =
      (drm_framebuffer_t *)kmem_calloc(sizeof(drm_framebuffer_t), 1);
  if (!fb) {
    return DRM_ID_NONE;
  }
  fb->width = width;
  fb->height = height;
  fb->pitch = pitch;
  fb->bpp = bpp;
  fb->src_y = 0;
  fb->gem = buf;
  buf->refcount++;

  drm_id_t id = drm_object_register(DRM_OBJECT_FRAMEBUFFER, fb);
  if (id == DRM_ID_NONE) {
    buf->refcount--;
    kmem_free(fb);
    return DRM_ID_NONE;
  }
  fb->id = id;
  return id;
}

drm_framebuffer_t *drm_framebuffer_get(drm_id_t id) {
  return (drm_framebuffer_t *)drm_object_get(id, DRM_OBJECT_FRAMEBUFFER);
}

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
  kmem_free(fb);
  return DRM_OK;
}
