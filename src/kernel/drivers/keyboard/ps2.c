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

#include <kernel/drivers/keyboard/ps2.h>
#include <mlibc/mlibc.h>

const char kbd_us[128] = {
    0,    27,   '1', '2',  '3',  '4', '5', '6', '7', '8',
    '9',  '0',  '-', '=',  '\b', /* 0x00 - 0x0E */
    '\t', 'q',  'w', 'e',  'r',  't', 'y', 'u', 'i', 'o',
    'p',  '[',  ']', '\n', /* 0x0F - 0x1C */
    0,    'a',  's', 'd',  'f',  'g', 'h', 'j', 'k', 'l',
    ';',  '\'', '`', /* 0x1D - 0x29 */
    0,    '\\', 'z', 'x',  'c',  'v', 'b', 'n', 'm', ',',
    '.',  '/',  0,                                      /* 0x2A - 0x37 */
    '*',  0,    ' ', 0,                                 /* 0x38 - 0x3B */
    0,    0,    0,   0,    0,    0,   0,   0,   0,   0, /* Function keys */
    0,                                                  /* 0x46: Scroll Lock */
    '7',  '8',  '9', '-',  '4',  '5', '6', '+', '1', '2',
    '3',  '0',  '.',          /* 0x47 - 0x53: Numpad */
    0,    0,    0,   0,    0, /* Rest are 0 */
};

const char kbd_us_caps[128] = {
    0,   27,   '!',  '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',
    '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}',  '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '\"', '~',  0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<',
    '>', '?',  0,    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,   '7', '8', '9', '-', '4', '5', '6', '+',
    '1', '2',  '3',  '0', '.', 0,   0,   0,   0,   0,
};

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

#define KB_BUFFER_SIZE 256
static char kb_buffer[KB_BUFFER_SIZE];
static int kb_head = 0;
static int kb_tail = 0;

static int shift_pressed = 0;
static int caps_lock = 0;

void ps2_keyboard_init() {
  kb_head = 0;
  kb_tail = 0;
  shift_pressed = 0;
  caps_lock = 0;
}

static void buffer_write(char c) {
  int next = (kb_head + 1) % KB_BUFFER_SIZE;
  if (next != kb_tail) {
    kb_buffer[kb_head] = c;
    kb_head = next;
  }
}

char ps2_keyboard_getchar() {
  if (kb_head == kb_tail) {
    return 0;
  }
  char c = kb_buffer[kb_tail];
  kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
  return c;
}

void ps2_keyboard_handler() {
  if (inb(KBD_STATUS_PORT) & 0x01) {
    u8 scancode = inb(KBD_DATA_PORT);

    if (scancode & 0x80) {
      scancode &= 0x7F;
      if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 0;
      }
    } else {
      if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
      } else if (scancode == 0x3A) {
        caps_lock = !caps_lock;
      } else {
        char c = 0;
        if (shift_pressed || caps_lock) {

          if (caps_lock && !shift_pressed) {
            c = kbd_us[scancode];
            if (c >= 'a' && c <= 'z') {
              c -= 32;
            }
          } else if (shift_pressed && !caps_lock) {
            c = kbd_us_caps[scancode];
          } else if (shift_pressed && caps_lock) {
            c = kbd_us_caps[scancode];
            if (c >= 'A' && c <= 'Z') {
              c += 32;
            }
          } else {
            c = kbd_us[scancode];
          }
        } else {
          c = kbd_us[scancode];
        }

        if (c != 0) {
          buffer_write(c);
        }
      }
    }
  }
}

static char ps2_read_char_blocking() {
  char c = 0;
  while ((c = ps2_keyboard_getchar()) == 0) {
    __asm__ volatile("nop");
  }
  return c;
}

int ps2Scanf(const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);

  int count = 0;

  while (*format) {
    if (*format == '%') {
      format++;
      if (*format == 'd') {
        int *val = __builtin_va_arg(args, int *);
        char buf[32];
        int i = 0;
        char c;
        while ((c = ps2_read_char_blocking()) == ' ' || c == '\n' ||
               c == '\t') {
        }

        int sign = 1;
        if (c == '-') {
          ps2_read_char_blocking();
        }

        int num = 0;
        int started = 0;

        int is_neg = 0;
        while (1) {
          c = ps2_read_char_blocking();

          if (c != '\b') {
          }

          if (c == ' ' || c == '\n' || c == '\t') {
            if (started)
              break;
            continue;
          }

          if (c == '-' && !started) {
            is_neg = 1;
            started = 1;
            continue;
          }

          if (c >= '0' && c <= '9') {
            num = num * 10 + (c - '0');
            started = 1;
          } else {
            break;
          }
        }

        if (is_neg)
          num = -num;
        *val = num;
        count++;

      } else if (*format == 's') {
        char *str = __builtin_va_arg(args, char *);
        char c;
        int started = 0;
        while (1) {
          c = ps2_read_char_blocking();
          if (c == ' ' || c == '\n' || c == '\t') {
            if (started) {
              *str = 0;
              break;
            }
            continue;
          }
          *str++ = c;
          started = 1;
        }
        count++;
      } else if (*format == 'c') {
        char *ch = __builtin_va_arg(args, char *);
        *ch = ps2_read_char_blocking();
        count++;
      } else if (*format == 'x') {
        // Hex implementation
        int *val = __builtin_va_arg(args, int *);
        int num = 0;
        int started = 0;
        char c;
        while (1) {
          c = ps2_read_char_blocking();
          if (c == ' ' || c == '\n' || c == '\t') {
            if (started)
              break;
            continue;
          }

          int digit = -1;
          if (c >= '0' && c <= '9')
            digit = c - '0';
          else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
          else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;

          if (digit != -1) {
            num = num * 16 + digit;
            started = 1;
          } else {
            break;
          }
        }
        *val = num;
        count++;
      }
    } else {
    }
    format++;
  }

  __builtin_va_end(args);
  return count;
}
