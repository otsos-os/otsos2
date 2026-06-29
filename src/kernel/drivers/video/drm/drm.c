/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/auth.h>
#include <drm/object.h>
#include <drm/gem.h>
#include <drm/init.h>
#include <drm/kms/atomic.h>
#include <drm/kms/connector.h>
#include <drm/kms/crtc.h>
#include <drm/kms/framebuffer.h>
#include <drm/kms/plane.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static const drm_driver_t *g_selected_driver;
static int g_ready;

static const drm_driver_t *g_known_drivers[8];
static u32 g_known_count;

extern const drm_driver_t *drm_fbdev_driver_get(void);
extern const drm_driver_t *drm_virtio_gpu_driver_get(void);

static void build_driver_list(void) {
  if (g_known_count > 0) {
    return;
  }
  const drm_driver_t *vgpu = drm_virtio_gpu_driver_get();
  if (vgpu && g_known_count < 8) {
    g_known_drivers[g_known_count++] = vgpu;
  }
  const drm_driver_t *fbdev = drm_fbdev_driver_get();
  if (fbdev && g_known_count < 8) {
    g_known_drivers[g_known_count++] = fbdev;
  }
}

const drm_driver_t *drm_driver_get_fbdev(void) {
  build_driver_list();
  return drm_fbdev_driver_get();
}

u32 drm_driver_count(void) {
  build_driver_list();
  return g_known_count;
}

const drm_driver_t *drm_driver_get_by_index(u32 index) {
  build_driver_list();
  if (index >= g_known_count) {
    return NULL;
  }
  return g_known_drivers[index];
}

int drm_driver_get_selected_index(void) {
  build_driver_list();
  for (u32 i = 0; i < g_known_count; i++) {
    if (g_known_drivers[i] == g_selected_driver) {
      return (int)i;
    }
  }
  return -1;
}

extern const void *drm_fbdev_get_boot_info(void);

u32
drm_driver_available_count(void)
{
	const void	*boot;
	u32		count, i;

	build_driver_list();
	boot = drm_fbdev_get_boot_info();
	count = 0;
	for (i = 0; i < g_known_count; i++) {
		const drm_driver_t	*d;

		d = g_known_drivers[i];
		if (!d) {
			continue;
		}
		if (d == g_selected_driver) {
			count++;
			continue;
		}
		if (!d->probe || d->probe(boot) == 0) {
			count++;
		}
	}
	return (count);
}

const drm_driver_t *
drm_driver_available_get(u32 index)
{
	const void	*boot;
	u32		count, i;

	build_driver_list();
	boot = drm_fbdev_get_boot_info();
	count = 0;
	for (i = 0; i < g_known_count; i++) {
		const drm_driver_t	*d;

		d = g_known_drivers[i];
		if (!d) {
			continue;
		}
		if (d == g_selected_driver) {
			if (count == index) {
				return (d);
			}
			count++;
			continue;
		}
		if (!d->probe || d->probe(boot) == 0) {
			if (count == index) {
				return (d);
			}
			count++;
		}
	}
	return (NULL);
}

const drm_driver_t *drm_driver_get_selected(void) { return g_selected_driver; }

const char *drm_driver_get_selected_name(void) {
  return g_selected_driver && g_selected_driver->name ? g_selected_driver->name
                                                      : "none";
}

const drm_driver_t *drm_driver_select(const void *boot_info,
                                      const char *preferred) {
  build_driver_list();
  if (!boot_info) {
    return NULL;
  }

  if (preferred && preferred[0]) {
    for (u32 i = 0; i < g_known_count; i++) {
      const drm_driver_t *d = g_known_drivers[i];
      if (!d || !d->name) {
        continue;
      }
      if (strcmp(d->name, preferred) == 0) {
        if (!d->probe || d->probe(boot_info) == 0) {
          g_selected_driver = d;
          return d;
        }
        com1_printf("[DRM] preferred '%s' probe failed\n", preferred);
        return NULL;
      }
    }
    com1_printf("[DRM] preferred '%s' not found\n", preferred);
    return NULL;
  }

  const drm_driver_t *best = NULL;
  for (u32 i = 0; i < g_known_count; i++) {
    const drm_driver_t *d = g_known_drivers[i];
    if (!d) {
      continue;
    }
    if (d->probe && d->probe(boot_info) != 0) {
      continue;
    }
    if (!best || d->priority > best->priority) {
      best = d;
    }
  }
  g_selected_driver = best;
  return best;
}

int drm_driver_switch_by_id(int id) {
  build_driver_list();
  if (id < 0 || (u32)id >= g_known_count) {
    return DRM_ERR_NOENT;
  }
  const drm_driver_t *d = g_known_drivers[id];
  if (!d) {
    return DRM_ERR_NOENT;
  }
  g_selected_driver = d;
  return DRM_OK;
}

int drm_is_ready(void) { return g_ready; }

const char *drm_get_driver_name(void) {
  return drm_driver_get_selected_name();
}

int drm_init(const drm_driver_t *driver, const void *boot_info) {
  if (!driver || !driver->init) {
    return DRM_ERR_INVAL;
  }

  if (driver->init(boot_info) != 0) {
    com1_write_string("[DRM] driver init failed\n");
    return DRM_ERR_NODEV;
  }

  g_selected_driver = driver;
  drm_auth_init();
  /* The kernel holds DRM master from boot. This allows kms_console_init to
   * perform the initial modeset. Userspace can acquire master via kusr. */
  drm_auth_acquire();

  /* Bring up the KMS topology: one connector, one CRTC, one primary plane. */
  drm_kms_init();

  g_ready = 1;
  com1_printf("[DRM] ready, driver '%s'\n", driver->name ? driver->name : "?");
  return DRM_OK;
}

extern void drm_object_reset_all(void);
extern void kms_kernel_console_reset(void);

/* The fbdev boot_info is stored by drm_boot_init so we can re-initialise
 * fbdev if a driver switch fails. */
extern int drm_init(const drm_driver_t *driver, const void *boot_info);

int drm_reinit(const drm_driver_t *new_driver, const void *boot_info) {
  if (!new_driver || !new_driver->init) {
    return DRM_ERR_INVAL;
  }

  const drm_driver_t *old_driver = g_selected_driver;

  com1_write_string("[DRM] reinit: trying new driver '");
  com1_write_string(new_driver->name ? new_driver->name : "?");
  com1_write_string("'\n");

  /* Try the new driver's init FIRST, before tearing anything down.
   * This way if it fails, the old display is still active. */
  if (new_driver->init(boot_info) != 0) {
    com1_write_string("[DRM] reinit: new driver init failed, keeping old\n");
    return DRM_ERR_NODEV;
  }

  com1_write_string("[DRM] reinit: new driver init OK, switching\n");

  /* New driver initialised successfully — now safe to tear down old state. */
  kms_kernel_console_reset();
  drm_object_reset_all();
  g_ready = 0;

  /* Shut down the old driver if it has a shutdown callback. */
  if (old_driver && old_driver->shutdown) {
    old_driver->shutdown();
  }

  /* Install the new driver and rebuild KMS topology. */
  g_selected_driver = new_driver;
  drm_auth_init();
  drm_auth_acquire();
  drm_kms_init();

  g_ready = 1;
  com1_printf("[DRM] reinit complete, driver '%s'\n",
              new_driver->name ? new_driver->name : "?");
  return DRM_OK;
}
