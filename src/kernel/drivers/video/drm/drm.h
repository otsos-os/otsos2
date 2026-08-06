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


#ifndef DRM_DRM_H
#define DRM_DRM_H

#include <mlibc/mlibc.h>

#define DRM_KMS_MAX_PLANES      8
#define DRM_KMS_MAX_CRTCS       4
#define DRM_KMS_MAX_CONNECTORS  4
#define DRM_KMS_MAX_PROPS       32
#define DRM_KMS_MAX_FRAMEBUFFERS 64

typedef u32 drm_id_t;       /* global object id (0 == invalid) */
typedef u32 drm_handle_t;   /* process-local GEM handle (0 == invalid) */

#define DRM_ID_NONE 0

/* Driver represents the firmware/boot linear framebuffer. */
#define DRM_DRIVER_F_BOOT_FB 0x00000001U

typedef enum {
  DRM_OBJECT_FRAMEBUFFER = 1,
  DRM_OBJECT_PLANE,
  DRM_OBJECT_CRTC,
  DRM_OBJECT_CONNECTOR,
} drm_object_type_t;

typedef enum {
  DRM_OK            = 0,
  DRM_ERR_INVAL     = -1,
  DRM_ERR_NOMEM     = -2,
  DRM_ERR_NOENT     = -3,
  DRM_ERR_BUSY      = -4,
  DRM_ERR_PERM      = -5,
  DRM_ERR_NODEV     = -6,
  DRM_ERR_RANGE     = -7,
} drm_result_t;

typedef struct drm_kms_state drm_kms_state_t;
typedef struct drm_driver drm_driver_t;
typedef struct drm_gem_buffer drm_gem_buffer_t;
typedef struct drm_framebuffer drm_framebuffer_t;
typedef struct drm_plane drm_plane_t;
typedef struct drm_crtc drm_crtc_t;
typedef struct drm_connector drm_connector_t;

struct drm_kms_state {
  u32 flags;
  u32 plane_count;
  u32 crtc_count;
  u32 connector_count;
  u64 plane_props[DRM_KMS_MAX_PLANES][DRM_KMS_MAX_PROPS];
  u64 crtc_props[DRM_KMS_MAX_CRTCS][DRM_KMS_MAX_PROPS];
  u64 connector_props[DRM_KMS_MAX_CONNECTORS][DRM_KMS_MAX_PROPS];
};

struct drm_gem_buffer {
  u8 *data;
  u64 size;
  u32 refcount;
  u32 handle;
  u64 entity;
};

struct drm_framebuffer {
  drm_id_t id;
  u32 index;
  u32 width;
  u32 height;
  u32 pitch;
  u8 bpp;
  drm_gem_buffer_t *gem;
};

struct drm_plane {
  drm_id_t id;
  u32 index;
  u32 type;            /* 0=primary, 1=cursor, 2=overlay */
  drm_id_t crtc_id;    /* bound CRTC, or DRM_ID_NONE */
  drm_id_t fb_id;      /* bound FB, or DRM_ID_NONE */
  drm_crtc_t *crtc;
  drm_framebuffer_t *fb;
  u64 props[DRM_KMS_MAX_PROPS];
};

struct drm_crtc {
  drm_id_t id;
  u32 index;
  int active;
  u32 mode_w;
  u32 mode_h;
  drm_plane_t *primary;
  drm_connector_t *connector;
  u64 props[DRM_KMS_MAX_PROPS];
};

struct drm_connector {
  drm_id_t id;
  u32 index;
  int connected;
  drm_crtc_t *crtc;
  u64 props[DRM_KMS_MAX_PROPS];
};

struct drm_driver {
  const char *name;
  int priority;
  u32 flags;
  int (*probe)(const void *boot_info);
  int (*init)(const void *boot_info);
  int (*atomic_commit)(const drm_kms_state_t *state);
  void (*shutdown)(void);
};

/* Lifecycle. */
int drm_init(const drm_driver_t *driver, const void *boot_info);
int drm_reinit(const drm_driver_t *new_driver, const void *boot_info);
int drm_is_ready(void);
const char *drm_get_driver_name(void);

/* Driver registry. Display backends register themselves from newbus attach. */
int drm_register_driver(const drm_driver_t *driver);
/* Optional provider used by UI/syscalls when they need the current boot FB. */
int drm_register_boot_info_provider(const void *(*provider)(void));
const drm_driver_t *drm_driver_get_fbdev(void);
const drm_driver_t *drm_driver_select(const void *boot_info, const char *preferred);
const drm_driver_t *drm_driver_get_selected(void);
const char *drm_driver_get_selected_name(void);
u32 drm_driver_count(void);
const drm_driver_t *drm_driver_get_by_index(u32 index);
u32 drm_driver_available_count(void);
const drm_driver_t *drm_driver_available_get(u32 index);
int drm_driver_get_selected_index(void);
int drm_driver_switch_by_id(int id);

#endif
