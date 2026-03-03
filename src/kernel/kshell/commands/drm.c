#include <kernel/drivers/video/drm/init.h>
#include <kernel/kshell/kshell.h>

static int parse_nonneg_int(const char *s, int *ok) {
  int value = 0;
  int i = 0;

  *ok = 0;
  if (!s || s[0] == '\0') {
    return 0;
  }

  while (s[i] != '\0') {
    if (s[i] < '0' || s[i] > '9') {
      return 0;
    }
    value = (value * 10) + (s[i] - '0');
    i++;
  }

  *ok = 1;
  return value;
}

int kshell_drm_switch_command(int argc, char *argv[]) {
  if (argc != 2) {
    kshell_console_write("drm_switch: usage: drm_switch <id>\n");
    return -1;
  }

  int ok = 0;
  int id = parse_nonneg_int(argv[1], &ok);
  if (!ok) {
    kshell_console_write("drm_switch: invalid id\n");
    return -1;
  }

  if (drm_switch_driver_by_id(id) != 0) {
    kshell_console_write("drm_switch: failed to switch drm driver\n");
    return -1;
  }

  kshell_console_write("drm_switch: active driver: ");
  kshell_console_write(drm_get_active_driver_name());
  kshell_console_write(" (id=");
  kshell_console_write_int(drm_get_active_driver_id());
  kshell_console_write(")\n");

  return 0;
}
