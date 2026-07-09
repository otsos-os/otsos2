/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DRM_KMS_PLANE_H
#define DRM_KMS_PLANE_H

#include <drm/drm.h>
#include <drm/kms/property.h>

#define DRM_PLANE_PRIMARY 0
#define DRM_PLANE_CURSOR  1
#define DRM_PLANE_OVERLAY 2

drm_id_t drm_plane_create(u32 type);
drm_plane_t *drm_plane_get(drm_id_t id);
drm_plane_t *drm_plane_get_by_index(u32 index);
drm_plane_t *drm_plane_get_primary(void);
drm_plane_t *drm_plane_get_cursor(void);
drm_plane_t *drm_plane_find_by_fb(drm_id_t fb_id);
u32 drm_plane_count(void);

/* Bind fb / crtc on a plane (used by atomic commit). */
int drm_plane_set_fb(drm_plane_t *plane, drm_id_t fb_id);
int drm_plane_set_crtc(drm_plane_t *plane, drm_id_t crtc_id);

/* Internal: register the static primary plane. */
drm_id_t drm_plane_init_primary(void);
drm_id_t drm_plane_init_cursor(void);
void drm_plane_reset_all(void);

#endif
