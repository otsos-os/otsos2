#include <kernel/drivers/vga.h>
#include <mlibc/mlibc.h>
#include <stdarg.h>

static u16 *vga_buffer = (u16 *)0xB8000;
static int cursor_x = 0;
static int cursor_y = 0;
static u8 terminal_color = 0x07;

void clear_scr() {
  for (int y = 0; y < 25; y++) {
    for (int x = 0; x < 80; x++) {
      vga_buffer[y * 80 + x] = (u16)terminal_color << 8 | ' ';
    }
  }
  cursor_x = 0;
  cursor_y = 0;
}

void scroll_scr() {
  for (int y = 0; y < 24; y++) {
    for (int x = 0; x < 80; x++) {
      vga_buffer[y * 80 + x] = vga_buffer[(y + 1) * 80 + x];
    }
  }
  for (int x = 0; x < 80; x++) {
    vga_buffer[24 * 80 + x] = (u16)terminal_color << 8 | ' ';
  }
  if (cursor_y > 0)
    cursor_y--;
}

void vga_putc(char c) {
  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if (c == '\r') {
    cursor_x = 0;
  } else {
    vga_buffer[cursor_y * 80 + cursor_x] = (u16)terminal_color << 8 | c;
    cursor_x++;
  }

  if (cursor_x >= 80) {
    cursor_x = 0;
    cursor_y++;
  }

  if (cursor_y >= 25) {
    scroll_scr();
    cursor_y = 24;
  }
}

void vga_puts(const char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    vga_putc(str[i]);
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
        vga_puts(s);
        break;
      }
      case 'd': {
        int d = va_arg(args, int);
        char buf[32];
        itoa(d, buf, 10);
        vga_puts(buf);
        break;
      }
      case 'x': {
        int x = va_arg(args, int);
        char buf[32];
        itoa(x, buf, 16);
        vga_puts(buf);
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

void vga_set_color(u8 color) { terminal_color = color; }
