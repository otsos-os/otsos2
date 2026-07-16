/* !DEFINES!

$define %type api_drm_info as struct with active display mode data
$define %type api_drm_atomic_req as struct with one KMS property update
$define %func drmCall as function with args uint64_t, void *
$define %func drmGemCreate as function with args size_t, uint32_t *

*/

/* !SPACE!

$space %export drmCall, drmInfo, drmGemCreate, drmGemClose, drmGemMapInfo
$space %export drmGemMmap, drmFbCreate, drmFbDestroy, drmGetObjects
$space %export drmAtomicCommit, drmRapiClear, drmRapiPutPixel
$space %export drmRapiFillRect, drmRapiGlyph, drmRapiScroll, drmRapiBlit
$space %export drmDriverList, drmDriverSwitch

*/

#include <errno.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

int
drmCall(uint64_t op, void *arg)
{
	return (__sysret_int(__syscall2(CALL_DRM_CALL, (long)op,
	    (long)arg)));
}

int
drmInfo(struct api_drm_info *info)
{
	return (drmCall(DRM_OP_INFO, info));
}

int
drmGemCreate(size_t size, uint32_t *handle)
{
	struct api_drm_gem_create	args;
	int				ret;

	if (!handle) {
		errno = EINVAL;
		return (-1);
	}
	memset(&args, 0, sizeof(args));
	args.size = size;
	ret = drmCall(DRM_OP_GEM_CREATE, &args);
	if (ret < 0) {
		return (-1);
	}
	*handle = args.handle;
	return (0);
}

int
drmGemClose(uint32_t handle)
{
	return (drmCall(DRM_OP_GEM_CLOSE, (void *)(uintptr_t)handle));
}

int
drmGemMapInfo(uint32_t handle, struct api_drm_gem_map *info)
{
	if (!info) {
		errno = EINVAL;
		return (-1);
	}
	memset(info, 0, sizeof(*info));
	info->handle = handle;
	return (drmCall(DRM_OP_GEM_MAP, info));
}

void *
drmGemMmap(uint32_t handle, size_t size, uint32_t prot)
{
	struct mem_map_args	args;

	memset(&args, 0, sizeof(args));
	args.length = size;
	args.prot = prot;
	args.flags = API_MAP_SHARED | API_MAP_GEM;
	args.fd = (int)handle;
	return (memMap(&args));
}

int
drmFbCreate(uint32_t gem_handle, uint32_t width, uint32_t height,
    uint32_t pitch, uint8_t bpp, uint32_t *fb_id)
{
	struct api_drm_fb_create	args;
	int				ret;

	if (!fb_id) {
		errno = EINVAL;
		return (-1);
	}
	memset(&args, 0, sizeof(args));
	args.gem_handle = gem_handle;
	args.width = width;
	args.height = height;
	args.pitch = pitch;
	args.bpp = bpp;
	ret = drmCall(DRM_OP_FB_CREATE, &args);
	if (ret < 0) {
		return (-1);
	}
	*fb_id = args.fb_id;
	return (0);
}

int
drmFbDestroy(uint32_t fb_id)
{
	return (drmCall(DRM_OP_FB_DESTROY, (void *)(uintptr_t)fb_id));
}

int
drmGetObjects(struct api_drm_objects *objects)
{
	return (drmCall(DRM_OP_GET_OBJECTS, objects));
}

int
drmAtomicCommit(struct api_drm_atomic_req *reqs, uint32_t count,
    uint32_t flags)
{
	struct api_drm_atomic_commit	args;

	memset(&args, 0, sizeof(args));
	args.reqs = reqs;
	args.count = count;
	args.flags = flags;
	return (drmCall(DRM_OP_ATOMIC_COMMIT, &args));
}

int
drmRapiClear(uint32_t handle, uint32_t pitch, uint8_t bpp, uint32_t color)
{
	struct api_drm_rapi_rect	args;

	memset(&args, 0, sizeof(args));
	args.handle = handle;
	args.pitch = pitch;
	args.bpp = bpp;
	args.color = color;
	return (drmCall(DRM_OP_RAPI_CLEAR, &args));
}

int
drmRapiPutPixel(uint32_t handle, uint32_t pitch, uint8_t bpp,
    uint32_t x, uint32_t y, uint32_t color)
{
	struct api_drm_rapi_pixel	args;

	memset(&args, 0, sizeof(args));
	args.handle = handle;
	args.pitch = pitch;
	args.bpp = bpp;
	args.x = x;
	args.y = y;
	args.color = color;
	return (drmCall(DRM_OP_RAPI_PUT_PIXEL, &args));
}

int
drmRapiFillRect(uint32_t handle, uint32_t pitch, uint8_t bpp,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
	struct api_drm_rapi_rect	args;

	memset(&args, 0, sizeof(args));
	args.handle = handle;
	args.pitch = pitch;
	args.bpp = bpp;
	args.x = x;
	args.y = y;
	args.width = width;
	args.height = height;
	args.color = color;
	return (drmCall(DRM_OP_RAPI_FILL_RECT, &args));
}

int
drmRapiGlyph(uint32_t handle, uint32_t pitch, uint8_t bpp,
    uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg)
{
	struct api_drm_rapi_glyph	args;

	memset(&args, 0, sizeof(args));
	args.handle = handle;
	args.pitch = pitch;
	args.bpp = bpp;
	args.x = x;
	args.y = y;
	args.c = c;
	args.fg = fg;
	args.bg = bg;
	return (drmCall(DRM_OP_RAPI_GLYPH, &args));
}

int
drmRapiScroll(uint32_t handle, uint32_t pitch, uint8_t bpp,
    uint32_t lines, uint32_t bg)
{
	struct api_drm_rapi_scroll	args;

	memset(&args, 0, sizeof(args));
	args.handle = handle;
	args.pitch = pitch;
	args.bpp = bpp;
	args.lines = lines;
	args.bg = bg;
	return (drmCall(DRM_OP_RAPI_SCROLL, &args));
}

int
drmRapiBlit(struct api_drm_rapi_blit *blit)
{
	return (drmCall(DRM_OP_RAPI_BLIT, blit));
}

int
drmDriverList(struct api_drm_driver_entry *entries,
    uint32_t max_entries, uint32_t *count)
{
	struct api_drm_driver_list	args;
	int				ret;

	memset(&args, 0, sizeof(args));
	args.entries = entries;
	args.max_entries = max_entries;
	ret = drmCall(DRM_OP_DRIVER_LIST, &args);
	if (ret < 0) {
		return (-1);
	}
	if (count) {
		*count = args.count;
	}
	return (0);
}

int
drmDriverSwitch(uint32_t id)
{
	struct api_drm_driver_switch	args;

	args.id = id;
	return (drmCall(DRM_OP_DRIVER_SWITCH, &args));
}
