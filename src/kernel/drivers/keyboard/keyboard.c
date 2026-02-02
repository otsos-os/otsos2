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
#include <kernel/drivers/keyboard/ps2.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define KBD_STATUS_PORT 0x64

static keyboard_driver_t *current_driver = NULL;

static keyboard_driver_t ps2_driver = {.name = "PS/2 Keyboard",
                                       .init = ps2_keyboard_init,
                                       .getchar = ps2_keyboard_getchar,
                                       .handler = ps2_keyboard_handler};

void keyboard_manager_init() {

  u8 status = inb(KBD_STATUS_PORT);
  if (status == 0xFF) {
    com1_write_string("[KEYBOARD] no ps/2 detected (Status 0xFF).\n");
  } else {
    current_driver = &ps2_driver;

    com1_write_string("[KEYBOARD] detected: ");
    com1_write_string((char *)current_driver->name);
    com1_write_string("\n");

    com1_write_string("[KEYBOARD] switch to driver: ");
    com1_write_string((char *)current_driver->name);
    com1_write_string("\n");

    if (current_driver->init) {
      current_driver->init();
    }
  }
}

char keyboard_getchar() {
  if (current_driver && current_driver->getchar) {
    return current_driver->getchar();
  }
  return 0;
}

void keyboard_common_handler() {
  if (current_driver && current_driver->handler) {
    current_driver->handler();
  }
}

static char helper_read_char() {
  char c = 0;
  while ((c = keyboard_getchar()) == 0) {
    __asm__ volatile("nop");
  }
  return c;
}

int scanf(const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);

  int count = 0;

  while (*format) {
    if (*format == '%') {
      format++;
      if (*format == 'd') {
        int *val = __builtin_va_arg(args, int *);
        int num = 0;
        int started = 0;
        int is_neg = 0;
        char c;

        while (1) {
          c = helper_read_char();
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
        int started = 0;
        char c;
        while (1) {
          c = helper_read_char();
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
        *ch = helper_read_char();
        count++;
      } else if (*format == 'x') {
        int *val = __builtin_va_arg(args, int *);
        int num = 0;
        int started = 0;
        char c;
        while (1) {
          c = helper_read_char();
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
    }
    format++;
  }

  __builtin_va_end(args);
  return count;
}
