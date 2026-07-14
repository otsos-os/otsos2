/*
 * Copyright (c) 2026, otsos team
 *
 * [.BSD-2-clause license text...]
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type virtio_hw_t as struct with resolved transport state
$define %type virtio_vq_t as struct with virtqueue runtime state
$define %type virtio_gpu_state_t as struct with GPU driver state
$define %type virtio_gpu_mem_entry_t as packed struct with memory entry
$define %type virtio_gpu_resp_display_info_t as packed struct with display info
$define %type drm_driver_t as struct with DRM driver vtable
$define %type drm_framebuffer_t as struct with framebuffer info
$define %type pci_device_t as struct with PCI device info
$define %type pci_match_t as struct with PCI match criteria
$define %type pci_driver_t as struct with PCI driver

$define %func virtio_gpu_setup_queues as function with args virtio_gpu_state_t *
$define %func virtio_gpu_query_display as function with args virtio_gpu_state_t *
$define %func virtio_gpu_pci_probe as function with args pci_device_t *, const pci_match_t *
$define %func drm_virtio_gpu_pci_register as function with args void
$define %func drm_virtio_gpu_is_ready as function with args void
$define %func attach_backing_store as function with args virtio_gpu_state_t *
$define %func drm_virtio_gpu_display_init as function with args void
$define %func drm_virtio_gpu_display_shutdown as procedure with args void
$define %func vgpu_drm_probe as function with args const void *
$define %func vgpu_drm_init as function with args const void *
$define %func vgpu_drm_atomic_commit as function with args const drm_kms_state_t *
$define %func vgpu_drm_shutdown as procedure with args void
$define %func drm_virtio_gpu_driver_get as function with args void

*/

/* !SPACE!

$space %internal virtio_gpu_setup_queues, virtio_gpu_query_display
$space %internal virtio_gpu_pci_probe, attach_backing_store
$space %internal vgpu_drm_probe, vgpu_drm_init, vgpu_drm_atomic_commit
$space %internal vgpu_drm_shutdown
$space %export drm_virtio_gpu_pci_register, drm_virtio_gpu_is_ready
$space %export drm_virtio_gpu_display_init
$space %export drm_virtio_gpu_display_shutdown
$space %export drm_virtio_gpu_driver_get

*/

#include <kernel/drivers/video/card/virtio-gpu/virtio-gpu.h>
#include <kernel/drivers/virtio/virtio_pci.h>
#include <kernel/drivers/virtio/virtio_queue.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.h>
#include <drm/drm.h>
#include <drm/kms/crtc.h>
#include <drm/kms/framebuffer.h>
#include <drm/kms/plane.h>
#include <drm/rapi/rapi.h>
#include <kernel/pci/pci.h>
#include <kernel/pci/utils/bar.h>
#include <kernel/mm/vm/pmap.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	VIRTIO_GPU_QUEUE_SIZE		64
#define	VIRTIO_GPU_RESOURCE_ID		1
#define	VIRTIO_GPU_SCANOUT_ID		0
#define	VIRTIO_GPU_FALLBACK_W		1024
#define	VIRTIO_GPU_FALLBACK_H		768
#define	VIRTIO_GPU_BPP			32

typedef struct {
	virtio_hw_t	hw;
	virtio_vq_t	vqs[VIRTIO_GPU_NUM_VQS];
	u32		width;
	u32		height;
	u32		pitch;
	u8		bpp;
	u8		*backing;
	u64		backing_size;
	u32		backing_resource_id;
	int		hw_ready;
	int		display_ready;
} virtio_gpu_state_t;

static virtio_gpu_state_t	g_state;
static int			g_cursor_valid;
static u32			g_cursor_x, g_cursor_y;
static u32			g_cursor_w, g_cursor_h;
static drm_id_t		g_primary_fb_id;

static int
virtio_gpu_setup_queues(virtio_gpu_state_t *st)
{
	virtio_hw_t	*hw;
	int		i;
	u16		qsize;

	hw = &st->hw;

	for (i = 0; i < VIRTIO_GPU_NUM_VQS; i++) {
		virtio_hw_select_queue(hw, (u16)i);
		qsize = virtio_hw_get_queue_size(hw);
		if (qsize == 0) {
			qsize = VIRTIO_GPU_QUEUE_SIZE;
		}
		if (qsize > VIRTIO_GPU_QUEUE_SIZE) {
			qsize = VIRTIO_GPU_QUEUE_SIZE;
		}

		if (virtio_vq_create(&st->vqs[i], qsize) != 0) {
			drivers_log("[VIRTIO_GPU] vq %d create "
			    "failed\n", i);
			return (-1);
		}

		virtio_vq_bind(&st->vqs[i], hw, (u16)i);

		virtio_hw_set_queue_size(hw, qsize);
		virtio_hw_set_queue_desc(hw,
		    st->vqs[i].phys_desc);
		virtio_hw_set_queue_driver(hw,
		    st->vqs[i].phys_avail);
		virtio_hw_set_queue_device(hw,
		    st->vqs[i].phys_used);
		virtio_hw_enable_queue(hw);

		drivers_log("[VIRTIO_GPU] vq %d: size=%u\n",
		    i, qsize);
	}
	return (0);
}

static int
virtio_gpu_query_display(virtio_gpu_state_t *st)
{
	u32				crtc_w, crtc_h;
	virtio_gpu_resp_display_info_t	info;
	int				rc, i;

	crtc_w = drm_crtc_get_width();
	crtc_h = drm_crtc_get_height();

	if (crtc_w > 0 && crtc_h > 0) {
		st->width = crtc_w;
		st->height = crtc_h;
		drivers_log("[VIRTIO_GPU] using KMS mode: "
		    "%ux%u\n", st->width, st->height);
	} else {
		rc = virtio_gpu_cmd_get_display_info(
		    &st->hw,
		    &st->vqs[VIRTIO_GPU_CONTROLQ], &info);
		if (rc != 0) {
			drivers_log(
			    "[VIRTIO_GPU] GET_DISPLAY_INFO "
			    "failed\n");
			st->width = VIRTIO_GPU_FALLBACK_W;
			st->height = VIRTIO_GPU_FALLBACK_H;
			return (0);
		}

		for (i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
			if (info.pmodes[i].enabled) {
				st->width =
				    info.pmodes[i].r.width;
				st->height =
				    info.pmodes[i].r.height;
				drivers_log("[VIRTIO_GPU] device "
				    "display %d: %ux%u\n", i,
				    st->width, st->height);
				return (0);
			}
		}

		drivers_log("[VIRTIO_GPU] scanout doesnt "
		    "enabled\n");
		st->width = VIRTIO_GPU_FALLBACK_W;
		st->height = VIRTIO_GPU_FALLBACK_H;
	}
	return (0);
}

static int
virtio_gpu_pci_probe(pci_device_t *dev, const pci_match_t *match)
{
	(void)match;
	if (!dev) {
		return (-1);
	}

	drivers_log("[VIRTIO_GPU] probing PCI\n");
	memset(&g_state, 0, sizeof(g_state));
	g_state.backing_resource_id = VIRTIO_GPU_RESOURCE_ID;
	if (virtio_pci_init(&g_state.hw, dev, 0) != 0) {
		drivers_log("[VIRTIO_GPU] transport init "
		    "failed\n");
		return (-1);
	}
	if (virtio_gpu_setup_queues(&g_state) != 0) {
		drivers_log("[VIRTIO_GPU] queue setup "
		    "failed\n");
		return (-1);
	}

	virtio_hw_set_status(&g_state.hw,
	    VIRTIO_STATUS_ACKNOWLEDGE |
	    VIRTIO_STATUS_DRIVER |
	    VIRTIO_STATUS_FEATURES_OK |
	    VIRTIO_STATUS_DRIVER_OK);

	if (virtio_gpu_query_display(&g_state) != 0) {
		drivers_log("[VIRTIO_GPU] display query "
		    "failed\n");
		return (-1);
	}

	g_state.pitch = g_state.width * (VIRTIO_GPU_BPP / 8);
	g_state.bpp = VIRTIO_GPU_BPP;
	g_state.hw_ready = 1;

	drivers_log("[VIRTIO_GPU] hardware ready: %ux%u "
	    "x%u bpp\n",
	    g_state.width, g_state.height, g_state.bpp);
	return (0);
}

static pci_match_t virtio_gpu_matches[] = {
	{
		.vendor_id	= VIRTIO_PCI_VENDOR_ID,
		.device_id	= VIRTIO_GPU_DEVICE_ID,
		.class_code	= PCI_ANY_CLASS,
		.subclass	= PCI_ANY_SUBCLASS,
		.prog_if	= PCI_ANY_PROGIF,
	},
};

static pci_driver_t virtio_gpu_pci_driver = {
	.name		= "virtio-gpu",
	.matches	= virtio_gpu_matches,
	.match_count	= 1,
	.probe		= virtio_gpu_pci_probe,
	.remove		= NULL,
};

int
drm_virtio_gpu_pci_register(void)
{
	return (pci_register_driver(&virtio_gpu_pci_driver));
}

int
drm_virtio_gpu_is_ready(void)
{
	return (g_state.hw_ready);
}

static int
attach_backing_store(virtio_gpu_state_t *st)
{
	u64			fb_size, aligned, backing_phys;
	virtio_gpu_mem_entry_t	entry;
	int			rc;

	fb_size = (u64)st->pitch * st->height;
	aligned = (fb_size + PAGE_SIZE - 1) &
	    ~((u64)PAGE_SIZE - 1);

	st->backing = (u8 *)kmem_alloc_aligned(aligned,
	    PAGE_SIZE);
	if (!st->backing) {
		drivers_log("[VIRTIO_GPU] backing alloc "
		    "failed\n");
		return (-1);
	}
	memset(st->backing, 0, aligned);
	st->backing_size = aligned;

	backing_phys = virtio_virt_to_phys(st->backing);
	drivers_log("[VIRTIO_GPU] backing: virt=%p "
	    "phys=%p size=%u\n",
	    st->backing, (void *)backing_phys, (u32)aligned);

	entry.addr = backing_phys;
	entry.length = (u32)aligned;
	entry.padding = 0;

	rc = virtio_gpu_cmd_attach_backing(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id, &entry, 1);
	if (rc != 0) {
		drivers_log("[VIRTIO_GPU] attach backing "
		    "failed\n");
		kmem_free(st->backing);
		st->backing = NULL;
		return (-1);
	}

	drivers_log("[VIRTIO_GPU] backing attached\n");
	return (0);
}

int
drm_virtio_gpu_display_init(void)
{
	virtio_gpu_state_t	*st;

	st = &g_state;

	if (!st->hw_ready) {
		drivers_log("[VIRTIO_GPU] display_init: "
		    "hw not ready\n");
		return (-1);
	}
	if (st->display_ready) {
		return (0);
	}

	drivers_log("[VIRTIO_GPU] display_init: "
	    "creating 2D resource\n");
	if (virtio_gpu_cmd_resource_create_2d(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM,
	    st->width, st->height) != 0) {
		drivers_log("[VIRTIO_GPU] resource create "
		    "failed\n");
		return (-1);
	}

	drivers_log("[VIRTIO_GPU] display_init: "
	    "attaching backing\n");
	if (attach_backing_store(st) != 0) {
		return (-1);
	}

	drivers_log("[VIRTIO_GPU] display_init: "
	    "setting scanout\n");
	if (virtio_gpu_cmd_set_scanout(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    VIRTIO_GPU_SCANOUT_ID, st->backing_resource_id,
	    0, 0, st->width, st->height) != 0) {
		drivers_log("[VIRTIO_GPU] set scanout "
		    "failed\n");
		return (-1);
	}

	drm_crtc_set_mode_geometry(st->width, st->height,
	    st->pitch, st->bpp, 0);

	drivers_log("[VIRTIO_GPU] display_init: "
	    "initial transfer+flush\n");
	if (virtio_gpu_cmd_transfer_to_host_2d(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    0, 0, st->width, st->height, 0) != 0) {
		drivers_log("[VIRTIO_GPU] initial "
		    "transfer failed\n");
		return (-1);
	}
	if (virtio_gpu_cmd_resource_flush(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    0, 0, st->width, st->height) != 0) {
		drivers_log("[VIRTIO_GPU] initial flush "
		    "failed\n");
		return (-1);
	}

	st->display_ready = 1;
	drivers_log("[VIRTIO_GPU] display ready\n");
	return (0);
}

void
drm_virtio_gpu_display_shutdown(void)
{
	virtio_gpu_state_t	*st;

	st = &g_state;
	if (!st->display_ready) {
		return;
	}

	virtio_gpu_cmd_set_scanout(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    VIRTIO_GPU_SCANOUT_ID, 0, 0, 0, 0, 0);

	virtio_gpu_cmd_detach_backing(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id);
	virtio_gpu_cmd_resource_unref(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id);

	if (st->backing) {
		kmem_free(st->backing);
		st->backing = NULL;
	}

	st->display_ready = 0;
}

static int
vgpu_drm_probe(const void *boot_info)
{
	(void)boot_info;
	return (g_state.hw_ready ? 0 : -1);
}

static int
vgpu_drm_init(const void *boot_info)
{
	(void)boot_info;
	g_cursor_valid = 0;
	g_primary_fb_id = DRM_ID_NONE;
	return (drm_virtio_gpu_display_init());
}

static int
vgpu_drm_blit_full(const drm_framebuffer_t *src, u32 src_y)
{
	virtio_gpu_state_t	*st;
	u32			line_bytes, copy, y;

	st = &g_state;
	line_bytes = src->width * (u32)(src->bpp / 8);
	copy = line_bytes < src->pitch ? line_bytes : src->pitch;
	if (copy > st->pitch) {
		copy = st->pitch;
	}
	for (y = 0; y < src->height; y++) {
		memcpy(st->backing + (u64)y * st->pitch,
		    src->gem->data + (u64)(y + src_y) * src->pitch,
		    copy);
	}
	if (virtio_gpu_cmd_transfer_to_host_2d(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    0, 0, st->width, st->height, 0) != 0) {
		return (-1);
	}
	if (virtio_gpu_cmd_resource_flush(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    0, 0, st->width, st->height) != 0) {
		return (-1);
	}
	return (0);
}

static int
vgpu_drm_blit_rect(const drm_framebuffer_t *src, u32 src_y, u32 x, u32 y,
    u32 w, u32 h)
{
	virtio_gpu_state_t	*st;
	u32			x2, y2, rw, rh, bpp_bytes;
	u32			src_line_bytes, ry;
	u64			offset;

	st = &g_state;
	if (x >= st->width || y >= st->height || w == 0 || h == 0) {
		return (0);
	}

	x2 = x + w;
	y2 = y + h;
	if (x2 > st->width) {
		x2 = st->width;
	}
	if (y2 > st->height) {
		y2 = st->height;
	}
	rw = x2 - x;
	rh = y2 - y;

	bpp_bytes = (u32)(src->bpp / 8);
	src_line_bytes = rw * bpp_bytes;

	for (ry = 0; ry < rh; ry++) {
		memcpy(st->backing +
		    (u64)(y + ry) * st->pitch +
		    (u64)x * bpp_bytes,
		    src->gem->data +
		    (u64)(y + ry + src_y) * src->pitch +
		    (u64)x * bpp_bytes,
		    src_line_bytes);
	}

	offset = (u64)y * st->pitch + (u64)x * bpp_bytes;
	if (virtio_gpu_cmd_transfer_to_host_2d(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    x, y, rw, rh, offset) != 0) {
		return (-1);
	}
	if (virtio_gpu_cmd_resource_flush(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    x, y, rw, rh) != 0) {
		return (-1);
	}
	return (0);
}

static void
vgpu_rect_include(u32 *x, u32 *y, u32 *w, u32 *h, int *valid,
    u32 rx, u32 ry, u32 rw, u32 rh)
{
	u32	x2, y2, rx2, ry2;

	if (rw == 0 || rh == 0) {
		return;
	}
	if (!*valid) {
		*x = rx;
		*y = ry;
		*w = rw;
		*h = rh;
		*valid = 1;
		return;
	}
	x2 = *x + *w;
	y2 = *y + *h;
	rx2 = rx + rw;
	ry2 = ry + rh;
	if (rx < *x) {
		*x = rx;
	}
	if (ry < *y) {
		*y = ry;
	}
	if (rx2 > x2) {
		x2 = rx2;
	}
	if (ry2 > y2) {
		y2 = ry2;
	}
	*w = x2 - *x;
	*h = y2 - *y;
}

static int
vgpu_drm_flush_rect(u32 x, u32 y, u32 w, u32 h)
{
	virtio_gpu_state_t	*st;
	u32			x2, y2, rw, rh, bpp_bytes;
	u64			offset;

	st = &g_state;
	if (x >= st->width || y >= st->height || w == 0 || h == 0) {
		return (0);
	}
	x2 = x + w;
	y2 = y + h;
	if (x2 > st->width) {
		x2 = st->width;
	}
	if (y2 > st->height) {
		y2 = st->height;
	}
	rw = x2 - x;
	rh = y2 - y;
	bpp_bytes = (u32)(st->bpp / 8);
	offset = (u64)y * st->pitch + (u64)x * bpp_bytes;
	if (virtio_gpu_cmd_transfer_to_host_2d(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    x, y, rw, rh, offset) != 0) {
		return (-1);
	}
	if (virtio_gpu_cmd_resource_flush(&st->hw,
	    &st->vqs[VIRTIO_GPU_CONTROLQ],
	    st->backing_resource_id,
	    x, y, rw, rh) != 0) {
		return (-1);
	}
	return (0);
}

static int
vgpu_drm_atomic_commit(const drm_kms_state_t *state)
{
	drm_framebuffer_t	*src;
	drm_framebuffer_t	*cursor;
	drm_id_t		fb_id;
	drm_id_t		cursor_id;
	rapi_rect_t		cursor_dirty;
	u32			dx, dy, dw, dh, cx, cy, cw, ch;
	u32			src_y;
	int			dirty_valid, full_update;

	if (!state) {
		return (-1);
	}
	if (!g_state.display_ready) {
		return (-1);
	}
	fb_id = (drm_id_t)state->plane_props[0][DRM_PROP_PLANE_FB_ID];
	if (fb_id == DRM_ID_NONE) {
		return (0);
	}
	src = drm_framebuffer_get(fb_id);
	if (!src || !src->gem || !src->gem->data) {
		return (-1);
	}
	if (src->width != g_state.width || src->height != g_state.height ||
	    src->bpp != g_state.bpp) {
		return (-1);
	}
	src_y = (u32)state->plane_props[0][DRM_PROP_PLANE_SRC_Y];

	dx = (u32)state->plane_props[0][DRM_PROP_PLANE_DIRTY_X];
	dy = (u32)state->plane_props[0][DRM_PROP_PLANE_DIRTY_Y];
	dw = (u32)state->plane_props[0][DRM_PROP_PLANE_DIRTY_W];
	dh = (u32)state->plane_props[0][DRM_PROP_PLANE_DIRTY_H];

	cursor = NULL;
	cursor_id = DRM_ID_NONE;
	cx = cy = cw = ch = 0;
	if (state->plane_count > 1) {
		cursor_id = (drm_id_t)
		    state->plane_props[1][DRM_PROP_PLANE_FB_ID];
		if (cursor_id != DRM_ID_NONE) {
			cursor = drm_framebuffer_get(cursor_id);
			cx = (u32)state->plane_props[1]
			    [DRM_PROP_PLANE_CRTC_X];
			cy = (u32)state->plane_props[1]
			    [DRM_PROP_PLANE_CRTC_Y];
			if (cursor) {
				cw = cursor->width;
				ch = cursor->height;
			}
		}
	}

	dirty_valid = 0;
	full_update = (fb_id != g_primary_fb_id);
	if (!full_update && dw > 0 && dh > 0 &&
	    (dw < g_state.width || dh < g_state.height)) {
		vgpu_rect_include(&dx, &dy, &dw, &dh, &dirty_valid,
		    dx, dy, dw, dh);
	}
	if (!full_update && g_cursor_valid) {
		vgpu_rect_include(&dx, &dy, &dw, &dh,
		    &dirty_valid, g_cursor_x, g_cursor_y,
		    g_cursor_w, g_cursor_h);
	}
	if (!full_update && cursor) {
		vgpu_rect_include(&dx, &dy, &dw, &dh,
		    &dirty_valid, cx, cy, cw, ch);
	}

	if (!full_update && dirty_valid) {
		if (vgpu_drm_blit_rect(src, src_y, dx, dy, dw, dh) != 0) {
			return (-1);
		}
	} else {
		if (vgpu_drm_blit_full(src, src_y) != 0) {
			return (-1);
		}
		dx = 0;
		dy = 0;
		dw = g_state.width;
		dh = g_state.height;
		dirty_valid = 1;
	}

	if (cursor) {
		if (rapi_blend_argb32_to_raw(cursor, g_state.backing,
		    g_state.pitch, (u8)g_state.bpp, g_state.width,
		    g_state.height, cx, cy, &cursor_dirty) != DRM_OK) {
			return (-1);
		}
		vgpu_rect_include(&dx, &dy, &dw, &dh, &dirty_valid,
		    cursor_dirty.x, cursor_dirty.y,
		    cursor_dirty.width, cursor_dirty.height);
		g_cursor_valid = 1;
		g_cursor_x = cursor_dirty.x;
		g_cursor_y = cursor_dirty.y;
		g_cursor_w = cursor_dirty.width;
		g_cursor_h = cursor_dirty.height;
	} else {
		g_cursor_valid = 0;
	}

	g_primary_fb_id = fb_id;
	return (vgpu_drm_flush_rect(dx, dy, dw, dh));
}

static void
vgpu_drm_shutdown(void)
{
	drm_virtio_gpu_display_shutdown();
}

static const drm_driver_t g_virtio_gpu_driver = {
	.name		= "virtio-gpu",
	.priority	= 50,
	.probe		= vgpu_drm_probe,
	.init		= vgpu_drm_init,
	.atomic_commit	= vgpu_drm_atomic_commit,
	.shutdown	= vgpu_drm_shutdown,
};

const drm_driver_t *
drm_virtio_gpu_driver_get(void)
{
	return (&g_virtio_gpu_driver);
}
