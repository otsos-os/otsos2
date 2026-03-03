#include <kernel/drivers/video/drm/atomic.h>
#include <lib/com1.h>
#include <mlibc/memory.h>

static int g_enabled = 0;
static int g_pending_active = 0;
static int g_dirty = 0;
static int g_dirty_full = 0;
static drm_rect_t g_dirty_rect = {0, 0, 0, 0};

static const drm_driver_t *g_driver = 0;
static drm_framebuffer_t g_hw_fb;
static drm_framebuffer_t g_shadow_fb;
static drm_atomic_state_t g_current;
static drm_atomic_state_t g_pending;

static void drm_reset_dirty_state(void) {
  g_dirty = 0;
  g_dirty_full = 0;
  g_dirty_rect.x = 0;
  g_dirty_rect.y = 0;
  g_dirty_rect.width = 0;
  g_dirty_rect.height = 0;
}

static void drm_union_dirty_rect(u32 x, u32 y, u32 width, u32 height) {
  if (!width || !height) {
    return;
  }

  u32 x2 = x + width;
  u32 y2 = y + height;

  if (g_dirty_rect.width == 0 || g_dirty_rect.height == 0) {
    g_dirty_rect.x = x;
    g_dirty_rect.y = y;
    g_dirty_rect.width = width;
    g_dirty_rect.height = height;
    return;
  }

  u32 cur_x1 = g_dirty_rect.x;
  u32 cur_y1 = g_dirty_rect.y;
  u32 cur_x2 = g_dirty_rect.x + g_dirty_rect.width;
  u32 cur_y2 = g_dirty_rect.y + g_dirty_rect.height;

  if (x < cur_x1) {
    cur_x1 = x;
  }
  if (y < cur_y1) {
    cur_y1 = y;
  }
  if (x2 > cur_x2) {
    cur_x2 = x2;
  }
  if (y2 > cur_y2) {
    cur_y2 = y2;
  }

  g_dirty_rect.x = cur_x1;
  g_dirty_rect.y = cur_y1;
  g_dirty_rect.width = cur_x2 - cur_x1;
  g_dirty_rect.height = cur_y2 - cur_y1;
}

static int drm_atomic_validate(const drm_atomic_state_t *state) {
  if (!state) {
    return -1;
  }

  if (!state->connector.connected) {
    return -1;
  }

  if (!state->crtc.active || state->crtc.mode_width == 0 ||
      state->crtc.mode_height == 0) {
    return -1;
  }

  if (!state->primary_plane.enabled || !state->primary_plane.fb) {
    return -1;
  }

  drm_framebuffer_t *fb = state->primary_plane.fb;
  if (!fb->data || fb->width == 0 || fb->height == 0 || fb->pitch == 0 ||
      fb->bpp == 0) {
    return -1;
  }

  if (fb->width != state->crtc.mode_width ||
      fb->height != state->crtc.mode_height) {
    return -1;
  }

  return 0;
}

int drm_atomic_init(const drm_driver_t *driver, const drm_boot_display_t *boot) {
  if (!driver || !driver->init || !driver->present || !boot) {
    return -1;
  }

  drm_framebuffer_t new_hw_fb;
  if (driver->init(boot, &new_hw_fb) != 0) {
    com1_write_string("[DRM] driver init failed\n");
    return -1;
  }

  u64 shadow_bytes = (u64)new_hw_fb.pitch * (u64)new_hw_fb.height;
  u8 *new_shadow_mem = (u8 *)kcalloc((unsigned long)shadow_bytes, 1);
  if (!new_shadow_mem) {
    com1_write_string("[DRM] shadow fb alloc failed\n");
    return -1;
  }

  drm_framebuffer_t new_shadow_fb = {
      .width = new_hw_fb.width,
      .height = new_hw_fb.height,
      .pitch = new_hw_fb.pitch,
      .bpp = new_hw_fb.bpp,
      .data = new_shadow_mem,
      .hw_address = 0,
  };

  drm_atomic_state_t new_state = {
      .crtc =
          {
              .active = 1,
              .mode_width = new_shadow_fb.width,
              .mode_height = new_shadow_fb.height,
          },
      .primary_plane =
          {
              .enabled = 1,
              .fb = &new_shadow_fb,
          },
      .connector =
          {
              .connected = 1,
          },
  };

  if (driver->present(&new_shadow_fb, &new_hw_fb) != 0) {
    kfree(new_shadow_mem);
    com1_write_string("[DRM] initial present failed\n");
    return -1;
  }

  if (g_shadow_fb.data) {
    kfree(g_shadow_fb.data);
  }

  g_driver = driver;
  g_hw_fb = new_hw_fb;
  g_shadow_fb = new_shadow_fb;
  g_current = new_state;
  g_current.primary_plane.fb = &g_shadow_fb;
  g_pending = g_current;
  g_pending_active = 0;
  drm_reset_dirty_state();
  g_enabled = 1;

  return 0;
}

int drm_atomic_begin(void) {
  if (!g_enabled) {
    return -1;
  }

  g_pending = g_current;
  g_pending_active = 1;
  return 0;
}

int drm_atomic_set_mode(u32 width, u32 height) {
  if (!g_enabled || !g_pending_active) {
    return -1;
  }

  g_pending.crtc.mode_width = width;
  g_pending.crtc.mode_height = height;
  return 0;
}

int drm_atomic_set_connector(int connected) {
  if (!g_enabled || !g_pending_active) {
    return -1;
  }

  g_pending.connector.connected = connected ? 1 : 0;
  return 0;
}

int drm_atomic_set_primary_fb(drm_framebuffer_t *fb) {
  if (!g_enabled || !g_pending_active || !fb) {
    return -1;
  }

  g_pending.primary_plane.fb = fb;
  g_pending.primary_plane.enabled = 1;
  return 0;
}

void drm_atomic_mark_dirty(void) {
  if (!g_enabled) {
    return;
  }
  g_dirty = 1;
  g_dirty_full = 1;
}

void drm_atomic_mark_dirty_rect(u32 x, u32 y, u32 width, u32 height) {
  if (!g_enabled) {
    return;
  }
  g_dirty = 1;
  if (g_dirty_full) {
    return;
  }
  drm_union_dirty_rect(x, y, width, height);
}

int drm_atomic_commit(void) {
  if (!g_enabled || !g_driver || !g_driver->present) {
    return -1;
  }

  drm_atomic_state_t *target = g_pending_active ? &g_pending : &g_current;
  if (drm_atomic_validate(target) != 0) {
    g_pending_active = 0;
    return -1;
  }

  if (g_pending_active || g_dirty) {
    int rc = 0;
    if (!g_pending_active && !g_dirty_full && g_driver->present_rect &&
        g_dirty_rect.width > 0 && g_dirty_rect.height > 0) {
      rc = g_driver->present_rect(target->primary_plane.fb, &g_hw_fb,
                                  &g_dirty_rect);
    } else {
      rc = g_driver->present(target->primary_plane.fb, &g_hw_fb);
    }
    if (rc != 0) {
      g_pending_active = 0;
      return -1;
    }
    g_current = *target;
  }

  g_pending_active = 0;
  drm_reset_dirty_state();
  return 0;
}

int drm_atomic_is_enabled(void) { return g_enabled; }

int drm_atomic_is_ready(void) {
  return g_enabled && g_current.connector.connected && g_current.crtc.active &&
         g_current.primary_plane.enabled && g_current.primary_plane.fb &&
         g_current.primary_plane.fb->data;
}

const char *drm_atomic_get_driver_name(void) {
  if (!g_driver || !g_driver->name) {
    return "none";
  }
  return g_driver->name;
}

u32 drm_atomic_get_width(void) {
  if (!g_enabled) {
    return 0;
  }
  return g_current.crtc.mode_width;
}

u32 drm_atomic_get_height(void) {
  if (!g_enabled) {
    return 0;
  }
  return g_current.crtc.mode_height;
}

u32 drm_atomic_get_pitch(void) {
  if (!g_enabled) {
    return 0;
  }
  return g_hw_fb.pitch;
}

u8 drm_atomic_get_bpp(void) {
  if (!g_enabled) {
    return 0;
  }
  return g_hw_fb.bpp;
}

u64 drm_atomic_get_hw_address(void) {
  if (!g_enabled) {
    return 0;
  }
  return g_hw_fb.hw_address;
}

drm_framebuffer_t *drm_atomic_get_draw_fb(void) {
  if (!g_enabled) {
    return 0;
  }
  return g_current.primary_plane.fb;
}
