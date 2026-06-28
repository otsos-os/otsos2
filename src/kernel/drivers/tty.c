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

#include <kernel/console.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/tty.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/video/drm/kms/console.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define TTY_COUNT 10
#define TTY_LINE_BUF_SIZE 256
static void tty_lazy_init(void);
void tty_update(void);

typedef struct {
  u16 *cells;
  int width;
  int height;
  int cursor_x;
  int cursor_y;
  u8 color;
  u32 fg_rgb;
  int ansi_state;
  int ansi_params[8];
  int ansi_param_count;
  int ansi_cur_param;
} tty_state_t;

typedef struct {
  char data[TTY_LINE_BUF_SIZE];
  u32 len;
  u32 read_pos;
} tty_line_buf_t;

typedef struct {
  char buf[1024];
  int head;
  int tail;
} tty_input_queue_t;

static tty_state_t ttys[TTY_COUNT];
static tty_line_buf_t tty_line_bufs[TTY_COUNT];
static tty_input_queue_t tty_inputs[TTY_COUNT];
static int tty_active = 0;
static int tty_initialized = 0;
static volatile int tty_switch_pending = -1;
static int tty_ctrl_down = 0;
static int tty_suppress_com1_mirror = 0;

static u64 tty_indicator_end_time = 0;
static int tty_indicator_active = 0;

static u32 tty_palette[16] = {0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
                              0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
                              0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
                              0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF};

/* Use the shared kernel console singleton. */
static kms_console_t *g_con;

static kms_console_t *tty_con(void) {
  if (g_con) return g_con;
  g_con = kms_kernel_console();
  return g_con;
}

static void tty_draw_cell(int x, int y, char c, u8 color) {
  kms_console_t *con = tty_con();
  if (!con) return;
  u32 rgb;
  tty_state_t *tty = &ttys[tty_active];
  if (tty->fg_rgb != 0xFFFFFFFF)
    rgb = tty->fg_rgb;
  else
    rgb = tty_palette[color & 0x0F];
  kms_console_glyph(con, (u32)(x * 8), (u32)(y * 16), c, rgb, 0x000000);
}

static void tty_set_hw_cursor(const tty_state_t *tty) {
  (void)tty;
}

static void tty_ansi_sgr(tty_state_t *tty, int params[], int count) {
  int i = 0;
  while (i < count) {
    int code = params[i];

    if (code == 38 && i + 3 < count && params[i + 1] == 2) {
      int r = params[i + 2];
      int g = params[i + 3];
      int b = params[i + 4];
      tty->fg_rgb = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
      i += 5;
      continue;
    }

    if (code == 39) {
      tty->fg_rgb = 0xFFFFFFFF;
      tty->color = 0x07;
      i++;
      continue;
    }

    u8 color_idx = 0x07;
    switch (code) {
    case 0:  color_idx = 0x07; break;
    case 30: color_idx = 0x00; break;
    case 31: color_idx = 0x04; break;
    case 32: color_idx = 0x02; break;
    case 33: color_idx = 0x0E; break;
    case 34: color_idx = 0x01; break;
    case 35: color_idx = 0x05; break;
    case 36: color_idx = 0x03; break;
    case 37: color_idx = 0x0F; break;
    case 90: color_idx = 0x08; break;
    case 91: color_idx = 0x0C; break;
    case 92: color_idx = 0x0A; break;
    case 93: color_idx = 0x0E; break;
    case 94: color_idx = 0x09; break;
    case 95: color_idx = 0x0D; break;
    case 96: color_idx = 0x0B; break;
    case 97: color_idx = 0x0F; break;
    default: i++; continue;
    }
    tty->fg_rgb = 0xFFFFFFFF;
    tty->color = color_idx;
    i++;
  }
}

static void tty_erase_line(tty_state_t *tty, int mode, int active) {
  if (!tty || !tty->cells) return;
  int y = tty->cursor_y;
  if (y < 0 || y >= tty->height) return;

  int from, to;
  switch (mode) {
  case 0: from = tty->cursor_x; to = tty->width; break;
  case 1: from = 0; to = tty->cursor_x + 1; break;
  case 2: from = 0; to = tty->width; break;
  default: return;
  }

  u16 blank = ((u16)tty->color << 8) | ' ';
  for (int x = from; x < to && x < tty->width; x++) {
    tty->cells[y * tty->width + x] = blank;
    if (active) tty_draw_cell(x, y, ' ', tty->color);
  }
}

static void tty_erase_display(tty_state_t *tty, int mode, int active) {
  if (!tty || !tty->cells) return;

  u16 blank = ((u16)tty->color << 8) | ' ';
  switch (mode) {
  case 0:
    tty_erase_line(tty, 0, active);
    for (int y = tty->cursor_y + 1; y < tty->height; y++) {
      for (int x = 0; x < tty->width; x++) {
        tty->cells[y * tty->width + x] = blank;
        if (active) tty_draw_cell(x, y, ' ', tty->color);
      }
    }
    break;
  case 1:
    for (int y = 0; y < tty->cursor_y; y++) {
      for (int x = 0; x < tty->width; x++) {
        tty->cells[y * tty->width + x] = blank;
        if (active) tty_draw_cell(x, y, ' ', tty->color);
      }
    }
    tty_erase_line(tty, 1, active);
    break;
  case 2:
    for (int y = 0; y < tty->height; y++) {
      for (int x = 0; x < tty->width; x++) {
        tty->cells[y * tty->width + x] = blank;
        if (active) tty_draw_cell(x, y, ' ', tty->color);
      }
    }
    tty->cursor_x = 0;
    tty->cursor_y = 0;
    break;
  }
  if (active) tty_set_hw_cursor(tty);
}

static int tty_ansi_param(tty_state_t *tty, int idx, int def) {
  if (idx < tty->ansi_param_count) return tty->ansi_params[idx];
  return def;
}

static void tty_ansi_execute(tty_state_t *tty, char cmd, int active) {
  switch (cmd) {
  case 'A': {
    int n = tty_ansi_param(tty, 0, 1);
    if (n <= 0) n = 1;
    tty->cursor_y -= n;
    if (tty->cursor_y < 0) tty->cursor_y = 0;
    break;
  }
  case 'B': {
    int n = tty_ansi_param(tty, 0, 1);
    if (n <= 0) n = 1;
    tty->cursor_y += n;
    if (tty->cursor_y >= tty->height) tty->cursor_y = tty->height - 1;
    break;
  }
  case 'C': {
    int n = tty_ansi_param(tty, 0, 1);
    if (n <= 0) n = 1;
    tty->cursor_x += n;
    if (tty->cursor_x >= tty->width) tty->cursor_x = tty->width - 1;
    break;
  }
  case 'D': {
    int n = tty_ansi_param(tty, 0, 1);
    if (n <= 0) n = 1;
    tty->cursor_x -= n;
    if (tty->cursor_x < 0) tty->cursor_x = 0;
    break;
  }
  case 'H':
  case 'f': {
    int row = tty_ansi_param(tty, 0, 1);
    int col = tty_ansi_param(tty, 1, 1);
    if (row <= 0) row = 1;
    if (col <= 0) col = 1;
    tty->cursor_y = row - 1;
    tty->cursor_x = col - 1;
    if (tty->cursor_y >= tty->height) tty->cursor_y = tty->height - 1;
    if (tty->cursor_x >= tty->width) tty->cursor_x = tty->width - 1;
    break;
  }
  case 'J': {
    int mode = tty_ansi_param(tty, 0, 0);
    tty_erase_display(tty, mode, active);
    return;
  }
  case 'K': {
    int mode = tty_ansi_param(tty, 0, 0);
    tty_erase_line(tty, mode, active);
    return;
  }
  case 'm': {
    tty_ansi_sgr(tty, tty->ansi_params, tty->ansi_param_count);
    break;
  }
  }
  tty_set_hw_cursor(tty);
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
      tty_draw_cell(x, y, c, color);
    }
  }
  if (g_con) kms_console_flush(g_con);

  tty_set_hw_cursor(tty);
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
      tty->ansi_param_count = 0;
      tty->ansi_cur_param = 0;
      return;
    }
    tty->ansi_state = 0;
  } else if (tty->ansi_state == 2) {
    if (c >= '0' && c <= '9') {
      tty->ansi_cur_param = tty->ansi_cur_param * 10 + (c - '0');
      return;
    }
    if (c == ';') {
      if (tty->ansi_param_count < 8)
        tty->ansi_params[tty->ansi_param_count++] = tty->ansi_cur_param;
      tty->ansi_cur_param = 0;
      return;
    }
    if (tty->ansi_param_count < 8)
      tty->ansi_params[tty->ansi_param_count++] = tty->ansi_cur_param;
    tty_ansi_execute(tty, c, active);
    tty->ansi_state = 0;
    return;
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
        tty_draw_cell(x, y, c, tty->color);
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
    tty_set_hw_cursor(tty);
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

#include <kernel/drivers/timer.h>

static void tty_draw_indicator(int index) {
  int x = ttys[index].width - 15;
  int y = 0;
  if (x < 0)
    x = 0;

  char buf[16];
  buf[0] = 'T';
  buf[1] = 'T';
  buf[2] = 'Y';
  buf[3] = ' ';
  int tty_num = index + 1;
  if (tty_num >= 10) {
    buf[4] = '1';
    buf[5] = '0';
    buf[6] = '\0';
  } else {
    buf[4] = '0' + tty_num;
    buf[5] = '\0';
  }

  for (int i = 0; buf[i]; i++) {
    tty_draw_cell(x + i, y, buf[i], 0x0A);
  }
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

  tty_indicator_end_time = timer_get_ticks() + timer_get_frequency();
  tty_indicator_active = 1;
  tty_draw_indicator(index);
}

void tty_set_active(int index) {
  tty_lazy_init();
  tty_switch_to(index);
}

void tty_restore_active_display(void) {
  if (!tty_initialized) {
    return;
  }

  tty_redraw(&ttys[tty_active]);
  if (tty_indicator_active) {
    tty_draw_indicator(tty_active);
  }
}

void tty_update(void) {
  if (tty_indicator_active && timer_get_ticks() >= tty_indicator_end_time) {
    tty_indicator_active = 0;
    tty_redraw(&ttys[tty_active]);
  }

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

  int width = 0;
  int height = 0;
  if (tty_con()) {
    width = (int)g_con->cols;
    height = (int)g_con->rows;
  }
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
    ttys[i].fg_rgb = 0xFFFFFFFF;
    ttys[i].ansi_params[0] = 0;
    ttys[i].ansi_params[1] = 0;
    ttys[i].ansi_params[2] = 0;
    ttys[i].ansi_params[3] = 0;
    ttys[i].ansi_params[4] = 0;
    ttys[i].ansi_params[5] = 0;
    ttys[i].ansi_params[6] = 0;
    ttys[i].ansi_params[7] = 0;
    ttys[i].ansi_param_count = 0;
    ttys[i].ansi_cur_param = 0;
    ttys[i].cells =
        (u16 *)kmem_calloc((unsigned long)(width * height), sizeof(u16));
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

void tty_reinit(void) {
  if (!tty_initialized) {
    tty_init();
    return;
  }

  /* Force the kernel console to re-initialise with the new display geometry. */
  kms_kernel_console_reset();
  g_con = NULL;
  g_con = kms_kernel_console();
  if (!g_con) {
    return;
  }

  int new_w = (int)g_con->cols;
  int new_h = (int)g_con->rows;
  if (new_w <= 0) new_w = 80;
  if (new_h <= 0) new_h = 25;

  for (int i = 0; i < TTY_COUNT; i++) {
    int old_w = ttys[i].width;
    int old_h = ttys[i].height;
    u16 *old_cells = ttys[i].cells;

    ttys[i].width = new_w;
    ttys[i].height = new_h;
    ttys[i].cells =
        (u16 *)kmem_calloc((unsigned long)(new_w * new_h), sizeof(u16));
    if (ttys[i].cells) {
      u16 blank = ((u16)ttys[i].color << 8) | ' ';
      for (int j = 0; j < new_w * new_h; j++) {
        ttys[i].cells[j] = blank;
      }
      /* Copy as much old content as fits into the new buffer. */
      if (old_cells) {
        int copy_w = old_w < new_w ? old_w : new_w;
        int copy_h = old_h < new_h ? old_h : new_h;
        for (int y = 0; y < copy_h; y++) {
          memcpy(&ttys[i].cells[y * new_w], &old_cells[y * old_w],
                 (unsigned long)copy_w * sizeof(u16));
        }
        kmem_free(old_cells);
      }
    } else if (old_cells) {
      kmem_free(old_cells);
    }

    /* Clamp cursor to new bounds. */
    if (ttys[i].cursor_x >= new_w) ttys[i].cursor_x = 0;
    if (ttys[i].cursor_y >= new_h) ttys[i].cursor_y = 0;
  }

  /* Redraw all ttys so each one is ready when the user switches to it. */
  for (int i = 0; i < TTY_COUNT; i++) {
    tty_redraw(&ttys[i]);
  }

  /* Draw the indicator on the active tty. */
  tty_indicator_end_time = timer_get_ticks() + timer_get_frequency();
  tty_indicator_active = 1;
  if (g_con) kms_console_flush(g_con);
  tty_draw_indicator(tty_active);
}

void tty_set_color(u8 color) {
  if (!tty_initialized) {
    return;
  }
  ttys[tty_active].color = color;
}

void tty_flush_kernel(void) {
  if (g_con) kms_console_flush(g_con);
}

void tty_putc_from_kernel(char c) {
  if (!tty_initialized) {
    return;
  }
  tty_update();
  tty_putc_internal(&ttys[tty_active], c, 1);
  /* Batch flush: only present to hardware on newline, not every character.
   * This avoids a ~3 MB memcpy per glyph. */
  if (c == '\n') {
    tty_flush_kernel();
  }
}

void tty_com1_mirror(char c) {
  if (tty_suppress_com1_mirror) {
    return;
  }
  if (!tty_initialized) {
    console_putchar(c);
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
  tty->fg_rgb = 0xFFFFFFFF;
  tty->ansi_params[0] = 0;
  tty->ansi_params[1] = 0;
  tty->ansi_params[2] = 0;
  tty->ansi_params[3] = 0;
  tty->ansi_params[4] = 0;
  tty->ansi_params[5] = 0;
  tty->ansi_params[6] = 0;
  tty->ansi_params[7] = 0;
  tty->ansi_param_count = 0;
  tty->ansi_cur_param = 0;
  tty_redraw(tty);
}

static void tty_pump_keyboard(void) {
  while (1) {
    char c = keyboard_getchar();
    if (c == 0) {
      break;
    }
    tty_input_queue_t *q = &tty_inputs[tty_active];
    int next = (q->head + 1) % 256;
    if (next != q->tail) {
      q->buf[q->head] = c;
      q->head = next;
    }
  }
}

static char tty_getchar_blocking(int tty_idx) {
  while (1) {
    tty_update();
    tty_pump_keyboard();
    tty_input_queue_t *q = &tty_inputs[tty_idx];
    if (q->head != q->tail) {
      char c = q->buf[q->tail];
      q->tail = (q->tail + 1) % 256;
      tty_update();
      return c;
    }
    __asm__ volatile("hlt");
  }
}

static void tty_emit_to(int tty_idx, char c) {
  tty_putc_internal(&ttys[tty_idx], c, tty_idx == tty_active);
  if (tty_idx == tty_active) {
    tty_suppress_com1_mirror = 1;
    com1_write_byte((u8)c);
    tty_suppress_com1_mirror = 0;
  }
}

static void tty_fill_line_buffer(int tty_idx) {
  tty_line_buf_t *line = &tty_line_bufs[tty_idx];
  if (!line) {
    return;
  }

  line->len = 0;
  line->read_pos = 0;

  while (1) {
    char c = tty_getchar_blocking(tty_idx);
    if (c == 0) {
      continue;
    }

    if (c == '\b' || c == 0x7F) {
      if (line->len > 0) {
        line->len--;
        tty_emit_to(tty_idx, '\b');
        tty_emit_to(tty_idx, ' ');
        tty_emit_to(tty_idx, '\b');
        if (tty_idx == tty_active && g_con) kms_console_flush(g_con);
      }
      continue;
    }

    if (c == '\n' || c == '\r') {
      if (line->len < (TTY_LINE_BUF_SIZE - 1)) {
        line->data[line->len++] = '\n';
      }
      tty_emit_to(tty_idx, '\n');
      if (tty_idx == tty_active && g_con) kms_console_flush(g_con);
      break;
    }

    if (line->len < (TTY_LINE_BUF_SIZE - 1)) {
      line->data[line->len++] = c;
      tty_emit_to(tty_idx, c);
      if (tty_idx == tty_active && g_con) kms_console_flush(g_con);
    }
  }
}

int tty_read(void *buf, u32 count) {
  tty_lazy_init();

  if (count == 0) {
    return 0;
  }

  __asm__ volatile("sti");

  int tty_idx = tty_active;
  tty_line_buf_t *line = &tty_line_bufs[tty_idx];
  if (line->read_pos >= line->len) {
    line->len = 0;
    line->read_pos = 0;
    tty_fill_line_buffer(tty_idx);
  }

  u32 available = line->len - line->read_pos;
  u32 to_copy = count;
  if (to_copy > available) {
    to_copy = available;
  }

  if (to_copy > 0) {
    memcpy(buf, line->data + line->read_pos, to_copy);
    line->read_pos += to_copy;
  }

  if (line->read_pos >= line->len) {
    line->len = 0;
    line->read_pos = 0;
  }

  return (int)to_copy;
}

int tty_write(const void *buf, u32 count) {
  tty_lazy_init();
  tty_update();

  if (count == 0) {
    return 0;
  }

  const char *data = (const char *)buf;
  for (u32 i = 0; i < count; i++) {
    tty_emit(data[i]);
  }

  /* Flush once after the entire write — this covers prompts that don't
   * end with \n (e.g. "/ $ " or "Enter path: "). */
  if (g_con) kms_console_flush(g_con);

  return (int)count;
}
