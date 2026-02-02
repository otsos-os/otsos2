/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/drivers/vga.h>
#include <kernel/drivers/video/fb.h>
#include <mlibc/mlibc.h>
#include <stdarg.h>

static u16 *vga_buffer = (u16 *)0xB8000;
static int cursor_x = 0;
static int cursor_y = 0;
static u8 terminal_color = 0x07;

static int vga_width = 80;
static int vga_height = 25;
static int vga_using_fb = -1;

void update_vga_dims() {
  int fb_enabled = is_framebuffer_enabled();

  if (fb_enabled) {
    vga_width = fb_get_width() / 8;
    vga_height = fb_get_height() / 16;
  } else {
    vga_width = 80;
    vga_height = 25;
  }
  vga_using_fb = fb_enabled;
}

void clear_scr() {
  if (is_framebuffer_enabled() != vga_using_fb)
    update_vga_dims();

  if (is_framebuffer_enabled()) {
    fb_clear(0x000000);
  } else {
    for (int y = 0; y < 25; y++) {
      for (int x = 0; x < 80; x++) {
        vga_buffer[y * 80 + x] = (u16)terminal_color << 8 | ' ';
      }
    }
  }
  cursor_x = 0;
  cursor_y = 0;
}

void scroll_scr() {
  if (is_framebuffer_enabled() != vga_using_fb)
    update_vga_dims();

  if (is_framebuffer_enabled()) {
    fb_scroll(1);

  } else {
    for (int y = 0; y < 24; y++) {
      for (int x = 0; x < 80; x++) {
        vga_buffer[y * 80 + x] = vga_buffer[(y + 1) * 80 + x];
      }
    }
    for (int x = 0; x < 80; x++) {
      vga_buffer[24 * 80 + x] = (u16)terminal_color << 8 | ' ';
    }
  }
  if (cursor_y > 0)
    cursor_y--;
}

extern int is_framebuffer_enabled();

static u32 vga_palette[16] = {0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
                              0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
                              0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
                              0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF};
static u32 current_fb_color = 0xFFFFFF;

void vga_set_color(u8 color) {
  terminal_color = color;
  current_fb_color = vga_palette[color & 0x0F];
}

static int ansi_state = 0;
static int ansi_val = 0;

void vga_apply_ansi(int code) {
  u8 color_idx = 0x07;
  switch (code) {
  case 0:
    color_idx = 0x07;
    break;
  case 30:
    color_idx = 0x00;
    break; // black
  case 31:
    color_idx = 0x04;
    break; // red
  case 32:
    color_idx = 0x02;
    break; // green
  case 33:
    color_idx = 0x0E;
    break; // yellow
  case 34:
    color_idx = 0x01;
    break; // blue
  case 35:
    color_idx = 0x05;
    break; // magenta
  case 36:
    color_idx = 0x03;
    break; // cyan
  case 37:
    color_idx = 0x0F;
    break; // white
  default:
    return;
  }
  vga_set_color(color_idx);
}

void vga_putc(char c) {
  if (is_framebuffer_enabled() != vga_using_fb)
    update_vga_dims();

  if (ansi_state == 0) {
    if (c == 0x1B) {
      ansi_state = 1;
      return;
    }
  } else if (ansi_state == 1) {
    if (c == '[') {
      ansi_state = 2;
      ansi_val = 0;
      return;
    } else {
      ansi_state = 0;
    }
  } else if (ansi_state == 2) {
    if (c >= '0' && c <= '9') {
      ansi_val = ansi_val * 10 + (c - '0');
      return;
    } else if (c == 'm') {
      vga_apply_ansi(ansi_val);
      ansi_state = 0;
      return;
    } else {
      ansi_state = 0;
    }
  }

  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if (c == '\r') {
    cursor_x = 0;
  } else if (c == '\b') {
    if (cursor_x > 0) {
      cursor_x--;
    } else if (cursor_y > 0) {
      cursor_y--;
      cursor_x = vga_width - 1;
    }
  } else {
    if (is_framebuffer_enabled()) {
      fb_put_char(cursor_x * 8, cursor_y * 16, c, current_fb_color);
    } else {
      vga_buffer[cursor_y * 80 + cursor_x] = (u16)terminal_color << 8 | c;
    }
    cursor_x++;
  }

  if (cursor_x >= vga_width) {
    cursor_x = 0;
    cursor_y++;
  }

  if (cursor_y >= vga_height) {
    scroll_scr();
    cursor_y = vga_height - 1;
  }
}

void vga_puts(const char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    vga_putc(str[i]);
  }
}

void vga_write_dec(u64 value) {
  if (value == 0) {
    vga_putc('0');
    return;
  }
  char buffer[32];
  int i = 0;
  while (value > 0) {
    buffer[i++] = '0' + (value % 10);
    value /= 10;
  }
  while (i > 0) {
    vga_putc(buffer[--i]);
  }
}

void vga_write_hex(u64 value, int width) {
  const char hex[] = "0123456789ABCDEF";
  for (int i = (width - 1) * 4; i >= 0; i -= 4) {
    vga_putc(hex[(value >> i) & 0xF]);
  }
}

void printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] == '%') {
      i++;
      switch (fmt[i]) {
      case 's': {
        const char *s = va_arg(args, const char *);
        if (!s)
          s = "(null)";
        vga_puts(s);
        break;
      }
      case 'd': {
        int d = va_arg(args, int);
        if (d < 0) {
          vga_putc('-');
          vga_write_dec((u64)-d);
        } else {
          vga_write_dec((u64)d);
        }
        break;
      }
      case 'u': {
        u64 u = va_arg(args, u64);
        vga_write_dec(u);
        break;
      }
      case 'x': {
        u64 x = va_arg(args, u64);
        vga_write_hex(x, 8);
        break;
      }
      case 'p': {
        void *p = va_arg(args, void *);
        vga_puts("0x");
        vga_write_hex((u64)p, 16);
        break;
      }
      case 'c': {
        char c = (char)va_arg(args, int);
        vga_putc(c);
        break;
      }
      case '%': {
        vga_putc('%');
        break;
      }
      default:
        vga_putc('%');
        vga_putc(fmt[i]);
        break;
      }
    } else {
      vga_putc(fmt[i]);
    }
  }

  va_end(args);
}
