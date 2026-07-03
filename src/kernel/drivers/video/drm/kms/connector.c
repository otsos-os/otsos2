/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/object.h>
#include <drm/kms/connector.h>
#include <drm/kms/crtc.h>
#include <mlibc/mlibc.h>

static drm_connector_t g_connectors[DRM_KMS_MAX_CONNECTORS];
static u32 g_connector_count;

static u32 connector_alloc_index(void) {
  for (u32 i = 0; i < DRM_KMS_MAX_CONNECTORS; i++) {
    if (g_connectors[i].id == DRM_ID_NONE) {
      return i;
    }
  }
  return DRM_KMS_MAX_CONNECTORS;
}

static void connector_sync_from_props(drm_connector_t *conn) {
  conn->connected = (int)conn->props[DRM_PROP_CONNECTOR_CONNECTED];
  conn->crtc = NULL;
  if (conn->props[DRM_PROP_CONNECTOR_CRTC_ID] != DRM_ID_NONE) {
    conn->crtc = drm_crtc_get((drm_id_t)conn->props[DRM_PROP_CONNECTOR_CRTC_ID]);
  }
}

drm_id_t drm_connector_create(void) {
  u32 idx = connector_alloc_index();
  if (idx >= DRM_KMS_MAX_CONNECTORS) {
    return DRM_ID_NONE;
  }
  drm_connector_t *c = &g_connectors[idx];
  memset(c, 0, sizeof(*c));
  c->index = idx;
  c->props[DRM_PROP_CONNECTOR_CONNECTED] = 1;
  c->props[DRM_PROP_CONNECTOR_CRTC_ID] = DRM_ID_NONE;
  drm_id_t id = drm_object_register(DRM_OBJECT_CONNECTOR, c, idx);
  if (id == DRM_ID_NONE) {
    memset(c, 0, sizeof(*c));
    return DRM_ID_NONE;
  }
  c->id = id;
  connector_sync_from_props(c);
  if (idx + 1 > g_connector_count) {
    g_connector_count = idx + 1;
  }
  return id;
}

drm_connector_t *drm_connector_get(drm_id_t id) {
  return (drm_connector_t *)drm_object_get(id, DRM_OBJECT_CONNECTOR);
}

drm_connector_t *drm_connector_get_by_index(u32 index) {
  if (index >= DRM_KMS_MAX_CONNECTORS) {
    return NULL;
  }
  return &g_connectors[index];
}

u32 drm_connector_count(void) { return g_connector_count; }

drm_connector_t *drm_connector_get_primary(void) { return &g_connectors[0]; }

int drm_connector_set_connected(drm_connector_t *conn, int connected) {
  if (!conn) {
    return DRM_ERR_INVAL;
  }
  conn->props[DRM_PROP_CONNECTOR_CONNECTED] = connected ? 1 : 0;
  connector_sync_from_props(conn);
  return DRM_OK;
}

drm_id_t drm_connector_init_primary(void) {
  memset(&g_connectors[0], 0, sizeof(g_connectors[0]));
  g_connectors[0].index = 0;
  g_connectors[0].connected = 1;
  g_connectors[0].props[DRM_PROP_CONNECTOR_CONNECTED] = 1;
  g_connectors[0].props[DRM_PROP_CONNECTOR_CRTC_ID] = DRM_ID_NONE;
  drm_id_t id = drm_object_register(DRM_OBJECT_CONNECTOR, &g_connectors[0], 0);
  g_connectors[0].id = id;
  connector_sync_from_props(&g_connectors[0]);
  if (1 > g_connector_count) {
    g_connector_count = 1;
  }
  return id;
}

void drm_connector_reset_all(void) {
  memset(g_connectors, 0, sizeof(g_connectors));
  g_connector_count = 0;
}
