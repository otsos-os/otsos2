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

#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/tty.h>
#include <kernel/drivers/vga.h>
#include <lib/com1.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

#define TTY_COUNT 10
static void tty_lazy_init(void);

typedef struct {
  u16 *cells;
  int width;
  int height;
  int cursor_x;
  int cursor_y;
  u8 color;
  int ansi_state;
  int ansi_val;
} tty_state_t;

static tty_state_t ttys[TTY_COUNT];
static int tty_active = 0;
static int tty_initialized = 0;
static volatile int tty_switch_pending = -1;
static int tty_ctrl_down = 0;
static int tty_suppress_com1_mirror = 0;

static void tty_apply_ansi(tty_state_t *tty, int code) {
  u8 color_idx = 0x07;
  switch (code) {
  case 0:
    color_idx = 0x07;
    break;
  case 30:
    color_idx = 0x00;
    break;
  case 31:
    color_idx = 0x04;
    break;
  case 32:
    color_idx = 0x02;
    break;
  case 33:
    color_idx = 0x0E;
    break;
  case 34:
    color_idx = 0x01;
    break;
  case 35:
    color_idx = 0x05;
    break;
  case 36:
    color_idx = 0x03;
    break;
  case 37:
    color_idx = 0x0F;
    break;
  default:
    return;
  }
  tty->color = color_idx;
}

static void tty_redraw(const tty_state_t *tty) {
  if (!tty || !tty->cells) {
    return;
  }

  for (int y = 0; y < tty->height; y++) {
    for (int x = 0; x < tty->width; x++) {
      u16 cell = tty->cells[y * tty->width + x];
      char c = (char)(cell & 0xFF);
      u8 color = (u8)((cell >> 8) & 0xFF);
      vga_put_entry_at(c, color, x, y);
    }
  }

  vga_set_cursor(tty->cursor_x, tty->cursor_y);
  vga_set_color(tty->color);
}

static void tty_scroll(tty_state_t *tty, int active) {
  if (!tty || !tty->cells) {
    return;
  }

  if (tty->height <= 1) {
    return;
  }

  for (int y = 1; y < tty->height; y++) {
    memcpy(&tty->cells[(y - 1) * tty->width], &tty->cells[y * tty->width],
           tty->width * sizeof(u16));
  }

  u16 blank = ((u16)tty->color << 8) | ' ';
  for (int x = 0; x < tty->width; x++) {
    tty->cells[(tty->height - 1) * tty->width + x] = blank;
  }

  if (active) {
    tty_redraw(tty);
  }
}

static void tty_putc_internal(tty_state_t *tty, char c, int active) {
  if (!tty || !tty->cells) {
    return;
  }

  if (tty->ansi_state == 0) {
    if (c == 0x1B) {
      tty->ansi_state = 1;
      return;
    }
  } else if (tty->ansi_state == 1) {
    if (c == '[') {
      tty->ansi_state = 2;
      tty->ansi_val = 0;
      return;
    }
    tty->ansi_state = 0;
  } else if (tty->ansi_state == 2) {
    if (c >= '0' && c <= '9') {
      tty->ansi_val = tty->ansi_val * 10 + (c - '0');
      return;
    }
    if (c == 'm') {
      tty_apply_ansi(tty, tty->ansi_val);
      tty->ansi_state = 0;
      return;
    }
    tty->ansi_state = 0;
  }

  if (c == '\n') {
    tty->cursor_x = 0;
    tty->cursor_y++;
  } else if (c == '\r') {
    tty->cursor_x = 0;
  } else if (c == '\b') {
    if (tty->cursor_x > 0) {
      tty->cursor_x--;
    } else if (tty->cursor_y > 0) {
      tty->cursor_y--;
      tty->cursor_x = tty->width - 1;
    }
  } else {
    int x = tty->cursor_x;
    int y = tty->cursor_y;
    if (x >= 0 && y >= 0 && x < tty->width && y < tty->height) {
      tty->cells[y * tty->width + x] = ((u16)tty->color << 8) | (u8)c;
      if (active) {
        vga_put_entry_at(c, tty->color, x, y);
      }
    }
    tty->cursor_x++;
  }

  if (tty->cursor_x >= tty->width) {
    tty->cursor_x = 0;
    tty->cursor_y++;
  }

  if (tty->cursor_y >= tty->height) {
    tty_scroll(tty, active);
    tty->cursor_y = tty->height - 1;
  }

  if (active) {
    vga_set_cursor(tty->cursor_x, tty->cursor_y);
    vga_set_color(tty->color);
  }
}

static void tty_emit(char c) {
  tty_putc_internal(&ttys[tty_active], c, 1);
  tty_suppress_com1_mirror = 1;
  com1_write_byte((u8)c);
  tty_suppress_com1_mirror = 0;
}

static void tty_emit_backspace(void) {
  tty_emit('\b');
  tty_emit(' ');
  tty_emit('\b');
}

static void tty_request_switch(int index) {
  if (index < 0 || index >= TTY_COUNT) {
    return;
  }
  tty_switch_pending = index;
}

static void tty_switch_to(int index) {
  if (index < 0 || index >= TTY_COUNT) {
    return;
  }
  if (index == tty_active) {
    return;
  }

  tty_active = index;
  tty_redraw(&ttys[tty_active]);
}

void tty_set_active(int index) {
  tty_lazy_init();
  tty_switch_to(index);
}

static void tty_maybe_switch(void) {
  int target = tty_switch_pending;
  if (target < 0) {
    return;
  }
  tty_switch_pending = -1;
  tty_switch_to(target);
}

static int tty_numpad_digit(u8 scancode, int extended) {
  if (extended) {
    return -1;
  }
  switch (scancode) {
  case 0x52:
    return 0;
  case 0x4F:
    return 1;
  case 0x50:
    return 2;
  case 0x51:
    return 3;
  case 0x4B:
    return 4;
  case 0x4C:
    return 5;
  case 0x4D:
    return 6;
  case 0x47:
    return 7;
  case 0x48:
    return 8;
  case 0x49:
    return 9;
  default:
    return -1;
  }
}

static void tty_scancode_callback(u8 scancode, int released, int extended) {
  if (scancode == 0x1D) {
    tty_ctrl_down = released ? 0 : 1;
    return;
  }

  if (!released && tty_ctrl_down) {
    int digit = tty_numpad_digit(scancode, extended);
    if (digit >= 0) {
      int index = (digit == 0) ? 9 : (digit - 1);
      tty_request_switch(index);
    }
  }
}

static void tty_lazy_init(void) {
  if (!tty_initialized) {
    tty_init();
  }
}

void tty_init(void) {
  if (tty_initialized) {
    return;
  }

  int width = vga_get_width();
  int height = vga_get_height();
  if (width <= 0) {
    width = 80;
  }
  if (height <= 0) {
    height = 25;
  }

  for (int i = 0; i < TTY_COUNT; i++) {
    ttys[i].width = width;
    ttys[i].height = height;
    ttys[i].cursor_x = 0;
    ttys[i].cursor_y = 0;
    ttys[i].color = 0x07;
    ttys[i].ansi_state = 0;
    ttys[i].ansi_val = 0;
    ttys[i].cells =
        (u16 *)kcalloc((unsigned long)(width * height), sizeof(u16));
    if (ttys[i].cells) {
      u16 blank = ((u16)ttys[i].color << 8) | ' ';
      for (int j = 0; j < width * height; j++) {
        ttys[i].cells[j] = blank;
      }
    }
  }

  keyboard_set_scancode_callback(tty_scancode_callback);
  tty_initialized = 1;
  tty_redraw(&ttys[tty_active]);
}

int tty_is_initialized(void) { return tty_initialized; }

void tty_set_color(u8 color) {
  if (!tty_initialized) {
    return;
  }
  ttys[tty_active].color = color;
}

void tty_putc_from_kernel(char c) {
  if (!tty_initialized) {
    return;
  }
  tty_maybe_switch();
  tty_putc_internal(&ttys[tty_active], c, 1);
}

void tty_com1_mirror(char c) {
  if (tty_suppress_com1_mirror) {
    return;
  }
  if (!tty_initialized) {
    vga_putc(c);
    return;
  }
  tty_putc_internal(&ttys[0], c, tty_active == 0);
}

void tty_clear_active(void) {
  if (!tty_initialized) {
    return;
  }
  tty_state_t *tty = &ttys[tty_active];
  if (!tty->cells) {
    return;
  }
  u16 blank = ((u16)tty->color << 8) | ' ';
  for (int i = 0; i < tty->width * tty->height; i++) {
    tty->cells[i] = blank;
  }
  tty->cursor_x = 0;
  tty->cursor_y = 0;
  tty->ansi_state = 0;
  tty->ansi_val = 0;
  tty_redraw(tty);
}

static char tty_getchar_blocking(void) {
  char c = 0;
  while ((c = keyboard_getchar()) == 0) {
    tty_maybe_switch();
    __asm__ volatile("hlt");
  }
  tty_maybe_switch();
  return c;
}

int tty_read(void *buf, u32 count) {
  tty_lazy_init();

  if (count == 0) {
    return 0;
  }

  __asm__ volatile("sti");

  char *data = (char *)buf;
  u32 i = 0;

  while (i < count) {
    char c = tty_getchar_blocking();
    if (c == 0) {
      continue;
    }

    if (c == '\b') {
      if (i > 0) {
        i--;
        tty_emit_backspace();
      }
      continue;
    }

    data[i] = c;
    tty_emit(c);

    if (c == '\n' || c == '\r') {
      if (c == '\r') {
        data[i] = '\n';
        tty_emit('\n');
      }
      i++;
      break;
    }

    i++;
  }

  return (int)i;
}

int tty_write(const void *buf, u32 count) {
  tty_lazy_init();
  tty_maybe_switch();

  if (count == 0) {
    return 0;
  }

  const char *data = (const char *)buf;
  for (u32 i = 0; i < count; i++) {
    tty_emit(data[i]);
  }

  return (int)count;
}
