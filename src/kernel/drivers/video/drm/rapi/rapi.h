/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_RAPI_RAPI_H
#define DRM_RAPI_RAPI_H

#include <drm/drm.h>

/*
 * rapi — low-level rendering helpers. These functions write pixels into a
 * GEM buffer's memory. They do NOT touch the screen; the caller wraps the
 * buffer in a framebuffer and atomic-commits it to make it visible.
 *
 * Console-aware helpers also live here. They operate on the kernel console
 * driver (kms_console_t) and use the low-level buffer helpers. This keeps
 * text/glyph/rectangle abstractions out of KMS.
 */

struct kms_console;

typedef struct {
  u32 x;
  u32 y;
  u32 width;
  u32 height;
} rapi_rect_t;

/* Surface descriptor used by the console-aware helpers. */
typedef struct {
  drm_handle_t gem;
  u32 pitch;
  u8 bpp;
  u32 width;
  u32 height;
  u32 pan_y;
  u32 buf_h;
} rapi_surface_t;

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

/* Alpha-blend an ARGB32 framebuffer into a raw scanout/backing buffer. */
int rapi_blend_argb32_to_raw(const drm_framebuffer_t *src, u8 *dst,
                             u32 dst_pitch, u8 dst_bpp, u32 dst_w,
                             u32 dst_h, u32 dx, u32 dy,
                             rapi_rect_t *dirty);

/* Scroll the buffer up by `lines` scanlines (in pixels), clearing the
 * exposed bottom region with `bg`. */
int rapi_scroll_up(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 lines,
                    u32 bg);

/* Render a single 8x16 glyph into the buffer at pixel (x, y). */
int rapi_glyph(drm_gem_buffer_t *buf, u32 pitch, u8 bpp, u32 x, u32 y, char c,
                u32 fg, u32 bg);

/* Console-aware drawing helpers. They update the console dirty rect. */
int rapi_console_put_pixel(struct kms_console *con, u32 x, u32 y, u32 color);
int rapi_console_fill_rect(struct kms_console *con, u32 x, u32 y, u32 w, u32 h,
                           u32 color);
int rapi_console_clear(struct kms_console *con, u32 color);
int rapi_console_scroll_up(struct kms_console *con, u32 lines, u32 bg);
int rapi_console_glyph(struct kms_console *con, u32 x, u32 y, char c, u32 fg,
                       u32 bg);

#endif
