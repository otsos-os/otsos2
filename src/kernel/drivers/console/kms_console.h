/*
 * Copyright (c) 2026, otsos team
 */

#ifndef KERNEL_DRIVERS_CONSOLE_KMS_CONSOLE_H
#define KERNEL_DRIVERS_CONSOLE_KMS_CONSOLE_H

#include <drm/drm.h>
#include <drm/rapi/rapi.h>

/*
 * kms_console — kernel-internal display console driver. It is NOT part of
 * KMS; it is a client that uses DRM/KMS to show a GEM-backed framebuffer.
 * It owns a GEM buffer + framebuffer sized to the active mode, tracks the
 * dirty region, and flushes it via KMS atomic commits.
 *
 * Text/glyph/rectangle drawing lives in rapi. This driver only manages the
 * buffer, panning state, dirty-rect and flush.
 */

struct kms_console {
	drm_handle_t	gem;
	drm_id_t	fb;
	u32		width;		/* in pixels */
	u32		height;		/* visible screen height */
	u32		pitch;
	u8		bpp;
	u32		cols;		/* in text cells (8x16 font) */
	u32		rows;
	int		ready;
	/* offset-based scrolling state */
	u32		buf_h;		/* total GEM buffer height */
	u32		pan_y;		/* vertical offset of visible region */
	/* dirty-rect tracking */
	u32		dirty_x1, dirty_y1, dirty_x2, dirty_y2;
	int		dirty;
};

typedef struct kms_console kms_console_t;

/* Get the singleton kernel console. Initializes it on first call. */
kms_console_t	*kms_kernel_console(void);

/* Reset the singleton kernel console. */
void			kms_kernel_console_reset(void);

/* Bring up / tear down a console instance. */
int			kms_console_init(kms_console_t *con);
void			kms_console_shutdown(kms_console_t *con);

/* Flush the dirty region to the screen via KMS atomic commit. */
int			kms_console_flush(kms_console_t *con);

/* Fill the surface descriptor for rapi drawing. */
void			kms_console_surface(const kms_console_t *con,
			    rapi_surface_t *out);

/* Extend the dirty region. */
void			kms_console_mark_dirty(kms_console_t *con,
			    u32 x, u32 y, u32 w, u32 h);

#endif
