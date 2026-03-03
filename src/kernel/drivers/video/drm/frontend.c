#include <kernel/drivers/video/drm/atomic.h>
#include <kernel/drivers/video/drm/frontend.h>
#include <mlibc/mlibc.h>

#define DRM_CHAR_W 8
#define DRM_CHAR_H 16
#define DRM_COMMIT_THRESHOLD 4096U

static int g_batch_depth = 0;
static u32 g_pending_writes = 0;

extern const u8 *get_font_data(char c);

static inline void drm_store_pixel(u8 *base, u32 pitch, u8 bpp, int x, int y,
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

static void drm_maybe_commit(int force) {
  if (!drm_atomic_is_ready()) {
    return;
  }

  if (!force && g_pending_writes < DRM_COMMIT_THRESHOLD) {
    return;
  }

  drm_atomic_commit();
  g_pending_writes = 0;
}

int drm_frontend_is_available(void) { return drm_atomic_is_ready(); }

int drm_frontend_get_cols(void) {
  if (!drm_frontend_is_available()) {
    return 0;
  }
  return (int)(drm_atomic_get_width() / DRM_CHAR_W);
}

int drm_frontend_get_rows(void) {
  if (!drm_frontend_is_available()) {
    return 0;
  }
  return (int)(drm_atomic_get_height() / DRM_CHAR_H);
}

void drm_frontend_batch_begin(void) {
  if (!drm_frontend_is_available()) {
    return;
  }

  if (g_batch_depth == 0) {
    drm_atomic_begin();
  }
  g_batch_depth++;
}

void drm_frontend_batch_end(void) {
  if (!drm_frontend_is_available()) {
    return;
  }

  if (g_batch_depth <= 0) {
    g_batch_depth = 0;
    drm_maybe_commit(1);
    return;
  }

  g_batch_depth--;
  if (g_batch_depth == 0) {
    drm_maybe_commit(1);
  }
}

void drm_frontend_flush(void) { drm_maybe_commit(1); }

void drm_frontend_clear(u32 color) {
  if (!drm_frontend_is_available()) {
    return;
  }

  drm_framebuffer_t *fb = drm_atomic_get_draw_fb();
  if (!fb || !fb->data) {
    return;
  }

  drm_frontend_batch_begin();
  for (u32 y = 0; y < fb->height; y++) {
    for (u32 x = 0; x < fb->width; x++) {
      drm_store_pixel(fb->data, fb->pitch, fb->bpp, (int)x, (int)y, color);
    }
  }
  drm_atomic_mark_dirty();
  g_pending_writes += fb->width * fb->height;
  drm_frontend_batch_end();
}

void drm_frontend_scroll_lines(int lines) {
  if (!drm_frontend_is_available() || lines <= 0) {
    return;
  }

  drm_framebuffer_t *fb = drm_atomic_get_draw_fb();
  if (!fb || !fb->data) {
    return;
  }

  u32 px_lines = (u32)lines * DRM_CHAR_H;
  if (px_lines >= fb->height) {
    drm_frontend_clear(0x000000);
    return;
  }

  u32 bytes_per_line = fb->pitch;
  u32 bytes_to_copy = bytes_per_line * (fb->height - px_lines);
  u32 bytes_to_clear = bytes_per_line * px_lines;

  drm_frontend_batch_begin();
  for (u32 i = 0; i < bytes_to_copy; i++) {
    fb->data[i] = fb->data[i + (bytes_per_line * px_lines)];
  }

  u8 *bottom = fb->data + bytes_to_copy;
  for (u32 i = 0; i < bytes_to_clear; i++) {
    bottom[i] = 0;
  }

  drm_atomic_mark_dirty();
  g_pending_writes += bytes_to_copy + bytes_to_clear;
  drm_frontend_batch_end();
}

void drm_frontend_put_char_cell(int x, int y, char c, u32 fg_color) {
  if (!drm_frontend_is_available()) {
    return;
  }

  drm_framebuffer_t *fb = drm_atomic_get_draw_fb();
  const u8 *glyph = get_font_data(c);
  if (!fb || !fb->data || !glyph) {
    return;
  }

  int px = x * DRM_CHAR_W;
  int py = y * DRM_CHAR_H;

  for (int row = 0; row < DRM_CHAR_H; row++) {
    u8 bits = glyph[row];
    for (int col = 0; col < DRM_CHAR_W; col++) {
      u32 color = (bits & (1 << (7 - col))) ? fg_color : 0x000000;
      drm_store_pixel(fb->data, fb->pitch, fb->bpp, px + col, py + row, color);
    }
  }
  drm_atomic_mark_dirty_rect((u32)px, (u32)py, DRM_CHAR_W, DRM_CHAR_H);
  g_pending_writes += (DRM_CHAR_W * DRM_CHAR_H);

  if (g_batch_depth == 0) {
    drm_maybe_commit(0);
  }
}
