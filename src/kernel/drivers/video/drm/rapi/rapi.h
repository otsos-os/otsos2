/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_RAPI_RAPI_H
#define DRM_RAPI_RAPI_H

#include <drm/drm.h>

/*
 * rapi — low-level rendering helpers.
 *
 * These functions write pixels into a GEM buffer's memory. They do NOT touch
 * the screen; the caller wraps the buffer in a framebuffer and atomic-commits
 * it to make it visible. This keeps the rendering surface and the scanout
 * pipeline decoupled: a program can render off-screen, double-buffer, or
 * composite multiple buffers before flipping.
 */

typedef struct {
  u32 x;
  u32 y;
  u32 width;
  u32 height;
} rapi_rect_t;

/* Store a pixel at (x, y) of the given buffer. Bounds-checked. */
int rapi_put_pixel(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 x, u32 y,
                   u32 color);

/* Fill a rectangle region of the buffer with a solid color. */
int rapi_fill_rect(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, rapi_rect_t rect,
                   u32 color);

/* Clear the entire buffer to a color. */
int rapi_clear(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 color);

/* Copy a rectangular region between two buffers of the same format. */
int rapi_blit(drm_gem_buffer_t *src, u32 src_pitch, u8 bpp, rapi_rect_t srect,
              drm_gem_buffer_t *dst, u32 dst_pitch, u32 dx, u32 dy);

/* Scroll the buffer up by `lines` scanlines (in pixels), clearing the
 * exposed bottom region with `bg`. */
int rapi_scroll_up(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 lines,
                   u32 bg);

/* Render a single 8x16 glyph into the buffer at pixel (x, y). */
int rapi_glyph(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 x, u32 y, char c,
               u32 fg, u32 bg);

#endif
