/*
 * Copyright (c) 2026, otsos team
 */

#include <kernel/drivers/console/kms_console.h>
#include <drm/auth.h>
#include <drm/gem.h>
#include <drm/kms/atomic.h>
#include <drm/kms/crtc.h>
#include <drm/kms/framebuffer.h>
#include <drm/kms/plane.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define FONT_W 8
#define FONT_H 16

static kms_console_t	g_kernel_con;
static int		g_kernel_con_ready;

kms_console_t *
kms_kernel_console(void)
{
	if (g_kernel_con_ready) {
		return (&g_kernel_con);
	}
	if (!drm_is_ready()) {
		return (NULL);
	}
	if (kms_console_init(&g_kernel_con) != DRM_OK) {
		return (NULL);
	}
	g_kernel_con_ready = 1;
	return (&g_kernel_con);
}

void
kms_kernel_console_reset(void)
{
	if (g_kernel_con_ready) {
		kms_console_shutdown(&g_kernel_con);
	}
	g_kernel_con_ready = 0;
	memset(&g_kernel_con, 0, sizeof(g_kernel_con));
}

void
kms_console_surface(const kms_console_t *con, rapi_surface_t *out)
{
	if (!con || !out) {
		return;
	}
	out->gem = con->gem;
	out->pitch = con->pitch;
	out->bpp = con->bpp;
	out->width = con->width;
	out->height = con->height;
	out->pan_y = con->pan_y;
	out->buf_h = con->buf_h;
}

void
kms_console_mark_dirty(kms_console_t *con, u32 x, u32 y, u32 w, u32 h)
{
	if (!con) {
		return;
	}
	if (!con->dirty) {
		con->dirty_x1 = x;
		con->dirty_y1 = y;
		con->dirty_x2 = x + w;
		con->dirty_y2 = y + h;
		con->dirty = 1;
	} else {
		if (x < con->dirty_x1) {
			con->dirty_x1 = x;
		}
		if (y < con->dirty_y1) {
			con->dirty_y1 = y;
		}
		if (x + w > con->dirty_x2) {
			con->dirty_x2 = x + w;
		}
		if (y + h > con->dirty_y2) {
			con->dirty_y2 = y + h;
		}
	}
	if (con->dirty_x2 > con->width) {
		con->dirty_x2 = con->width;
	}
	if (con->dirty_y2 > con->height) {
		con->dirty_y2 = con->height;
	}
}

int
kms_console_init(kms_console_t *con)
{
	u32	w, h, buf_h;
	u64	size;
	u8	bpp;
	u32	pitch;
	int	rc;
	drm_plane_t	*plane;
	drm_atomic_req_t	req;

	if (!con || !drm_is_ready()) {
		return (DRM_ERR_NODEV);
	}
	if (g_kernel_con_ready && con == &g_kernel_con) {
		return (DRM_OK);
	}

	w = drm_crtc_get_width();
	h = drm_crtc_get_height();
	pitch = drm_crtc_get_pitch();
	bpp = drm_crtc_get_bpp();
	if (w == 0 || h == 0 || pitch == 0 || bpp == 0) {
		return (DRM_ERR_NODEV);
	}

	buf_h = h * 2;
	size = (u64)pitch * buf_h;
	con->gem = drm_gem_create(size);
	if (con->gem == 0) {
		return (DRM_ERR_NOMEM);
	}

	con->fb = drm_framebuffer_create(con->gem, w, h, pitch, bpp);
	if (con->fb == DRM_ID_NONE) {
		drm_gem_close(con->gem);
		con->gem = 0;
		return (DRM_ERR_NOMEM);
	}

	plane = drm_plane_get_primary();
	req.obj_id = plane->id;
	req.prop_id = DRM_PROP_PLANE_FB_ID;
	req.value = con->fb;
	rc = drm_atomic_commit(&req, 1, DRM_ATOMIC_ALLOW_MODESET);
	if (rc != DRM_OK) {
		drivers_log("[KMSCON] initial commit failed: %d\n", rc);
		drm_framebuffer_destroy(con->fb);
		drm_gem_close(con->gem);
		con->fb = DRM_ID_NONE;
		con->gem = 0;
		return (rc);
	}

	con->width = w;
	con->height = h;
	con->pitch = pitch;
	con->bpp = bpp;
	con->buf_h = buf_h;
	con->pan_y = 0;
	con->cols = w / FONT_W;
	con->rows = h / FONT_H;
	con->dirty = 0;
	con->ready = 1;

	kms_console_mark_dirty(con, 0, 0, w, h);
	return (DRM_OK);
}

void
kms_console_shutdown(kms_console_t *con)
{
	drm_atomic_req_t	req;

	if (!con || !con->ready) {
		return;
	}
	req.obj_id = drm_plane_get_primary()->id;
	req.prop_id = DRM_PROP_PLANE_FB_ID;
	req.value = DRM_ID_NONE;
	drm_atomic_commit(&req, 1, 0);
	if (con->fb != DRM_ID_NONE) {
		drm_framebuffer_destroy(con->fb);
	}
	if (con->gem) {
		drm_gem_close(con->gem);
	}
	con->ready = 0;
	if (con == &g_kernel_con) {
		g_kernel_con_ready = 0;
	}
}

int
kms_console_flush(kms_console_t *con)
{
	drm_atomic_req_t	reqs[6];
	drm_plane_t		*plane;
	u32			dx, dy, dw, dh;
	int			rc;

	if (!con || !con->ready) {
		return (DRM_ERR_INVAL);
	}
	if (!con->dirty) {
		return (DRM_OK);
	}

	plane = drm_plane_get_primary();
	if (!plane || plane->id == DRM_ID_NONE) {
		return (DRM_ERR_NODEV);
	}

	dx = con->dirty_x1;
	dy = con->dirty_y1;
	dw = con->dirty_x2 - con->dirty_x1;
	dh = con->dirty_y2 - con->dirty_y1;

	reqs[0].obj_id = plane->id;
	reqs[0].prop_id = DRM_PROP_PLANE_FB_ID;
	reqs[0].value = con->fb;
	reqs[1].obj_id = plane->id;
	reqs[1].prop_id = DRM_PROP_PLANE_SRC_Y;
	reqs[1].value = con->pan_y;
	reqs[2].obj_id = plane->id;
	reqs[2].prop_id = DRM_PROP_PLANE_DIRTY_X;
	reqs[2].value = dx;
	reqs[3].obj_id = plane->id;
	reqs[3].prop_id = DRM_PROP_PLANE_DIRTY_Y;
	reqs[3].value = dy;
	reqs[4].obj_id = plane->id;
	reqs[4].prop_id = DRM_PROP_PLANE_DIRTY_W;
	reqs[4].value = dw;
	reqs[5].obj_id = plane->id;
	reqs[5].prop_id = DRM_PROP_PLANE_DIRTY_H;
	reqs[5].value = dh;

	rc = drm_atomic_commit(reqs, 6, 0);
	if (rc != DRM_OK) {
		drivers_log("[KMSCON] flush commit failed: %d\n", rc);
		return (rc);
	}

	con->dirty = 0;
	return (DRM_OK);
}
