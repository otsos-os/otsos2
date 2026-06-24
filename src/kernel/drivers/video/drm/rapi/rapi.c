/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/rapi/rapi.h>
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
  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  u32 h = buf_height(buf, pitch);
  u32 w = pitch / (u32)(bpp / 8);
  if (rect.x >= w || rect.y >= h || rect.width == 0 || rect.height == 0) {
    return DRM_ERR_RANGE;
  }
  u32 x2 = rect.x + rect.width;
  u32 y2 = rect.y + rect.height;
  if (x2 > w) x2 = w;
  if (y2 > h) y2 = h;
  for (u32 y = rect.y; y < y2; y++) {
    for (u32 x = rect.x; x < x2; x++) {
      store_pixel(buf->data, pitch, bpp, x, y, color);
    }
  }
  return DRM_OK;
}

int rapi_clear(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 color) {
  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  u32 h = buf_height(buf, pitch);
  for (u32 y = 0; y < h; y++) {
    for (u32 x = 0; x * (u32)(bpp / 8) < pitch; x++) {
      store_pixel(buf->data, pitch, bpp, x, y, color);
    }
  }
  return DRM_OK;
}

int rapi_blit(drm_gem_buffer_t *src, u32 src_pitch, u8 bpp, rapi_rect_t srect,
              drm_gem_buffer_t *dst, u32 dst_pitch, u32 dx, u32 dy) {
  if (!src || !dst || !src->data || !dst->data || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  u32 bpp_bytes = (u32)(bpp / 8);
  for (u32 row = 0; row < srect.height; row++) {
    u32 sy = srect.y + row;
    u32 dyy = dy + row;
    for (u32 col = 0; col < srect.width; col++) {
      u32 sx = srect.x + col;
      u32 dxx = dx + col;
      u64 soff = (u64)sy * src_pitch + (u64)sx * bpp_bytes;
      u64 doff = (u64)dyy * dst_pitch + (u64)dxx * bpp_bytes;
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
  if (!buf || !buf->data || pitch == 0 || bpp == 0 || lines == 0) {
    return DRM_ERR_INVAL;
  }
  u32 h = buf_height(buf, pitch);
  if (lines >= h) {
    return rapi_clear(buf, pitch, bpp, bg);
  }
  u32 move_bytes = pitch * (h - lines);
  memcpy(buf->data, buf->data + (u64)lines * pitch, move_bytes);
  /* Clear exposed bottom. */
  u8 *bottom = buf->data + move_bytes;
  for (u64 i = 0; i < (u64)lines * pitch; i++) {
    bottom[i] = 0;
  }
  /* Repaint bottom region with bg via rapi_fill_rect would double-iterate;
   * cheaper to do it inline by storing bg per pixel. */
  u32 bpp_bytes = (u32)(bpp / 8);
  u32 w = pitch / bpp_bytes;
  for (u32 y = h - lines; y < h; y++) {
    for (u32 x = 0; x < w; x++) {
      store_pixel(buf->data, pitch, bpp, x, y, bg);
    }
  }
  return DRM_OK;
}

extern const u8 *get_font_data(char c);

int rapi_glyph(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 x, u32 y, char c,
               u32 fg, u32 bg) {
  if (!buf || !buf->data || pitch == 0 || bpp == 0) {
    return DRM_ERR_INVAL;
  }
  const u8 *glyph = get_font_data(c);
  if (!glyph) {
    return DRM_ERR_NOENT;
  }
  for (int row = 0; row < 16; row++) {
    u8 bits = glyph[row];
    for (int col = 0; col < 8; col++) {
      u32 color = (bits & (1 << (7 - col))) ? fg : bg;
      store_pixel(buf->data, pitch, bpp, x + col, y + row, color);
    }
  }
  return DRM_OK;
}
