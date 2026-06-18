/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/auth.h>

static int g_master_held;

void drm_auth_init(void) { g_master_held = 0; }

int drm_auth_acquire(void) {
  if (g_master_held) {
    return DRM_ERR_BUSY;
  }
  g_master_held = 1;
  return DRM_OK;
}

int drm_auth_release(void) {
  g_master_held = 0;
  return DRM_OK;
}

int drm_auth_is_master(void) { return g_master_held; }
