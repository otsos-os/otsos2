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

  plane->props[DRM_PROP_PLANE_CRTC_ID] = crtc->id;
  plane->crtc_id = crtc->id;
  plane->crtc = crtc;
  crtc->primary = plane;
  crtc->connector = conn;
  conn->props[DRM_PROP_CONNECTOR_CRTC_ID] = crtc->id;
  conn->crtc = crtc;

  drivers_log("[KMS] topology: 1 connector + 1 crtc + 1 primary plane\n");
  return DRM_OK;
}

int drm_kms_state_load_current(drm_kms_state_t *out) {
  u32 i;

  if (!out) {
    return (DRM_ERR_INVAL);
  }
  memset(out, 0, sizeof(*out));
  out->plane_count = drm_plane_count();
  out->crtc_count = drm_crtc_count();
  out->connector_count = drm_connector_count();
  for (i = 0; i < out->plane_count; i++) {
    drm_plane_t *p = drm_plane_get_by_index(i);
    if (p && p->id != DRM_ID_NONE) {
      memcpy(out->plane_props[i], p->props, sizeof(p->props));
    }
  }
  for (i = 0; i < out->crtc_count; i++) {
    drm_crtc_t *c = drm_crtc_get_by_index(i);
    if (c && c->id != DRM_ID_NONE) {
      memcpy(out->crtc_props[i], c->props, sizeof(c->props));
    }
  }
  for (i = 0; i < out->connector_count; i++) {
    drm_connector_t *c = drm_connector_get_by_index(i);
    if (c && c->id != DRM_ID_NONE) {
      memcpy(out->connector_props[i], c->props, sizeof(c->props));
    }
  }
  return (DRM_OK);
}

static int state_set_prop(drm_kms_state_t *st, drm_id_t obj_id, u32 prop_id,
                          u64 value) {
  u32 idx;

  if (drm_object_get_index(obj_id, DRM_OBJECT_PLANE, &idx) == DRM_OK) {
    if (idx >= DRM_KMS_MAX_PLANES || idx >= st->plane_count) {
      return (DRM_ERR_RANGE);
    }
    if (prop_id == 0 || prop_id >= DRM_KMS_MAX_PROPS) {
      return (DRM_ERR_INVAL);
    }
    st->plane_props[idx][prop_id] = value;
    return (DRM_OK);
  }
  if (drm_object_get_index(obj_id, DRM_OBJECT_CRTC, &idx) == DRM_OK) {
    if (idx >= DRM_KMS_MAX_CRTCS || idx >= st->crtc_count) {
      return (DRM_ERR_RANGE);
    }
    if (prop_id == 0 || prop_id >= DRM_KMS_MAX_PROPS) {
      return (DRM_ERR_INVAL);
    }
    st->crtc_props[idx][prop_id] = value;
    return (DRM_OK);
  }
  if (drm_object_get_index(obj_id, DRM_OBJECT_CONNECTOR, &idx) == DRM_OK) {
    if (idx >= DRM_KMS_MAX_CONNECTORS || idx >= st->connector_count) {
      return (DRM_ERR_RANGE);
    }
    if (prop_id == 0 || prop_id >= DRM_KMS_MAX_PROPS) {
      return (DRM_ERR_INVAL);
    }
    st->connector_props[idx][prop_id] = value;
    return (DRM_OK);
  }
  return (DRM_ERR_NOENT);
}

int drm_kms_state_apply_reqs(drm_kms_state_t *st, const drm_atomic_req_t *reqs,
                               u32 count) {
  u32 i;
  int rc;

  if (!st || !reqs || count == 0) {
    return (DRM_ERR_INVAL);
  }
  for (i = 0; i < count; i++) {
    rc = state_set_prop(st, reqs[i].obj_id, reqs[i].prop_id, reqs[i].value);
    if (rc != DRM_OK) {
      drivers_log("[ATOMIC] apply req %u (obj=%u prop=%u) failed: %d\n", i,
                  reqs[i].obj_id, reqs[i].prop_id, rc);
      return (rc);
    }
  }
  return (DRM_OK);
}

static int validate_fb(drm_id_t fb_id) {
  if (fb_id == DRM_ID_NONE) {
    return (DRM_OK);
  }
  if (!drm_framebuffer_get(fb_id)) {
    return (DRM_ERR_NOENT);
  }
  return (DRM_OK);
}

static int validate_crtc(drm_id_t crtc_id) {
  if (crtc_id == DRM_ID_NONE) {
    return (DRM_OK);
  }
  if (!drm_crtc_get(crtc_id)) {
    return (DRM_ERR_NOENT);
  }
  return (DRM_OK);
}

int drm_kms_state_validate(const drm_kms_state_t *st) {
  u32 i;
  u32 pw, ph, cw, ch;
  int rc;

  if (!st) {
    return (DRM_ERR_INVAL);
  }
  for (i = 0; i < st->plane_count; i++) {
    rc = validate_fb((drm_id_t)st->plane_props[i][DRM_PROP_PLANE_FB_ID]);
    if (rc != DRM_OK) {
      drivers_log("[ATOMIC] plane %u fb invalid\n", i);
      return (rc);
    }
    rc = validate_crtc((drm_id_t)st->plane_props[i][DRM_PROP_PLANE_CRTC_ID]);
    if (rc != DRM_OK) {
      drivers_log("[ATOMIC] plane %u crtc invalid\n", i);
      return (rc);
    }
  }
  for (i = 0; i < st->connector_count; i++) {
    rc = validate_crtc((drm_id_t)st->connector_props[i][DRM_PROP_CONNECTOR_CRTC_ID]);
    if (rc != DRM_OK) {
      drivers_log("[ATOMIC] connector %u crtc invalid\n", i);
      return (rc);
    }
  }
  for (i = 0; i < st->plane_count; i++) {
    drm_id_t fb_id = (drm_id_t)st->plane_props[i][DRM_PROP_PLANE_FB_ID];
    drm_id_t crtc_id = (drm_id_t)st->plane_props[i][DRM_PROP_PLANE_CRTC_ID];
    drm_framebuffer_t *fb = (fb_id == DRM_ID_NONE) ? NULL : drm_framebuffer_get(fb_id);
    drm_crtc_t *crtc = (crtc_id == DRM_ID_NONE) ? NULL : drm_crtc_get(crtc_id);
    if (!fb || !crtc) {
      continue;
    }
    pw = fb->width;
    ph = fb->height;
    cw = (u32)crtc->props[DRM_PROP_CRTC_MODE_W];
    ch = (u32)crtc->props[DRM_PROP_CRTC_MODE_H];
    if (cw == 0 || ch == 0) {
      continue;
    }
    if (pw != cw || ph != ch) {
      drivers_log("[ATOMIC] plane %u fb %ux%u does not match crtc %ux%u\n",
                  i, pw, ph, cw, ch);
      return (DRM_ERR_RANGE);
    }
  }
  return (DRM_OK);
}

static int state_is_modeset(const drm_kms_state_t *st,
                            const drm_atomic_req_t *reqs, u32 count) {
  u32 i;

  (void)st;
  for (i = 0; i < count; i++) {
    u32 p = reqs[i].prop_id;
    if (p == DRM_PROP_PLANE_FB_ID ||
        p == DRM_PROP_PLANE_SRC_X || p == DRM_PROP_PLANE_SRC_Y ||
        p == DRM_PROP_PLANE_SRC_W || p == DRM_PROP_PLANE_SRC_H ||
        p == DRM_PROP_PLANE_DIRTY_X || p == DRM_PROP_PLANE_DIRTY_Y ||
        p == DRM_PROP_PLANE_DIRTY_W || p == DRM_PROP_PLANE_DIRTY_H) {
      continue;
    }
    return (1);
  }
  return (0);
}

static void state_sync_to_objects(const drm_kms_state_t *st) {
  u32 i;

  for (i = 0; i < st->plane_count && i < DRM_KMS_MAX_PLANES; i++) {
    drm_plane_t *p = drm_plane_get_by_index(i);
    if (!p || p->id == DRM_ID_NONE) {
      continue;
    }
    memcpy(p->props, st->plane_props[i], sizeof(p->props));
    p->fb_id = (drm_id_t)p->props[DRM_PROP_PLANE_FB_ID];
    p->crtc_id = (drm_id_t)p->props[DRM_PROP_PLANE_CRTC_ID];
    p->fb = (p->fb_id == DRM_ID_NONE) ? NULL : drm_framebuffer_get(p->fb_id);
    p->crtc = (p->crtc_id == DRM_ID_NONE) ? NULL : drm_crtc_get(p->crtc_id);
  }
  for (i = 0; i < st->crtc_count && i < DRM_KMS_MAX_CRTCS; i++) {
    drm_crtc_t *c = drm_crtc_get_by_index(i);
    if (!c || c->id == DRM_ID_NONE) {
      continue;
    }
    memcpy(c->props, st->crtc_props[i], sizeof(c->props));
    c->active = (int)c->props[DRM_PROP_CRTC_ACTIVE];
    c->mode_w = (u32)c->props[DRM_PROP_CRTC_MODE_W];
    c->mode_h = (u32)c->props[DRM_PROP_CRTC_MODE_H];
  }
  for (i = 0; i < st->connector_count && i < DRM_KMS_MAX_CONNECTORS; i++) {
    drm_connector_t *c = drm_connector_get_by_index(i);
    if (!c || c->id == DRM_ID_NONE) {
      continue;
    }
    memcpy(c->props, st->connector_props[i], sizeof(c->props));
    c->connected = (int)c->props[DRM_PROP_CONNECTOR_CONNECTED];
    c->crtc = (c->props[DRM_PROP_CONNECTOR_CRTC_ID] == DRM_ID_NONE) ?
        NULL : drm_crtc_get((drm_id_t)c->props[DRM_PROP_CONNECTOR_CRTC_ID]);
  }
}

int drm_kms_state_commit(const drm_kms_state_t *st, u32 flags) {
  const drm_driver_t *drv;
  int rc;

  if (!st) {
    return (DRM_ERR_INVAL);
  }
  drv = drm_driver_get_selected();
  if (!drv) {
    return (DRM_ERR_NODEV);
  }
  if (flags & DRM_ATOMIC_TEST_ONLY) {
    return (DRM_OK);
  }
  if (!drv->atomic_commit) {
    return (DRM_ERR_NODEV);
  }
  rc = drv->atomic_commit(st);
  if (rc != 0) {
    drivers_log("[ATOMIC] driver atomic_commit failed: %d\n", rc);
    return (DRM_ERR_NODEV);
  }
  state_sync_to_objects(st);
  return (DRM_OK);
}

int drm_atomic_commit(const drm_atomic_req_t *reqs, u32 count, u32 flags) {
  drm_kms_state_t st;
  int rc;
  int modeset;

  if (!reqs || count == 0) {
    return (DRM_ERR_INVAL);
  }

  rc = drm_kms_state_load_current(&st);
  if (rc != DRM_OK) {
    return (rc);
  }
  rc = drm_kms_state_apply_reqs(&st, reqs, count);
  if (rc != DRM_OK) {
    return (rc);
  }
  rc = drm_kms_state_validate(&st);
  if (rc != DRM_OK) {
    return (rc);
  }

  modeset = state_is_modeset(&st, reqs, count);
  if (modeset) {
    if (!(flags & DRM_ATOMIC_ALLOW_MODESET)) {
      return (DRM_ERR_INVAL);
    }
    if (!drm_auth_is_master()) {
      return (DRM_ERR_PERM);
    }
  }

  return (drm_kms_state_commit(&st, flags));
}
