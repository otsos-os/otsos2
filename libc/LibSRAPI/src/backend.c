/* !DEFINES!

$define %type srapi_image as DRM backed image state
$define %func srapi_image_mark_dirty as procedure with args image, rect
$define %func srapi_backend_present as function with args device, image
$define %func srapi_backend_unbind as function with args device, image

*/

/* !SPACE!

$space %internal rect_include
$space %export srapi_image_mark_dirty, srapi_backend_present
$space %export srapi_backend_unbind

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <native.h>
#include <srapi.h>
#include <stdint.h>
#include <string.h>

#include "srapi_private.h"

static void
rect_include(struct srapi_rect *dst, int *valid, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
	uint32_t	x2, y2, rx2, ry2;

	if (width == 0 || height == 0) {
		return;
	}
	if (!*valid) {
		dst->x = x;
		dst->y = y;
		dst->width = width;
		dst->height = height;
		*valid = 1;
		return;
	}
	x2 = dst->x + dst->width;
	y2 = dst->y + dst->height;
	rx2 = x + width;
	ry2 = y + height;
	if (x < dst->x) {
		dst->x = x;
	}
	if (y < dst->y) {
		dst->y = y;
	}
	if (rx2 > x2) {
		x2 = rx2;
	}
	if (ry2 > y2) {
		y2 = ry2;
	}
	dst->width = x2 - dst->x;
	dst->height = y2 - dst->y;
}

void
srapi_image_mark_dirty(srapi_image_t *image, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
	uint32_t	x2, y2;

	if (!image || width == 0 || height == 0 || x >= image->width ||
	    y >= image->height) {
		return;
	}
	x2 = x + width;
	y2 = y + height;
	if (x2 > image->width || x2 < x) {
		x2 = image->width;
	}
	if (y2 > image->height || y2 < y) {
		y2 = image->height;
	}
	rect_include(&image->dirty, &image->dirty_valid, x, y, x2 - x,
	    y2 - y);
}

int
srapi_backend_present(srapi_device_t *device, srapi_image_t *image)
{
	struct api_drm_atomic_req	reqs[9];
	struct srapi_rect		dirty;
	uint32_t			count;

	if (!device || !image || image->fb == 0) {
		return (SRAPI_ERR_INVALID);
	}
	if (!image->dirty_valid) {
		dirty.x = 0;
		dirty.y = 0;
		dirty.width = image->width;
		dirty.height = image->height;
	} else {
		dirty = image->dirty;
	}

	memset(reqs, 0, sizeof(reqs));
	count = 0;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_FB_ID;
	reqs[count].value = image->fb;
	count++;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_SRC_X;
	reqs[count].value = 0;
	count++;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_SRC_Y;
	reqs[count].value = 0;
	count++;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_SRC_W;
	reqs[count].value = image->width;
	count++;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_SRC_H;
	reqs[count].value = image->height;
	count++;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_DIRTY_X;
	reqs[count].value = dirty.x;
	count++;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_DIRTY_Y;
	reqs[count].value = dirty.y;
	count++;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_DIRTY_W;
	reqs[count].value = dirty.width;
	count++;
	reqs[count].obj_id = device->objects.primary_plane_id;
	reqs[count].prop_id = DRM_PROP_PLANE_DIRTY_H;
	reqs[count].value = dirty.height;
	count++;

	if (drmAtomicCommit(reqs, count, 0) != 0) {
		return (SRAPI_ERR_DRIVER);
	}
	image->dirty_valid = 0;
	return (SRAPI_OK);
}

int
srapi_backend_unbind(srapi_device_t *device, srapi_image_t *image)
{
	struct api_drm_atomic_req	req;

	if (!device || !image || image->fb == 0) {
		return (SRAPI_ERR_INVALID);
	}
	memset(&req, 0, sizeof(req));
	req.obj_id = device->objects.primary_plane_id;
	req.prop_id = DRM_PROP_PLANE_FB_ID;
	req.value = 0;
	if (drmAtomicCommit(&req, 1, 0) != 0) {
		return (SRAPI_ERR_DRIVER);
	}
	return (SRAPI_OK);
}
