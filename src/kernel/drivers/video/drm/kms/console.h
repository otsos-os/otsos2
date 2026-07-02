/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_KMS_CONSOLE_H
#define DRM_KMS_CONSOLE_H

#include <drm/drm.h>

/*
 * kms_console — kernel-internal display console built on the low-level DRM
 * API. It owns a GEM buffer + framebuffer sized to the active mode, exposes
 * a cell-grid drawing interface (used by tty/kshell/panic), and flushes only
 * the dirty region to hardware via present_rect.
 *
 * There is a single shared instance (the "kernel console") obtained via
 * kms_kernel_console(). All kernel subsystems draw to the same buffer, so
 * there is no competition for the primary plane. Userspace never sees this —
 * it talks to the DRM syscall directly.
 */

typedef struct {
  drm_handle_t gem;
  drm_id_t fb;
  u32 width;       /* in pixels */
  u32 height;      /* visible screen height */
  u32 pitch;
  u8 bpp;
  u32 cols;        /* in text cells (8x16 font) */
  u32 rows;
  int ready;
  /* offset-based scrolling state */
  u32 buf_h;       /* total GEM buffer height */
  u32 pan_y;       /* vertical offset of the visible region */
  /* dirty-rect tracking */
  u32 dirty_x1, dirty_y1, dirty_x2, dirty_y2;
  int dirty;
} kms_console_t;

/* Get the singleton kernel console. Initializes it on first call. */
kms_console_t *kms_kernel_console(void);

/* Reset the singleton kernel console (shuts it down and clears the ready
 * flag so the next kms_kernel_console() call re-initialises it). */
void kms_kernel_console_reset(void);

/* Bring up / tear down a console instance. */
int kms_console_init(kms_console_t *con);
void kms_console_shutdown(kms_console_t *con);

/* Flush the dirty region to the screen. Uses present_rect for partial
 * updates, falling back to full present. */
int kms_console_flush(kms_console_t *con);

/* Drawing primitives — operate on the console's buffer, mark dirty. */
int kms_console_put_pixel(kms_console_t *con, u32 x, u32 y, u32 color);
int kms_console_fill_rect(kms_console_t *con, u32 x, u32 y, u32 w, u32 h,
                          u32 color);
int kms_console_clear(kms_console_t *con, u32 color);
int kms_console_scroll_up(kms_console_t *con, u32 lines, u32 bg);
int kms_console_glyph(kms_console_t *con, u32 x, u32 y, char c, u32 fg,
                      u32 bg);

#endif
