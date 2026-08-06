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
#include <drm/object.h>
#include <kernel/entity/entity.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define OBJ_TABLE_SIZE 256

typedef struct {
  drm_id_t id;
  drm_object_type_t type;
  void *ptr;
  u32 index;
  u64 entity;
} obj_entry_t;

static obj_entry_t g_obj_table[OBJ_TABLE_SIZE];
static drm_id_t g_obj_next;

drm_id_t drm_object_register(drm_object_type_t type, void *ptr, u32 index) {
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
      g_obj_table[idx].index = index;
      if (entity_is_initialized()) {
        g_obj_table[idx].entity = entity_create(ENTITY_ARCH_DRM, 0,
            0, 0, 0, 0, 0, 1);
        if (g_obj_table[idx].entity != 0) {
          char name[64];

          entity_set_i32(g_obj_table[idx].entity, 0, (s32)type);
          entity_set_i32(g_obj_table[idx].entity, 1, (s32)index);
          snprintf(name, sizeof(name), "/Entity/Drm/%u",
              (unsigned int)id);
          entity_ns_bind(name, g_obj_table[idx].entity);
        }
      }
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

int drm_object_get_index(drm_id_t id, drm_object_type_t type, u32 *out_index) {
  if (id == DRM_ID_NONE || !out_index) {
    return DRM_ERR_INVAL;
  }
  u32 idx = id - 1;
  if (idx >= OBJ_TABLE_SIZE) {
    return DRM_ERR_NOENT;
  }
  obj_entry_t *e = &g_obj_table[idx];
  if (e->id != id || e->type != type) {
    return DRM_ERR_NOENT;
  }
  *out_index = e->index;
  return DRM_OK;
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
  if (e->entity != 0) {
    entity_destroy(e->entity);
    e->entity = 0;
  }
  e->id = DRM_ID_NONE;
  e->type = 0;
  e->ptr = NULL;
  return DRM_OK;
}

void drm_object_reset_all(void) {
  for (int i = 0; i < OBJ_TABLE_SIZE; i++) {
    if (g_obj_table[i].entity != 0) {
      entity_destroy(g_obj_table[i].entity);
      g_obj_table[i].entity = 0;
    }
  }
  memset(g_obj_table, 0, sizeof(g_obj_table));
  g_obj_next = 0;
}
