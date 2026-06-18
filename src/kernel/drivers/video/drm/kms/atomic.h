/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_KMS_ATOMIC_H
#define DRM_KMS_ATOMIC_H

#include <drm/drm.h>

/* Property identifiers — what an atomic request can set on an object. */
#define DRM_PROP_PLANE_FB_ID    1   /* plane -> framebuffer id */
#define DRM_PROP_PLANE_CRTC_ID  2   /* plane -> crtc id */
#define DRM_PROP_CRTC_ACTIVE    3   /* crtc -> 0/1 */
#define DRM_PROP_CRTC_MODE_W    4   /* crtc -> width */
#define DRM_PROP_CRTC_MODE_H    5   /* crtc -> height */
#define DRM_PROP_CONNECTOR_CRTC 6   /* connector -> crtc id */
#define DRM_PROP_CONNECTOR_CONN 7   /* connector -> 0/1 (connected) */

#define DRM_ATOMIC_NONBLOCK     0x01
#define DRM_ATOMIC_BLOCK        0x02
#define DRM_ATOMIC_ALLOW_MODESET 0x04

typedef struct {
  drm_id_t obj_id;
  u32 prop_id;
  u64 value;
} drm_atomic_req_t;

/* Initialize the KMS topology: register primary plane/crtc/connector. */
int drm_kms_init(void);

/* Apply a batch of property changes atomically. Returns DRM_OK on success.
 * A non-modeset commit (only PLANE_FB_ID on an active CRTC) is a page-flip
 * and does not require master. Anything else requires master. */
int drm_atomic_commit(const drm_atomic_req_t *reqs, u32 count, u32 flags);

#endif
