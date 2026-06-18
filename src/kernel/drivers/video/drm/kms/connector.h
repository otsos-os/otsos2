/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_KMS_CONNECTOR_H
#define DRM_KMS_CONNECTOR_H

#include <drm/drm.h>

drm_id_t drm_connector_create(void);
drm_connector_t *drm_connector_get(drm_id_t id);
drm_connector_t *drm_connector_get_primary(void);
int drm_connector_set_connected(drm_connector_t *conn, int connected);

/* Internal: register the static primary connector. */
drm_id_t drm_connector_init_primary(void);

#endif
