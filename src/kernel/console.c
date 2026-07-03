/*
 * Copyright (c) 2026, otsos team
 */

#include <kernel/console.h>
#include <kernel/drivers/tty.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/video/drm/kms/console.h>
#include <kernel/drivers/video/drm/kms/crtc.h>
#include <mlibc/mlibc.h>
#include <stdarg.h>

static u32 console_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500,
    0xAAAAAA, 0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF,
    0xFFFF55, 0xFFFFFF};

u32 console_color_rgb(u8 attr) { return console_palette[attr & 0x0F]; }

int console_get_width(void) {
  kms_console_t *con = kms_kernel_console();
  if (con) return (int)con->cols;
  if (drm_is_ready()) return (int)(drm_crtc_get_width() / 8);
  return 80;
}

int console_get_height(void) {
  kms_console_t *con = kms_kernel_console();
  if (con) return (int)con->rows;
  if (drm_is_ready()) return (int)(drm_crtc_get_height() / 16);
  return 25;
}

void console_set_color(u8 color) {
  if (tty_is_initialized()) {
    tty_set_color(color);
  }
}

void console_put_entry_at(char c, u8 color, int x, int y) {
  /* After tty is up, all drawing goes through the tty. */
  if (tty_is_initialized()) {
    return;
  }
  kms_console_t *con = kms_kernel_console();
  if (!con) return;
  kms_console_glyph(con, (u32)(x * 8), (u32)(y * 16), c,
                    console_color_rgb(color), 0x000000);
  kms_console_flush(con);
}

/* Early-boot cursor (pre-tty). */
static int early_x = 0;
static int early_y = 0;

static void early_scroll(void) {
  kms_console_t *con = kms_kernel_console();
  if (con) {
    kms_console_scroll_up(con, 16, 0x000000);
  }
  if (early_y > 0) early_y--;
}

static void early_putc(char c) {
  kms_console_t *con = kms_kernel_console();
  if (!con) return;

  int w = (int)con->cols;
  int h = (int)con->rows;
  if (w <= 0) w = 80;
  if (h <= 0) h = 25;

  if (c == '\n') {
    early_x = 0;
    early_y++;
    kms_console_flush(con);
  } else if (c == '\r') {
    early_x = 0;
  } else if (c == '\b') {
    if (early_x > 0) early_x--;
    else if (early_y > 0) { early_y--; early_x = w - 1; }
  } else {
    kms_console_glyph(con, (u32)(early_x * 8), (u32)(early_y * 16), c,
                      0xAAAAAA, 0x000000);
    early_x++;
  }

  if (early_x >= w) { early_x = 0; early_y++; }
  if (early_y >= h) { early_scroll(); early_y = h - 1; }
}

void console_putchar(char c) {
  if (tty_is_initialized()) {
    tty_putc_from_kernel(c);
    return;
  }
  early_putc(c);
}

void console_puts(const char *s) {
  if (!s) return;
  for (int i = 0; s[i]; i++) console_putchar(s[i]);
}

void clear_scr(void) {
  if (tty_is_initialized()) {
    tty_clear_active();
    tty_flush_kernel();
    return;
  }
  kms_console_t *con = kms_kernel_console();
  if (con) {
    kms_console_clear(con, 0x000000);
    kms_console_flush(con);
  }
  early_x = 0;
  early_y = 0;
}

void printf(const char *fmt, ...) {
  char buffer[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  console_puts(buffer);
}
