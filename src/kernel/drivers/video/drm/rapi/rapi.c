/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/gem.h>
#include <drm/rapi/rapi.h>
#include <kernel/drivers/console/kms_console.h>
#include <mlibc/mlibc.h>

static inline void store_pixel(u8 *base, u32 pitch, u8 bpp, u32 x, u32 y,
                               u32 color) {
  u32 bytes_pp = (u32)(bpp / 8);
  u64 offset = (u64)y * pitch + (u64)x * bytes_pp;
  if (bytes_pp == 4) {
    *(volatile u32 *)(base + offset) = color;
  } else if (bytes_pp == 3) {
    base[offset] = (u8)(color & 0xFF);
    base[offset + 1] = (u8)((color >> 8) & 0xFF);
    base[offset + 2] = (u8)((color >> 16) & 0xFF);
  } else if (bytes_pp == 2) {
    *(volatile u16 *)(base + offset) = (u16)(color & 0xFFFF);
  } else {
    base[offset] = (u8)(color & 0xFF);
  }
}

static u32 buf_height(drm_gem_buffer_t *buf, u32 pitch) {
  if (pitch == 0) {
    return 0;
  }
  return (u32)(buf->size / pitch);
}

int rapi_put_pixel(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 x, u32 y,
                   u32 color) {
  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  if (x * (u32)(bpp / 8) >= pitch) {
    return DRM_ERR_RANGE;
  }
  if ((u64)y * pitch + (u64)x * (bpp / 8) >= buf->size) {
    return DRM_ERR_RANGE;
  }
  store_pixel(buf->data, pitch, bpp, x, y, color);
  return DRM_OK;
}

int rapi_fill_rect(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, rapi_rect_t rect,
                   u32 color) {
  u32 h, w, x2, y2, y, x;

  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  h = buf_height(buf, pitch);
  w = pitch / (u32)(bpp / 8);
  if (rect.x >= w || rect.y >= h || rect.width == 0 || rect.height == 0) {
    return DRM_ERR_RANGE;
  }
  x2 = rect.x + rect.width;
  y2 = rect.y + rect.height;
  if (x2 > w) x2 = w;
  if (y2 > h) y2 = h;
  for (y = rect.y; y < y2; y++) {
    for (x = rect.x; x < x2; x++) {
      store_pixel(buf->data, pitch, bpp, x, y, color);
    }
  }
  return DRM_OK;
}

int rapi_clear(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 color) {
  u32 h, y, x;

  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  h = buf_height(buf, pitch);
  for (y = 0; y < h; y++) {
    for (x = 0; x * (u32)(bpp / 8) < pitch; x++) {
      store_pixel(buf->data, pitch, bpp, x, y, color);
    }
  }
  return DRM_OK;
}

int rapi_blit(drm_gem_buffer_t *src, u32 src_pitch, u8 bpp, rapi_rect_t srect,
              drm_gem_buffer_t *dst, u32 dst_pitch, u32 dx, u32 dy) {
  u32 bpp_bytes, row, col, sy, dyy, sx, dxx;
  u64 soff, doff;

  if (!src || !dst || !src->data || !dst->data || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  bpp_bytes = (u32)(bpp / 8);
  for (row = 0; row < srect.height; row++) {
    sy = srect.y + row;
    dyy = dy + row;
    for (col = 0; col < srect.width; col++) {
      sx = srect.x + col;
      dxx = dx + col;
      soff = (u64)sy * src_pitch + (u64)sx * bpp_bytes;
      doff = (u64)dyy * dst_pitch + (u64)dxx * bpp_bytes;
      if (soff + bpp_bytes > src->size || doff + bpp_bytes > dst->size) {
        continue;
      }
      memcpy(dst->data + doff, src->data + soff, bpp_bytes);
    }
  }
  return DRM_OK;
}

int rapi_scroll_up(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 lines,
                    u32 bg) {
  u32 h, move_bytes, bottom_bytes, bpp_bytes, w, y, x;
  u8 *bottom;

  if (!buf || !buf->data || pitch == 0 || bpp == 0 || lines == 0) {
    return DRM_ERR_INVAL;
  }
  h = buf_height(buf, pitch);
  if (lines >= h) {
    return rapi_clear(buf, pitch, bpp, bg);
  }
  move_bytes = pitch * (h - lines);
  memcpy(buf->data, buf->data + (u64)lines * pitch, move_bytes);
  bottom = buf->data + move_bytes;
  bottom_bytes = (u32)((u64)lines * pitch);
  if (bg == 0) {
    memset(bottom, 0, bottom_bytes);
  } else {
    bpp_bytes = (u32)(bpp / 8);
    w = pitch / bpp_bytes;
    for (y = h - lines; y < h; y++) {
      for (x = 0; x < w; x++) {
        store_pixel(buf->data, pitch, bpp, x, y, bg);
      }
    }
  }
  return DRM_OK;
}

extern const u8 *get_font_data(char c);

int rapi_glyph(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 x, u32 y, char c,
                u32 fg, u32 bg) {
  const u8 *glyph;
  int row, col;

  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  glyph = get_font_data(c);
  if (!glyph) {
    return DRM_ERR_NOENT;
  }
  for (row = 0; row < 16; row++) {
    u8 bits = glyph[row];
    for (col = 0; col < 8; col++) {
      u32 color = (bits & (1 << (7 - col))) ? fg : bg;
      store_pixel(buf->data, pitch, bpp, x + col, y + row, color);
    }
  }
  return DRM_OK;
}

#define FONT_W 8
#define FONT_H 16

int rapi_console_put_pixel(kms_console_t *con, u32 x, u32 y, u32 color) {
  drm_gem_buffer_t *buf;
  u32 by;
  int rc;

  if (!con || !con->ready) return DRM_ERR_INVAL;
  if (x >= con->width || y >= con->height) return DRM_ERR_RANGE;
  buf = drm_gem_lookup(con->gem);
  by = y + con->pan_y;
  if (by >= con->buf_h) return DRM_ERR_RANGE;
  rc = rapi_put_pixel(buf, con->pitch, con->bpp, x, by, color);
  if (rc == DRM_OK) kms_console_mark_dirty(con, x, y, 1, 1);
  return rc;
}

int rapi_console_fill_rect(kms_console_t *con, u32 x, u32 y, u32 w, u32 h,
                           u32 color) {
  drm_gem_buffer_t *buf;
  u32 by;
  rapi_rect_t r;
  int rc;

  if (!con || !con->ready) return DRM_ERR_INVAL;
  if (x >= con->width || y >= con->height) return DRM_ERR_RANGE;
  buf = drm_gem_lookup(con->gem);
  by = y + con->pan_y;
  if (by + h > con->buf_h || x + w > con->width) return DRM_ERR_RANGE;
  r.x = x;
  r.y = by;
  r.width = w;
  r.height = h;
  rc = rapi_fill_rect(buf, con->pitch, con->bpp, r, color);
  if (rc == DRM_OK) kms_console_mark_dirty(con, x, y, w, h);
  return rc;
}

int rapi_console_clear(kms_console_t *con, u32 color) {
  drm_gem_buffer_t *buf;
  u32 by;
  rapi_rect_t r;
  int rc;

  if (!con || !con->ready) return DRM_ERR_INVAL;
  buf = drm_gem_lookup(con->gem);
  if (!buf || !buf->data) return DRM_ERR_INVAL;
  by = con->pan_y;
  r.x = 0;
  r.y = by;
  r.width = con->width;
  r.height = con->height;
  rc = rapi_fill_rect(buf, con->pitch, con->bpp, r, color);
  if (rc == DRM_OK) kms_console_mark_dirty(con, 0, 0, con->width, con->height);
  return rc;
}

int rapi_console_scroll_up(kms_console_t *con, u32 lines, u32 bg) {
  drm_gem_buffer_t *buf;
  u32 new_pan_y, by, clear_h;
  rapi_rect_t r;
  int rc;

  if (!con || !con->ready) return DRM_ERR_INVAL;
  if (lines == 0) return DRM_OK;
  if (lines >= con->height) return rapi_console_clear(con, bg);
  buf = drm_gem_lookup(con->gem);
  if (!buf || !buf->data) return DRM_ERR_INVAL;

  new_pan_y = con->pan_y + lines;
  if (new_pan_y + con->height > con->buf_h) {
    u64 move_bytes = (u64)con->pitch * con->height;
    memcpy(buf->data, buf->data + (u64)con->pan_y * con->pitch, move_bytes);
    new_pan_y = lines;
  }
  con->pan_y = new_pan_y;

  by = con->pan_y + con->height - lines;
  clear_h = lines;
  if (by + clear_h > con->buf_h) clear_h = con->buf_h - by;
  if (clear_h > 0 && by < con->buf_h) {
    r.x = 0;
    r.y = by;
    r.width = con->width;
    r.height = clear_h;
    rapi_fill_rect(buf, con->pitch, con->bpp, r, bg);
  }

  kms_console_mark_dirty(con, 0, 0, con->width, con->height);
  return DRM_OK;
}

int rapi_console_glyph(kms_console_t *con, u32 x, u32 y, char c, u32 fg,
                       u32 bg) {
  drm_gem_buffer_t *buf;
  u32 by;
  int rc;

  if (!con || !con->ready) return DRM_ERR_INVAL;
  if (x >= con->width || y >= con->height) return DRM_ERR_RANGE;
  buf = drm_gem_lookup(con->gem);
  by = y + con->pan_y;
  if (by + FONT_H > con->buf_h) return DRM_ERR_RANGE;
  rc = rapi_glyph(buf, con->pitch, con->bpp, x, by, c, fg, bg);
  if (rc == DRM_OK) kms_console_mark_dirty(con, x, y, FONT_W, FONT_H);
  return rc;
}
