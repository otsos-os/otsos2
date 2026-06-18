/*
 * Copyright (c) 2026, otsos team
 */

#ifndef DRM_OBJECT_H
#define DRM_OBJECT_H

#include <drm/drm.h>

drm_id_t drm_object_register(drm_object_type_t type, void *ptr);
void *drm_object_get(drm_id_t id, drm_object_type_t type);
int drm_object_unregister(drm_id_t id);

#endif
