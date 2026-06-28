/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/object.h>
#include <mlibc/mlibc.h>

#define OBJ_TABLE_SIZE 256

typedef struct {
  drm_id_t id;
  drm_object_type_t type;
  void *ptr;
} obj_entry_t;

static obj_entry_t g_obj_table[OBJ_TABLE_SIZE];
static drm_id_t g_obj_next;

drm_id_t drm_object_register(drm_object_type_t type, void *ptr) {
  if (!ptr) {
    return DRM_ID_NONE;
  }
  for (int i = 0; i < OBJ_TABLE_SIZE; i++) {
    u32 idx = (g_obj_next + i) % OBJ_TABLE_SIZE;
    if (g_obj_table[idx].id == DRM_ID_NONE) {
      drm_id_t id = idx + 1;
      g_obj_table[idx].id = id;
      g_obj_table[idx].type = type;
      g_obj_table[idx].ptr = ptr;
      g_obj_next = (idx + 1) % OBJ_TABLE_SIZE;
      return id;
    }
  }
  return DRM_ID_NONE;
}

void *drm_object_get(drm_id_t id, drm_object_type_t type) {
  if (id == DRM_ID_NONE) {
    return NULL;
  }
  u32 idx = id - 1;
  if (idx >= OBJ_TABLE_SIZE) {
    return NULL;
  }
  obj_entry_t *e = &g_obj_table[idx];
  if (e->id != id || e->type != type) {
    return NULL;
  }
  return e->ptr;
}

int drm_object_unregister(drm_id_t id) {
  if (id == DRM_ID_NONE) {
    return DRM_ERR_INVAL;
  }
  u32 idx = id - 1;
  if (idx >= OBJ_TABLE_SIZE) {
    return DRM_ERR_NOENT;
  }
  obj_entry_t *e = &g_obj_table[idx];
  if (e->id != id) {
    return DRM_ERR_NOENT;
  }
  e->id = DRM_ID_NONE;
  e->type = 0;
  e->ptr = NULL;
  return DRM_OK;
}

void drm_object_reset_all(void) {
  memset(g_obj_table, 0, sizeof(g_obj_table));
  g_obj_next = 0;
}
