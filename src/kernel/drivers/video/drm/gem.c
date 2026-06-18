/*
 * Copyright (c) 2026, otsos team
 */

#include <drm/drm.h>
#include <drm/gem.h>
#include <lib/com1.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

#define GEM_MAX_BUFFERS 128

static drm_gem_buffer_t g_gem_table[GEM_MAX_BUFFERS];
static u32 g_gem_next;

static int gem_alloc_slot(void) {
  for (int i = 0; i < GEM_MAX_BUFFERS; i++) {
    u32 idx = (g_gem_next + i) % GEM_MAX_BUFFERS;
    if (g_gem_table[idx].handle == 0) {
      g_gem_next = (idx + 1) % GEM_MAX_BUFFERS;
      return (int)idx;
    }
  }
  return -1;
}

drm_handle_t drm_gem_create(u64 size) {
  if (size == 0) {
    return 0;
  }

  int slot = gem_alloc_slot();
  if (slot < 0) {
    com1_write_string("[GEM] table full\n");
    return 0;
  }

  /* Page-aligned for mmap friendliness. */
  u64 aligned = (size + 4095) & ~4095ULL;
  void *data = kmalloc_aligned(aligned, 4096);
  if (!data) {
    com1_write_string("[GEM] alloc failed\n");
    return 0;
  }
  memset(data, 0, aligned);

  drm_gem_buffer_t *buf = &g_gem_table[slot];
  buf->data = (u8 *)data;
  buf->size = aligned;
  buf->refcount = 1;
  buf->handle = (drm_handle_t)slot + 1;
  return buf->handle;
}

drm_gem_buffer_t *drm_gem_lookup(drm_handle_t handle) {
  if (handle == 0) {
    return NULL;
  }
  u32 idx = handle - 1;
  if (idx >= GEM_MAX_BUFFERS) {
    return NULL;
  }
  drm_gem_buffer_t *buf = &g_gem_table[idx];
  if (buf->handle != handle) {
    return NULL;
  }
  return buf;
}

void *drm_gem_vaddr(drm_handle_t handle) {
  drm_gem_buffer_t *buf = drm_gem_lookup(handle);
  return buf ? (void *)buf->data : NULL;
}

u64 drm_gem_size(drm_handle_t handle) {
  drm_gem_buffer_t *buf = drm_gem_lookup(handle);
  return buf ? buf->size : 0;
}

int drm_gem_close(drm_handle_t handle) {
  drm_gem_buffer_t *buf = drm_gem_lookup(handle);
  if (!buf) {
    return DRM_ERR_NOENT;
  }
  if (buf->refcount > 1) {
    buf->refcount--;
    return DRM_OK;
  }
  if (buf->data) {
    kfree(buf->data);
  }
  buf->data = NULL;
  buf->size = 0;
  buf->refcount = 0;
  buf->handle = 0;
  return DRM_OK;
}
