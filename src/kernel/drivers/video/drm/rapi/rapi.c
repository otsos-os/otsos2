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

static inline u32 load_pixel(u8 *base, u32 pitch, u8 bpp, u32 x, u32 y) {
  u32 bytes_pp = (u32)(bpp / 8);
  u64 offset = (u64)y * pitch + (u64)x * bytes_pp;

  if (bytes_pp == 4) {
    return (*(volatile u32 *)(base + offset) & 0x00FFFFFF);
  } else if (bytes_pp == 3) {
    return ((u32)base[offset] | ((u32)base[offset + 1] << 8) |
        ((u32)base[offset + 2] << 16));
  } else if (bytes_pp == 2) {
    u16 v = *(volatile u16 *)(base + offset);
    u32 r = ((u32)(v >> 11) & 0x1F) << 3;
    u32 g = ((u32)(v >> 5) & 0x3F) << 2;
    u32 b = ((u32)v & 0x1F) << 3;
    return (r << 16) | (g << 8) | b;
  }
  return ((u32)base[offset]);
}

static inline u32 blend_rgb(u32 dst, u32 src)
{
  u32 a, ia;
  u32 sr, sg, sb;
  u32 dr, dg, db;

  a = (src >> 24) & 0xFF;
  if (a == 0) {
    return (dst);
  }
  if (a == 255) {
    return (src & 0x00FFFFFF);
  }

  ia = 255 - a;
  sr = (src >> 16) & 0xFF;
  sg = (src >> 8) & 0xFF;
  sb = src & 0xFF;
  dr = (dst >> 16) & 0xFF;
  dg = (dst >> 8) & 0xFF;
  db = dst & 0xFF;

  dr = (sr * a + dr * ia) / 255;
  dg = (sg * a + dg * ia) / 255;
  db = (sb * a + db * ia) / 255;
  return ((dr << 16) | (dg << 8) | db);
}

static u32 buf_height(drm_gem_buffer_t *buf, u32 pitch) {
  if (pitch == 0) {
    return 0;
  }
  return (u32)(buf->size / pitch);
}

static void rapi_fill_span(u8 *row, u32 bytes_pp, u32 count, u32 color) {
  u32 *p32;
  u16 *p16;
  u32 i;

  if (count == 0) {
    return;
  }
  if (bytes_pp == 4) {
    p32 = (u32 *)row;
    for (i = 0; i < count; i++) {
      p32[i] = color;
    }
  } else if (bytes_pp == 3) {
    for (i = 0; i < count; i++) {
      row[i * 3] = (u8)(color & 0xFF);
      row[i * 3 + 1] = (u8)((color >> 8) & 0xFF);
      row[i * 3 + 2] = (u8)((color >> 16) & 0xFF);
    }
  } else if (bytes_pp == 2) {
    p16 = (u16 *)row;
    for (i = 0; i < count; i++) {
      p16[i] = (u16)(color & 0xFFFF);
    }
  } else {
    memset(row, (int)(color & 0xFF), count);
  }
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
  u8 *first, *row;
  u64 span_bytes;
  u32 bytes_pp, h, w, x2, y2, y;

  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  bytes_pp = (u32)(bpp / 8);
  if (bytes_pp == 0) {
    return DRM_ERR_INVAL;
  }
  h = buf_height(buf, pitch);
  w = pitch / bytes_pp;
  if (rect.x >= w || rect.y >= h || rect.width == 0 || rect.height == 0) {
    return DRM_ERR_RANGE;
  }
  x2 = rect.x + rect.width;
  y2 = rect.y + rect.height;
  if (x2 > w) x2 = w;
  if (y2 > h) y2 = h;
  span_bytes = (u64)(x2 - rect.x) * bytes_pp;
  first = buf->data + (u64)rect.y * pitch + (u64)rect.x * bytes_pp;
  rapi_fill_span(first, bytes_pp, x2 - rect.x, color);
  for (y = rect.y + 1; y < y2; y++) {
    row = buf->data + (u64)y * pitch + (u64)rect.x * bytes_pp;
    memcpy(row, first, span_bytes);
  }
  return DRM_OK;
}

int rapi_clear(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 color) {
  rapi_rect_t rect;
  u32 bytes_pp, h;

  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  bytes_pp = (u32)(bpp / 8);
  if (bytes_pp == 0) {
    return DRM_ERR_INVAL;
  }
  h = buf_height(buf, pitch);
  if (h == 0) {
    return DRM_ERR_RANGE;
  }

  rect.x = 0;
  rect.y = 0;
  rect.width = pitch / bytes_pp;
  rect.height = h;
  return (rapi_fill_rect(buf, pitch, bpp, rect, color));
}

int rapi_blit(drm_gem_buffer_t *src, u32 src_pitch, u8 bpp, rapi_rect_t srect,
              drm_gem_buffer_t *dst, u32 dst_pitch, u32 dx, u32 dy) {
  u64 soff, doff, avail;
  u32 bpp_bytes, row, sy, dyy, cols;

  if (!src || !dst || !src->data || !dst->data || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  bpp_bytes = (u32)(bpp / 8);
  if (bpp_bytes == 0) {
    return DRM_ERR_INVAL;
  }
  for (row = 0; row < srect.height; row++) {
    sy = srect.y + row;
    dyy = dy + row;
    soff = (u64)sy * src_pitch + (u64)srect.x * bpp_bytes;
    doff = (u64)dyy * dst_pitch + (u64)dx * bpp_bytes;
    if (soff + bpp_bytes > src->size || doff + bpp_bytes > dst->size) {
      continue;
    }

    cols = srect.width;
    avail = (src->size - soff) / bpp_bytes;
    if (avail < cols) {
      cols = (u32)avail;
    }
    avail = (dst->size - doff) / bpp_bytes;
    if (avail < cols) {
      cols = (u32)avail;
    }
    if (cols == 0) {
      continue;
    }
    memcpy(dst->data + doff, src->data + soff, (u64)cols * bpp_bytes);
  }
  return DRM_OK;
}

int
rapi_blend_argb32_to_raw(const drm_framebuffer_t *src, u8 *dst,
                         u32 dst_pitch, u8 dst_bpp, u32 dst_w,
                         u32 dst_h, u32 dx, u32 dy,
                         rapi_rect_t *dirty)
{
  u32 x2, y2, sx, sy, x, y;
  u32 dst_color, src_color, out_color;

  if (!src || !src->gem || !src->gem->data || !dst ||
      src->bpp != 32 || dst_pitch == 0 || dst_bpp == 0) {
    return (DRM_ERR_INVAL);
  }
  if (dx >= dst_w || dy >= dst_h) {
    return (DRM_OK);
  }

  x2 = dx + src->width;
  y2 = dy + src->height;
  if (x2 > dst_w) {
    x2 = dst_w;
  }
  if (y2 > dst_h) {
    y2 = dst_h;
  }
  if (x2 <= dx || y2 <= dy) {
    return (DRM_OK);
  }

  for (y = dy; y < y2; y++) {
    sy = y - dy;
    for (x = dx; x < x2; x++) {
      sx = x - dx;
      src_color = *(volatile u32 *)(src->gem->data +
          (u64)sy * src->pitch + (u64)sx * 4);
      if ((src_color >> 24) == 0) {
        continue;
      }
      dst_color = load_pixel(dst, dst_pitch, dst_bpp, x, y);
      out_color = blend_rgb(dst_color, src_color);
      store_pixel(dst, dst_pitch, dst_bpp, x, y, out_color);
    }
  }

  if (dirty) {
    dirty->x = dx;
    dirty->y = dy;
    dirty->width = x2 - dx;
    dirty->height = y2 - dy;
  }
  return (DRM_OK);
}

int rapi_scroll_up(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 lines,
                    u32 bg) {
  rapi_rect_t r;
  u8 *bottom;
  u32 h, move_bytes, bottom_bytes, bpp_bytes;

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
    if (bpp_bytes == 0) {
      return DRM_ERR_INVAL;
    }

    r.x = 0;
    r.y = h - lines;
    r.width = pitch / bpp_bytes;
    r.height = lines;
    rapi_fill_rect(buf, pitch, bpp, r, bg);
  }
  return DRM_OK;
}

extern const u8 *get_font_data(char c);

int rapi_glyph(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 x, u32 y, char c,
                u32 fg, u32 bg) {
  const u8 *glyph;
  u8 *rowp;
  u32 *p32;
  u64 span, last;
  u32 bytes_pp, bits, color;
  int row, col;

  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  bytes_pp = (u32)(bpp / 8);
  if (bytes_pp == 0) {
    return DRM_ERR_INVAL;
  }
  glyph = get_font_data(c);
  if (!glyph) {
    return DRM_ERR_NOENT;
  }
  span = (u64)8 * bytes_pp;
  last = (u64)(y + 15) * pitch + (u64)x * bytes_pp + span;
  if (last > buf->size) {
    return DRM_ERR_RANGE;
  }
  rowp = buf->data + (u64)y * pitch + (u64)x * bytes_pp;
  for (row = 0; row < 16; row++) {
    bits = glyph[row];
    if (bytes_pp == 4) {
      p32 = (u32 *)rowp;
      for (col = 0; col < 8; col++) {
        p32[col] = (bits & (1u << (7 - col))) ? fg : bg;
      }
    } else {
      for (col = 0; col < 8; col++) {
        color = (bits & (1u << (7 - col))) ? fg : bg;
        rapi_fill_span(rowp + (u64)col * bytes_pp, bytes_pp, 1, color);
      }
    }
    rowp += pitch;
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
  if (lines > 50) lines = 50;
  if (lines >= con->height) return rapi_console_clear(con, bg);
  buf = drm_gem_lookup(con->gem);
  if (!buf || !buf->data) return DRM_ERR_INVAL;

  if (con->buf_h <= con->height) {
    rc = rapi_scroll_up(buf, con->pitch, con->bpp, lines, bg);
    if (rc == DRM_OK) {
      con->pan_y = 0;
      kms_console_mark_dirty(con, 0, 0, con->width, con->height);
    }
    return rc;
  }

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
