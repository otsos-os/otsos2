/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_KMS_ATOMIC_H
#define DRM_KMS_ATOMIC_H

#include <drm/drm.h>
#include <drm/kms/plane.h>
#include <drm/kms/crtc.h>
#include <drm/kms/connector.h>
#include <drm/kms/property.h>

typedef struct {
  drm_id_t obj_id;
  u32 prop_id;
  u64 value;
} drm_atomic_req_t;

/* Initialize the KMS topology: register primary plane/crtc/connector. */
int drm_kms_init(void);

/* Load the current KMS state into a flat DOD structure. */
int drm_kms_state_load_current(drm_kms_state_t *out);

/* Apply a batch of property changes to a state. Returns DRM_OK on success. */
int drm_kms_state_apply_reqs(drm_kms_state_t *state,
                              const drm_atomic_req_t *reqs, u32 count);

/* Validate a state for consistency. */
int drm_kms_state_validate(const drm_kms_state_t *state);

/* Commit a validated state to hardware. */
int drm_kms_state_commit(const drm_kms_state_t *state, u32 flags);

/* Apply a batch of property changes atomically. Returns DRM_OK on success.
 * A non-modeset commit (only PLANE_FB_ID / damage on an active CRTC) is a
 * page-flip and does not require master. Anything else requires master. */
int drm_atomic_commit(const drm_atomic_req_t *reqs, u32 count, u32 flags);

#endif
