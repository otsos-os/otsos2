/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/object.h>
#include <drm/kms/connector.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

static drm_connector_t g_primary_connector;

drm_id_t drm_connector_create(void) {
  drm_connector_t *c =
      (drm_connector_t *)kcalloc(sizeof(drm_connector_t), 1);
  if (!c) {
    return DRM_ID_NONE;
  }
  drm_id_t id = drm_object_register(DRM_OBJECT_CONNECTOR, c);
  if (id == DRM_ID_NONE) {
    kfree(c);
    return DRM_ID_NONE;
  }
  c->id = id;
  return id;
}

drm_connector_t *drm_connector_get(drm_id_t id) {
  return (drm_connector_t *)drm_object_get(id, DRM_OBJECT_CONNECTOR);
}

drm_connector_t *drm_connector_get_primary(void) { return &g_primary_connector; }

int drm_connector_set_connected(drm_connector_t *conn, int connected) {
  if (!conn) {
    return DRM_ERR_INVAL;
  }
  conn->connected = connected ? 1 : 0;
  return DRM_OK;
}

drm_id_t drm_connector_init_primary(void) {
  memset(&g_primary_connector, 0, sizeof(g_primary_connector));
  g_primary_connector.connected = 1;
  drm_id_t id = drm_object_register(DRM_OBJECT_CONNECTOR, &g_primary_connector);
  g_primary_connector.id = id;
  return id;
}
