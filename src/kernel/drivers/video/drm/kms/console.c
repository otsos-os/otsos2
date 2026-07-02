/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/auth.h>
#include <drm/gem.h>
#include <drm/kms/atomic.h>
#include <drm/kms/console.h>
#include <drm/kms/crtc.h>
#include <drm/kms/framebuffer.h>
#include <drm/kms/plane.h>
#include <drm/rapi/rapi.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define FONT_W 8
#define FONT_H 16

/* Singleton kernel console — shared by early boot, tty, kshell, and panic.
 * All kernel drawing goes to the same GEM buffer + framebuffer + plane, so
 * there is no competition or corruption between subsystems. */
static kms_console_t g_kernel_con;
static int g_kernel_con_ready;

kms_console_t *kms_kernel_console(void) {
  if (g_kernel_con_ready) {
    return &g_kernel_con;
  }
  if (!drm_is_ready()) {
    return NULL;
  }
  if (kms_console_init(&g_kernel_con) != DRM_OK) {
    return NULL;
  }
  g_kernel_con_ready = 1;
  return &g_kernel_con;
}

void kms_kernel_console_reset(void) {
  if (g_kernel_con_ready) {
    kms_console_shutdown(&g_kernel_con);
  }
  g_kernel_con_ready = 0;
  memset(&g_kernel_con, 0, sizeof(g_kernel_con));
}

/* Dirty-rect tracking: each drawing call extends the dirty region. On flush,
 * only the dirty sub-rectangle is blitted to hardware via present_rect,
 * avoiding a full-framebuffer copy (which can be ~3 MB per flush). */
static void mark_dirty(kms_console_t *con, u32 x, u32 y, u32 w, u32 h) {
  if (!con->dirty) {
    con->dirty_x1 = x;
    con->dirty_y1 = y;
    con->dirty_x2 = x + w;
    con->dirty_y2 = y + h;
    con->dirty = 1;
  } else {
    if (x < con->dirty_x1) con->dirty_x1 = x;
    if (y < con->dirty_y1) con->dirty_y1 = y;
    if (x + w > con->dirty_x2) con->dirty_x2 = x + w;
    if (y + h > con->dirty_y2) con->dirty_y2 = y + h;
  }
  /* Clamp to screen. */
  if (con->dirty_x2 > con->width) con->dirty_x2 = con->width;
  if (con->dirty_y2 > con->height) con->dirty_y2 = con->height;
}

int kms_console_init(kms_console_t *con) {
  if (!con || !drm_is_ready()) {
    return DRM_ERR_NODEV;
  }

  /* If the kernel console is already up, hand back the singleton. */
  if (g_kernel_con_ready && con == &g_kernel_con) {
    return DRM_OK;
  }

  u32 w = drm_crtc_get_width();
  u32 h = drm_crtc_get_height();
  u32 pitch = drm_crtc_get_pitch();
  u8 bpp = drm_crtc_get_bpp();
  if (w == 0 || h == 0 || pitch == 0 || bpp == 0) {
    return DRM_ERR_NODEV;
  }

  /* Allocate a backbuffer twice the screen height so we can scroll by
   * changing the scanout offset rather than copying the whole framebuffer
   * every line. */
  u32 buf_h = h * 2;
  u64 size = (u64)pitch * buf_h;
  con->gem = drm_gem_create(size);
  if (con->gem == 0) {
    return DRM_ERR_NOMEM;
  }

  con->fb = drm_framebuffer_create(con->gem, w, h, pitch, bpp);
  if (con->fb == DRM_ID_NONE) {
    drm_gem_close(con->gem);
    con->gem = 0;
    return DRM_ERR_NOMEM;
  }

  /* Bind this framebuffer to the primary plane. Master is held by drm_init
   * on behalf of the kernel, so the modeset commit succeeds. */
  drm_plane_t *plane = drm_plane_get_primary();
  drm_atomic_req_t req = {
      .obj_id = plane->id,
      .prop_id = DRM_PROP_PLANE_FB_ID,
      .value = con->fb,
  };
  int rc = drm_atomic_commit(&req, 1, DRM_ATOMIC_ALLOW_MODESET);
  if (rc != DRM_OK) {
    com1_printf("[KMSCON] initial commit failed: %d\n", rc);
    drm_framebuffer_destroy(con->fb);
    drm_gem_close(con->gem);
    con->fb = DRM_ID_NONE;
    con->gem = 0;
    return rc;
  }

  con->width = w;
  con->height = h;
  con->pitch = pitch;
  con->bpp = bpp;
  con->buf_h = buf_h;
  con->pan_y = 0;
  con->cols = w / FONT_W;
  con->rows = h / FONT_H;
  con->dirty = 0;
  con->ready = 1;

  /* Mark the whole screen dirty so the first flush clears it. */
  mark_dirty(con, 0, 0, w, h);
  return DRM_OK;
}

void kms_console_shutdown(kms_console_t *con) {
  if (!con || !con->ready) {
    return;
  }
  drm_atomic_req_t req = {
      .obj_id = drm_plane_get_primary()->id,
      .prop_id = DRM_PROP_PLANE_FB_ID,
      .value = DRM_ID_NONE,
  };
  drm_atomic_commit(&req, 1, DRM_ATOMIC_ALLOW_MODESET);
  if (con->fb != DRM_ID_NONE) {
    drm_framebuffer_destroy(con->fb);
  }
  if (con->gem) {
    drm_gem_close(con->gem);
  }
  con->ready = 0;
  if (con == &g_kernel_con) {
    g_kernel_con_ready = 0;
  }
}

int kms_console_flush(kms_console_t *con) {
  if (!con || !con->ready) {
    return DRM_ERR_INVAL;
  }
  if (!con->dirty) {
    return DRM_OK;
  }

  const drm_driver_t *drv = drm_driver_get_selected();
  if (!drv) {
    return DRM_ERR_NODEV;
  }

  /* Resolve the framebuffer object from its ID. */
  drm_framebuffer_t *fb = drm_framebuffer_get(con->fb);
  if (!fb) {
    con->dirty = 0;
    return DRM_ERR_NOENT;
  }

  u32 dx = con->dirty_x1;
  u32 dy = con->dirty_y1;
  u32 dw = con->dirty_x2 - con->dirty_x1;
  u32 dh = con->dirty_y2 - con->dirty_y1;

  /* Tell the driver which part of the backbuffer is currently visible. */
  fb->src_y = con->pan_y;

  /* Prefer a partial blit (only the changed rectangle). Fall back to a
   * full present if the driver doesn't implement present_rect or the
   * dirty region covers the whole screen. */
  if (drv->present_rect && (dw < con->width || dh < con->height)) {
    drv->present_rect(fb, dx, dy, dw, dh);
  } else if (drv->present) {
    drv->present(fb);
  }

  con->dirty = 0;
  return DRM_OK;
}

int kms_console_put_pixel(kms_console_t *con, u32 x, u32 y, u32 color) {
  if (!con || !con->ready) return DRM_ERR_INVAL;
  if (x >= con->width || y >= con->height) return DRM_ERR_RANGE;
  drm_gem_buffer_t *buf = drm_gem_lookup(con->gem);
  u32 by = y + con->pan_y;
  if (by >= con->buf_h) return DRM_ERR_RANGE;
  int rc = rapi_put_pixel(buf, con->pitch, con->bpp, x, by, color);
  if (rc == DRM_OK) mark_dirty(con, x, y, 1, 1);
  return rc;
}

int kms_console_fill_rect(kms_console_t *con, u32 x, u32 y, u32 w, u32 h,
                          u32 color) {
  if (!con || !con->ready) return DRM_ERR_INVAL;
  if (x >= con->width || y >= con->height) return DRM_ERR_RANGE;
  drm_gem_buffer_t *buf = drm_gem_lookup(con->gem);
  u32 by = y + con->pan_y;
  if (by + h > con->buf_h || x + w > con->width) return DRM_ERR_RANGE;
  rapi_rect_t r = {x, by, w, h};
  int rc = rapi_fill_rect(buf, con->pitch, con->bpp, r, color);
  if (rc == DRM_OK) mark_dirty(con, x, y, w, h);
  return rc;
}

int kms_console_clear(kms_console_t *con, u32 color) {
  if (!con || !con->ready) return DRM_ERR_INVAL;
  drm_gem_buffer_t *buf = drm_gem_lookup(con->gem);
  if (!buf || !buf->data) return DRM_ERR_INVAL;
  u32 by = con->pan_y;
  rapi_rect_t r = {0, by, con->width, con->height};
  int rc = rapi_fill_rect(buf, con->pitch, con->bpp, r, color);
  if (rc == DRM_OK) mark_dirty(con, 0, 0, con->width, con->height);
  return rc;
}

int kms_console_scroll_up(kms_console_t *con, u32 lines, u32 bg) {
  if (!con || !con->ready) return DRM_ERR_INVAL;
  if (lines == 0) return DRM_OK;
  if (lines >= con->height) return kms_console_clear(con, bg);

  drm_gem_buffer_t *buf = drm_gem_lookup(con->gem);
  if (!buf || !buf->data) return DRM_ERR_INVAL;

  u32 new_pan_y = con->pan_y + lines;
  if (new_pan_y + con->height > con->buf_h) {
    /* Ran out of backbuffer space: move the visible window back to the top
     * and continue scrolling from there. */
    u64 move_bytes = (u64)con->pitch * con->height;
    memcpy(buf->data, buf->data + (u64)con->pan_y * con->pitch, move_bytes);
    new_pan_y = lines;
  }
  con->pan_y = new_pan_y;

  /* Clear the newly exposed lines at the bottom of the visible window. */
  u32 by = con->pan_y + con->height - lines;
  u32 clear_h = lines;
  if (by + clear_h > con->buf_h) clear_h = con->buf_h - by;
  if (clear_h > 0 && by < con->buf_h) {
    rapi_rect_t r = {0, by, con->width, clear_h};
    rapi_fill_rect(buf, con->pitch, con->bpp, r, bg);
  }

  mark_dirty(con, 0, 0, con->width, con->height);
  return DRM_OK;
}

int kms_console_glyph(kms_console_t *con, u32 x, u32 y, char c, u32 fg,
                      u32 bg) {
  if (!con || !con->ready) return DRM_ERR_INVAL;
  if (x >= con->width || y >= con->height) return DRM_ERR_RANGE;
  drm_gem_buffer_t *buf = drm_gem_lookup(con->gem);
  u32 by = y + con->pan_y;
  if (by + FONT_H > con->buf_h) return DRM_ERR_RANGE;
  int rc = rapi_glyph(buf, con->pitch, con->bpp, x, by, c, fg, bg);
  if (rc == DRM_OK) mark_dirty(con, x, y, FONT_W, FONT_H);
  return rc;
}
