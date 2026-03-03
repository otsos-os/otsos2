#ifndef DRM_TYPES_H
#define DRM_TYPES_H

#include <mlibc/mlibc.h>

typedef struct {
  u32 width;
  u32 height;
  u32 pitch;
  u8 bpp;
  u8 *data;
  u64 hw_address;
} drm_framebuffer_t;

typedef struct {
  u32 x;
  u32 y;
  u32 width;
  u32 height;
} drm_rect_t;

typedef struct {
  int active;
  u32 mode_width;
  u32 mode_height;
} drm_crtc_state_t;

typedef struct {
  int enabled;
  drm_framebuffer_t *fb;
} drm_plane_state_t;

typedef struct {
  int connected;
} drm_connector_state_t;

typedef struct {
  drm_crtc_state_t crtc;
  drm_plane_state_t primary_plane;
  drm_connector_state_t connector;
} drm_atomic_state_t;

#endif
