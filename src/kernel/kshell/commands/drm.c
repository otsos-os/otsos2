#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/tty.h>
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

  const drm_driver_t *drv = drm_driver_get_by_index((u32)id);
  if (!drv) {
    kshell_console_write("drm_switch: no driver with id ");
    kshell_console_write_int(id);
    kshell_console_write("\n");
    return -1;
  }

  if (drv == drm_driver_get_selected()) {
    kshell_console_write("drm_switch: already active: ");
    kshell_console_write(drv->name ? drv->name : "unnamed");
    kshell_console_write("\n");
    return 0;
  }

  kshell_console_write("drm_switch: switching to ");
  kshell_console_write(drv->name ? drv->name : "unnamed");
  kshell_console_write("...\n");

  int rc = drm_reinit(drv, NULL);
  if (rc != 0) {
    kshell_console_write("drm_switch: failed (error ");
    kshell_console_write_int(rc);
    kshell_console_write(")\n");
    return -1;
  }

  tty_reinit();

  kshell_console_write("drm_switch: active driver: ");
  kshell_console_write(drm_driver_get_selected_name());
  kshell_console_write(" (id=");
  kshell_console_write_int(drm_driver_get_selected_index());
  kshell_console_write(")\n");

  return 0;
}
