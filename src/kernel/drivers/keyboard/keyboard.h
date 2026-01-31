#ifndef KEYBOARD_H
#define KEYBOARD_H

typedef void (*keyboard_init_fn)();
typedef char (*keyboard_getchar_fn)();
typedef void (*keyboard_handler_fn)();

typedef struct {
  const char *name;
  keyboard_init_fn init;
  keyboard_getchar_fn getchar;
  keyboard_handler_fn handler;
} keyboard_driver_t;

void keyboard_manager_init();
char keyboard_getchar();
void keyboard_common_handler();

#endif
