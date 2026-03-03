#include <kernel/drivers/video/drm/backend.h>
#include <mlibc/memory.h>

static int fbdev_probe(const drm_boot_display_t *boot) {
  if (!boot) {
    return -1;
  }
  if (!boot->hw_address || !boot->pitch || !boot->width || !boot->height ||
      !boot->bpp) {
    return -1;
  }
  return 0;
}

static int fbdev_init(const drm_boot_display_t *boot, drm_framebuffer_t *out_hw_fb) {
  if (fbdev_probe(boot) != 0 || !out_hw_fb) {
    return -1;
  }

  out_hw_fb->width = boot->width;
  out_hw_fb->height = boot->height;
  out_hw_fb->pitch = boot->pitch;
  out_hw_fb->bpp = boot->bpp;
  out_hw_fb->data = (u8 *)boot->hw_address;
  out_hw_fb->hw_address = boot->hw_address;

  return 0;
}

static int fbdev_present(const drm_framebuffer_t *src_fb,
                         const drm_framebuffer_t *dst_hw_fb) {
  if (!src_fb || !dst_hw_fb || !src_fb->data || !dst_hw_fb->data) {
    return -1;
  }

  if (src_fb->width != dst_hw_fb->width || src_fb->height != dst_hw_fb->height ||
      src_fb->bpp != dst_hw_fb->bpp) {
    return -1;
  }

  u32 line_bytes = src_fb->width * (src_fb->bpp / 8);
  if (line_bytes > src_fb->pitch || line_bytes > dst_hw_fb->pitch) {
    return -1;
  }

  for (u32 y = 0; y < src_fb->height; y++) {
    memcpy(dst_hw_fb->data + (y * dst_hw_fb->pitch),
           src_fb->data + (y * src_fb->pitch), line_bytes);
  }

  return 0;
}

static int fbdev_present_rect(const drm_framebuffer_t *src_fb,
                              const drm_framebuffer_t *dst_hw_fb,
                              const drm_rect_t *rect) {
  if (!src_fb || !dst_hw_fb || !src_fb->data || !dst_hw_fb->data || !rect) {
    return -1;
  }

  if (src_fb->width != dst_hw_fb->width || src_fb->height != dst_hw_fb->height ||
      src_fb->bpp != dst_hw_fb->bpp) {
    return -1;
  }

  if (rect->x >= src_fb->width || rect->y >= src_fb->height || rect->width == 0 ||
      rect->height == 0) {
    return 0;
  }

  u32 bpp_bytes = (u32)(src_fb->bpp / 8);
  u32 x_end = rect->x + rect->width;
  u32 y_end = rect->y + rect->height;
  if (x_end > src_fb->width) {
    x_end = src_fb->width;
  }
  if (y_end > src_fb->height) {
    y_end = src_fb->height;
  }

  u32 copy_width = x_end - rect->x;
  u32 copy_bytes = copy_width * bpp_bytes;

  for (u32 y = rect->y; y < y_end; y++) {
    u8 *src = src_fb->data + (y * src_fb->pitch) + (rect->x * bpp_bytes);
    u8 *dst = dst_hw_fb->data + (y * dst_hw_fb->pitch) + (rect->x * bpp_bytes);
    memcpy(dst, src, copy_bytes);
  }

  return 0;
}

static const drm_driver_t g_fbdev_driver = {
    .name = "fbdev",
    .priority = 10,
    .probe = fbdev_probe,
    .init = fbdev_init,
    .present = fbdev_present,
    .present_rect = fbdev_present_rect,
};

const drm_driver_t *drm_fbdev_driver_get(void) { return &g_fbdev_driver; }
