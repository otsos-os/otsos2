#define COM1_PORT 0x3F8
#define COM1_DATA (COM1_PORT + 0)
#define COM1_INT_ENABLE (COM1_PORT + 1)
#define COM1_FIFO_CTRL (COM1_PORT + 2)
#define COM1_LINE_CTRL (COM1_PORT + 3)
#define COM1_MODEM_CTRL (COM1_PORT + 4)
#define COM1_LINE_STATUS (COM1_PORT + 5)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

static void (*mirror_callback)(char) = 0;

void com1_set_mirror_callback(void (*callback)(char)) {
  mirror_callback = callback;
}

void com1_off_mirror_callback(void) { mirror_callback = 0; }

static inline void outb(u16 port, u8 value) {
  __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
  u8 value;
  __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

void com1_init(void) {
  outb(COM1_INT_ENABLE, 0x00);
  outb(COM1_LINE_CTRL, 0x80);
  outb(COM1_DATA, 0x01);
  outb(COM1_INT_ENABLE, 0x00);
  outb(COM1_LINE_CTRL, 0x03);
  outb(COM1_FIFO_CTRL, 0xC7);
  outb(COM1_MODEM_CTRL, 0x0B);
}

void com1_write_byte(u8 byte) {
  while ((inb(COM1_LINE_STATUS) & 0x20) == 0)
    ;
  outb(COM1_DATA, byte);
  if (mirror_callback) {
    mirror_callback((char)byte);
  }
}

void com1_write_string(const char *str) {
  while (*str) {
    com1_write_byte(*str++);
  }
}

void com1_write_hex_byte(u8 value) {
  const char hex[] = "0123456789ABCDEF";
  com1_write_byte(hex[(value >> 4) & 0xF]);
  com1_write_byte(hex[value & 0xF]);
}

void com1_write_hex_word(u16 value) {
  com1_write_hex_byte((value >> 8) & 0xFF);
  com1_write_hex_byte(value & 0xFF);
}

void com1_write_hex_dword(u32 value) {
  com1_write_hex_word((value >> 16) & 0xFFFF);
  com1_write_hex_word(value & 0xFFFF);
}

void com1_write_hex_qword(u64 value) {
  com1_write_hex_dword((value >> 32) & 0xFFFFFFFF);
  com1_write_hex_dword(value & 0xFFFFFFFF);
}

void com1_newline(void) {
  com1_write_byte('\r');
  com1_write_byte('\n');
}

u8 com1_read_byte(void) {
  while ((inb(COM1_LINE_STATUS) & 0x01) == 0)
    ;
  return inb(COM1_DATA);
}

int com1_has_data(void) { return inb(COM1_LINE_STATUS) & 0x01; }

void com1_write_dec(u64 value) {
  if (value == 0) {
    com1_write_byte('0');
    return;
  }

  char buffer[32];
  int i = 0;

  while (value > 0) {
    buffer[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i > 0) {
    com1_write_byte(buffer[--i]);
  }
}

void com1_printf(const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);

  while (*format) {
    if (*format == '%') {
      format++;
      switch (*format) {
      case 's': {
        const char *str = __builtin_va_arg(args, const char *);
        com1_write_string(str);
        break;
      }
      case 'c': {
        char c = (char)__builtin_va_arg(args, int);
        com1_write_byte(c);
        break;
      }
      case 'd': {
        int val = __builtin_va_arg(args, int);
        if (val < 0) {
          com1_write_byte('-');
          val = -val;
        }
        com1_write_dec(val);
        break;
      }
      case 'u': {
        unsigned int val = __builtin_va_arg(args, unsigned int);
        com1_write_dec(val);
        break;
      }
      case 'x': {
        unsigned int val = __builtin_va_arg(args, unsigned int);
        com1_write_hex_dword(val);
        break;
      }
      case 'p': {
        void *ptr = __builtin_va_arg(args, void *);
        com1_write_string("0x");
        com1_write_hex_qword((u64)ptr);
        break;
      }
      case '%': {
        com1_write_byte('%');
        break;
      }
      }
    } else {
      com1_write_byte(*format);
    }
    format++;
  }

  __builtin_va_end(args);
}
