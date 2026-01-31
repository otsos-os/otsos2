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
