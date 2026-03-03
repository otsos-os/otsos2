#include <kernel/drivers/video/drm/backend.h>
#include <kernel/drivers/video/drm/driver.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static const drm_driver_t *g_selected_driver = 0;

static void drm_build_driver_list(const drm_driver_t **out, u32 *out_count) {
  static const drm_driver_t *drivers[4];
  u32 idx = 0;

  drivers[idx++] = drm_fbdev_driver_get();

  if (out) {
    for (u32 i = 0; i < idx; i++) {
      out[i] = drivers[i];
    }
  }

  if (out_count) {
    *out_count = idx;
  }
}

u32 drm_driver_count(void) {
  u32 count = 0;
  drm_build_driver_list(NULL, &count);
  return count;
}

const drm_driver_t *drm_driver_get_by_index(u32 index) {
  const drm_driver_t *drivers[4] = {0};
  u32 count = 0;
  drm_build_driver_list(drivers, &count);
  if (index >= count) {
    return NULL;
  }
  return drivers[index];
}

const drm_driver_t *drm_driver_select(const drm_boot_display_t *boot,
                                      const char *preferred_name) {
  if (!boot) {
    return 0;
  }

  const drm_driver_t *drivers[4] = {0};
  u32 count = 0;
  drm_build_driver_list(drivers, &count);

  if (preferred_name && preferred_name[0] != '\0') {
    for (u32 i = 0; i < count; i++) {
      const drm_driver_t *d = drivers[i];
      if (!d || !d->name) {
        continue;
      }
      if (strcmp(d->name, preferred_name) == 0) {
        if (!d->probe || d->probe(boot) == 0) {
          g_selected_driver = d;
          return d;
        }
        com1_printf("[DRM] preferred driver '%s' probe failed\n", preferred_name);
        return 0;
      }
    }

    com1_printf("[DRM] preferred driver '%s' not found\n", preferred_name);
    return 0;
  }

  const drm_driver_t *best = 0;
  for (u32 i = 0; i < count; i++) {
    const drm_driver_t *d = drivers[i];
    if (!d) {
      continue;
    }
    if (d->probe && d->probe(boot) != 0) {
      continue;
    }
    if (!best || d->priority > best->priority) {
      best = d;
    }
  }

  g_selected_driver = best;
  return best;
}

const drm_driver_t *drm_driver_get_selected(void) { return g_selected_driver; }

const char *drm_driver_get_selected_name(void) {
  if (!g_selected_driver) {
    return "none";
  }
  return g_selected_driver->name ? g_selected_driver->name : "unnamed";
}

int drm_driver_get_selected_index(void) {
  if (!g_selected_driver) {
    return -1;
  }

  const drm_driver_t *drivers[4] = {0};
  u32 count = 0;
  drm_build_driver_list(drivers, &count);
  for (u32 i = 0; i < count; i++) {
    if (drivers[i] == g_selected_driver) {
      return (int)i;
    }
  }

  return -1;
}
