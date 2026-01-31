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

void vga_putc(char c) {
  if (is_framebuffer_enabled() != vga_using_fb)
    update_vga_dims();

  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if (c == '\r') {
    cursor_x = 0;
  } else {
    if (is_framebuffer_enabled()) {
      if (c != ' ') {
        fb_put_char(cursor_x * 8, cursor_y * 16, c, 0xFFFFFF);
      }
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
        vga_write_hex(x, 8); // Default to 8 hex digits for 32-bit-ish display
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

void vga_set_color(u8 color) { terminal_color = color; }
