/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_AUTH_H
#define DRM_AUTH_H

#include <drm/drm.h>

/* Only the DRM master can perform a modeset (change CRTC mode, connector
 * binding, primary plane fb). Non-master processes can only page-flip an
 * already-enabled primary plane. Init/kshell/tty hold master while booting. */

void drm_auth_init(void);
int drm_auth_acquire(void);   /* claim master (kernel-side) */
int drm_auth_release(void);
int drm_auth_is_master(void);

#endif
