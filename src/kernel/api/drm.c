/*
 * Copyright (c) 2026, otsos team
 */

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/drivers/tty.h>
#include <kernel/drivers/video/drm/auth.h>
#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/video/drm/gem.h>
#include <kernel/drivers/video/drm/kms/atomic.h>
#include <kernel/drivers/video/drm/kms/connector.h>
#include <kernel/drivers/video/drm/kms/crtc.h>
#include <kernel/drivers/video/drm/kms/framebuffer.h>
#include <kernel/drivers/video/drm/kms/plane.h>
#include <kernel/drivers/video/drm/rapi/rapi.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int drm_op_info(struct api_drm_info *out) {
  if (!out) return -API_ERR_INVAL;
  memset(out, 0, sizeof(*out));
  out->available = drm_is_ready() ? 1 : 0;
  if (!drm_is_ready()) return 0;
  out->width = drm_crtc_get_width();
  out->height = drm_crtc_get_height();
  out->pitch = drm_crtc_get_pitch();
  out->bpp = drm_crtc_get_bpp();
  out->cols = out->width / 8;
  out->rows = out->height / 16;
  const char *name = drm_driver_get_selected_name();
  if (name) {
    u32 i;
    for (i = 0; i < 31 && name[i]; i++) out->driver_name[i] = name[i];
    out->driver_name[i] = '\0';
  }
  return 0;
}

static int drm_op_gem_create(struct api_drm_gem_create *arg) {
  if (!arg) return -API_ERR_INVAL;
  drm_handle_t h = drm_gem_create(arg->size);
  if (h == 0) return -API_ERR_NOMEM;
  arg->handle = h;
  return 0;
}

static int drm_op_gem_close(u32 handle) {
  int rc = drm_gem_close(handle);
  return (rc == DRM_OK) ? 0 : -API_ERR_INVAL;
}

static int drm_op_gem_map(struct api_drm_gem_map *arg) {
  if (!arg) return -API_ERR_INVAL;
  void *ptr = drm_gem_vaddr(arg->handle);
  if (!ptr) return -API_ERR_NOT_FOUND;
  arg->vaddr = (u64)ptr;
  arg->size = drm_gem_size(arg->handle);
  return 0;
}

static int drm_op_fb_create(struct api_drm_fb_create *arg) {
  if (!arg) return -API_ERR_INVAL;
  drm_id_t id = drm_framebuffer_create(arg->gem_handle, arg->width,
                                       arg->height, arg->pitch, arg->bpp);
  if (id == DRM_ID_NONE) return -API_ERR_NOMEM;
  arg->fb_id = id;
  return 0;
}

static int drm_op_fb_destroy(u32 fb_id) {
  int rc = drm_framebuffer_destroy(fb_id);
  if (rc == DRM_ERR_BUSY) return -API_ERR_BUSY;
  if (rc != DRM_OK) return -API_ERR_INVAL;
  return 0;
}

static int drm_op_get_objects(struct api_drm_objects *out) {
  if (!out) return -API_ERR_INVAL;
  drm_plane_t *p = drm_plane_get_primary();
  drm_crtc_t *c = drm_crtc_get_primary();
  drm_connector_t *conn = drm_connector_get_primary();
  out->primary_plane_id = p ? p->id : 0;
  out->crtc_id = c ? c->id : 0;
  out->connector_id = conn ? conn->id : 0;
  return 0;
}

static int drm_op_atomic_commit(struct api_drm_atomic_commit *arg) {
  if (!arg || !arg->reqs || arg->count == 0) return -API_ERR_INVAL;

  /* Translate userspace array to kernel array. */
  drm_atomic_req_t *reqs =
      (drm_atomic_req_t *)kmem_alloc(sizeof(drm_atomic_req_t) * arg->count);
  if (!reqs) return -API_ERR_NOMEM;
  for (u32 i = 0; i < arg->count; i++) {
    reqs[i].obj_id = arg->reqs[i].obj_id;
    reqs[i].prop_id = arg->reqs[i].prop_id;
    reqs[i].value = arg->reqs[i].value;
  }

  int rc = drm_atomic_commit(reqs, arg->count, arg->flags);
  kmem_free(reqs);

  switch (rc) {
  case DRM_OK:         return 0;
  case DRM_ERR_PERM:   return -API_ERR_PERM;
  case DRM_ERR_NOENT:  return -API_ERR_NOT_FOUND;
  case DRM_ERR_BUSY:   return -API_ERR_BUSY;
  case DRM_ERR_RANGE:  return -API_ERR_INVAL;
  default:             return -API_ERR_INVAL;
  }
}

static int drm_op_rapi_clear(struct api_drm_rapi_rect *a) {
  if (!a) return -API_ERR_INVAL;
  drm_gem_buffer_t *buf = drm_gem_lookup(a->handle);
  if (!buf) return -API_ERR_NOT_FOUND;
  int rc = rapi_clear(buf, a->pitch, a->bpp, a->color);
  return (rc == DRM_OK) ? 0 : -API_ERR_INVAL;
}

static int drm_op_rapi_put_pixel(struct api_drm_rapi_pixel *a) {
  if (!a) return -API_ERR_INVAL;
  drm_gem_buffer_t *buf = drm_gem_lookup(a->handle);
  if (!buf) return -API_ERR_NOT_FOUND;
  int rc = rapi_put_pixel(buf, a->pitch, a->bpp, a->x, a->y, a->color);
  return (rc == DRM_OK) ? 0 : -API_ERR_INVAL;
}

static int drm_op_rapi_fill_rect(struct api_drm_rapi_rect *a) {
  if (!a) return -API_ERR_INVAL;
  drm_gem_buffer_t *buf = drm_gem_lookup(a->handle);
  if (!buf) return -API_ERR_NOT_FOUND;
  rapi_rect_t r = {a->x, a->y, a->width, a->height};
  int rc = rapi_fill_rect(buf, a->pitch, a->bpp, r, a->color);
  return (rc == DRM_OK) ? 0 : -API_ERR_INVAL;
}

static int drm_op_rapi_glyph(struct api_drm_rapi_glyph *a) {
  if (!a) return -API_ERR_INVAL;
  drm_gem_buffer_t *buf = drm_gem_lookup(a->handle);
  if (!buf) return -API_ERR_NOT_FOUND;
  int rc = rapi_glyph(buf, a->pitch, a->bpp, a->x, a->y, a->c, a->fg, a->bg);
  return (rc == DRM_OK) ? 0 : -API_ERR_INVAL;
}

static int drm_op_rapi_scroll(struct api_drm_rapi_scroll *a) {
  if (!a) return -API_ERR_INVAL;
  drm_gem_buffer_t *buf = drm_gem_lookup(a->handle);
  if (!buf) return -API_ERR_NOT_FOUND;
  int rc = rapi_scroll_up(buf, a->pitch, a->bpp, a->lines, a->bg);
  return (rc == DRM_OK) ? 0 : -API_ERR_INVAL;
}

static int drm_op_rapi_blit(struct api_drm_rapi_blit *a) {
  if (!a) return -API_ERR_INVAL;
  drm_gem_buffer_t *src = drm_gem_lookup(a->src_handle);
  drm_gem_buffer_t *dst = drm_gem_lookup(a->dst_handle);
  if (!src || !dst) return -API_ERR_NOT_FOUND;
  rapi_rect_t sr = {a->sx, a->sy, a->sw, a->sh};
  int rc = rapi_blit(src, a->src_pitch, a->bpp, sr, dst, a->dst_pitch, a->dx,
                     a->dy);
  return (rc == DRM_OK) ? 0 : -API_ERR_INVAL;
}

static int
drm_op_driver_list(struct api_drm_driver_list *arg)
{
	u32	count, i, active_idx;

	if (!arg) {
		return (-API_ERR_INVAL);
	}
	if (!is_user_address(arg->entries,
	    arg->max_entries * sizeof(struct api_drm_driver_entry))) {
		return (-API_ERR_BAD_ADDR);
	}

	count = drm_driver_available_count();
	active_idx = (u32)drm_driver_get_selected_index();

	arg->count = 0;
	for (i = 0; i < count && i < arg->max_entries; i++) {
		const drm_driver_t	*drv;
		struct api_drm_driver_entry	*e;

		drv = drm_driver_available_get(i);
		if (!drv) {
			continue;
		}

		e = &arg->entries[i];
		e->id = i;
		e->active = (drv == drm_driver_get_selected()) ? 1 : 0;
		memset(e->name, 0, sizeof(e->name));
		if (drv->name) {
			u32	j;

			for (j = 0; j < 31 && drv->name[j]; j++) {
				e->name[j] = drv->name[j];
			}
			e->name[j] = '\0';
		}
		arg->count++;
	}

	return (0);
}

static int
drm_op_driver_switch(struct api_drm_driver_switch *arg)
{
	const drm_driver_t	*drv;
	int			rc;

	if (!arg) {
		return (-API_ERR_INVAL);
	}

	drv = drm_driver_available_get(arg->id);
	if (!drv) {
		return (-API_ERR_NOT_FOUND);
	}

	if (drv == drm_driver_get_selected()) {
		return (0);
	}

	    arg->id, drv->name ? drv->name : "?";

	rc = drm_reinit(drv, NULL);
	if (rc != 0) {
		return (-API_ERR_NODEV);
	}

	tty_reinit();

	return (0);
}

int api_drm_call(u64 op, void *arg) {
  if (!drm_is_ready() && op != DRM_OP_INFO) {
    return -API_ERR_NODEV;
  }

  process_t *proc = process_current();
  int is_kusr = proc && proc->kusr_auth;

  /*
   * Permission model:
   *   Everyone: INFO, GEM_CREATE/CLOSE, RAPI_*  (render to own buffers)
   *   kusr only: GEM_MAP (raw kernel address — dangerous),
   *              FB_CREATE/DESTROY, GET_OBJECTS,
   *              ATOMIC_COMMIT (page-flip + modeset — touches screen)
   *
   * Regular programs can create GEM buffers and render into them via RAPI
   * or mmap, but cannot display anything without kusr. This prevents
   * unprivileged programs from breaking the screen.
   */

  switch (op) {
  case DRM_OP_INFO:
    return drm_op_info((struct api_drm_info *)arg);

  case DRM_OP_GEM_CREATE:
    return drm_op_gem_create((struct api_drm_gem_create *)arg);
  case DRM_OP_GEM_CLOSE:
    return drm_op_gem_close((u32)(u64)arg);

  case DRM_OP_RAPI_CLEAR:
    return drm_op_rapi_clear((struct api_drm_rapi_rect *)arg);
  case DRM_OP_RAPI_PUT_PIXEL:
    return drm_op_rapi_put_pixel((struct api_drm_rapi_pixel *)arg);
  case DRM_OP_RAPI_FILL_RECT:
    return drm_op_rapi_fill_rect((struct api_drm_rapi_rect *)arg);
  case DRM_OP_RAPI_GLYPH:
    return drm_op_rapi_glyph((struct api_drm_rapi_glyph *)arg);
  case DRM_OP_RAPI_SCROLL:
    return drm_op_rapi_scroll((struct api_drm_rapi_scroll *)arg);
  case DRM_OP_RAPI_BLIT:
    return drm_op_rapi_blit((struct api_drm_rapi_blit *)arg);

  /* --- kusr only beyond this point --- */
  case DRM_OP_GEM_MAP:
    if (!is_kusr) return -API_ERR_PERM;
    return drm_op_gem_map((struct api_drm_gem_map *)arg);

  case DRM_OP_FB_CREATE:
    if (!is_kusr) return -API_ERR_PERM;
    return drm_op_fb_create((struct api_drm_fb_create *)arg);
  case DRM_OP_FB_DESTROY:
    if (!is_kusr) return -API_ERR_PERM;
    return drm_op_fb_destroy((u32)(u64)arg);
  case DRM_OP_GET_OBJECTS:
    if (!is_kusr) return -API_ERR_PERM;
    return drm_op_get_objects((struct api_drm_objects *)arg);

  case DRM_OP_ATOMIC_COMMIT:
    if (!is_kusr) return -API_ERR_PERM;
    return drm_op_atomic_commit((struct api_drm_atomic_commit *)arg);

  /* --- driver management --- */
  case DRM_OP_DRIVER_LIST:
    return drm_op_driver_list((struct api_drm_driver_list *)arg);

  case DRM_OP_DRIVER_SWITCH:
    if (!is_kusr) return -API_ERR_PERM;
    return drm_op_driver_switch((struct api_drm_driver_switch *)arg);

  default:
    return -API_ERR_INVAL;
  }
}
