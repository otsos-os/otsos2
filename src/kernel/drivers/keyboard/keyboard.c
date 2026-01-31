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
    com1_write_string(
        "[KEYBOARD] no ps/2 detected (Status 0xFF).\n");
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
