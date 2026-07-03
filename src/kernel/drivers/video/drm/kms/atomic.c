/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/auth.h>
#include <drm/object.h>
#include <drm/kms/atomic.h>
#include <drm/kms/connector.h>
#include <drm/kms/crtc.h>
#include <drm/kms/framebuffer.h>
#include <drm/kms/plane.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

int drm_kms_init(void) {
  drm_plane_init_primary();
  drm_crtc_init_primary();
  drm_connector_init_primary();

  drm_plane_t *plane = drm_plane_get_primary();
  drm_crtc_t *crtc = drm_crtc_get_primary();
  drm_connector_t *conn = drm_connector_get_primary();

  plane->crtc = crtc;
  plane->crtc_id = crtc->id;
  crtc->primary = plane;
  crtc->connector = conn;
  conn->crtc = crtc;

  drivers_log("[KMS] topology: 1 connector + 1 crtc + 1 primary plane\n");
  return DRM_OK;
}

static int is_modeset(const drm_atomic_req_t *reqs, u32 count) {
  for (u32 i = 0; i < count; i++) {
    u32 p = reqs[i].prop_id;
    if (p == DRM_PROP_PLANE_FB_ID) {
      continue;
    }
    return 1;
  }
  return 0;
}

static int apply_req(const drm_atomic_req_t *req) {
  /* Resolve object type by trying each (planes/crtcs/connectors). */
  drm_plane_t *plane = drm_plane_get(req->obj_id);
  if (plane) {
    switch (req->prop_id) {
    case DRM_PROP_PLANE_FB_ID:
      return drm_plane_set_fb(plane, (drm_id_t)req->value);
    case DRM_PROP_PLANE_CRTC_ID:
      return drm_plane_set_crtc(plane, (drm_id_t)req->value);
    default:
      return DRM_ERR_INVAL;
    }
  }

  drm_crtc_t *crtc = drm_crtc_get(req->obj_id);
  if (crtc) {
    switch (req->prop_id) {
    case DRM_PROP_CRTC_ACTIVE:
      return drm_crtc_set_active(crtc, (int)req->value);
    case DRM_PROP_CRTC_MODE_W:
      crtc->mode_w = (u32)req->value;
      return DRM_OK;
    case DRM_PROP_CRTC_MODE_H:
      crtc->mode_h = (u32)req->value;
      return DRM_OK;
    default:
      return DRM_ERR_INVAL;
    }
  }

  drm_connector_t *conn = drm_connector_get(req->obj_id);
  if (conn) {
    switch (req->prop_id) {
    case DRM_PROP_CONNECTOR_CRTC:
      return DRM_OK;
    case DRM_PROP_CONNECTOR_CONN:
      return drm_connector_set_connected(conn, (int)req->value);
    default:
      return DRM_ERR_INVAL;
    }
  }

  return DRM_ERR_NOENT;
}

int drm_atomic_commit(const drm_atomic_req_t *reqs, u32 count, u32 flags) {
  (void)flags;
  if (!reqs || count == 0) {
    return DRM_ERR_INVAL;
  }

  int modeset = is_modeset(reqs, count);
  if (modeset && !drm_auth_is_master()) {
    return DRM_ERR_PERM;
  }

  for (u32 i = 0; i < count; i++) {
    int rc = apply_req(&reqs[i]);
    if (rc != DRM_OK) {
      drivers_log("[ATOMIC] req %d (obj=%u prop=%u) failed: %d\n", i,
                  reqs[i].obj_id, reqs[i].prop_id, rc);
      return rc;
    }
  }

  /* Flush the primary plane's fb to the hardware via the active driver. */
  drm_plane_t *plane = drm_plane_get_primary();
  if (plane && plane->fb) {
    const drm_driver_t *drv = drm_driver_get_selected();
    if (drv && drv->present) {
      int rc = drv->present(plane->fb);
      if (rc != 0) {
        drivers_log("[ATOMIC] present failed: %d\n", rc);
        return DRM_ERR_NODEV;
      }
    }
  }

  return DRM_OK;
}
