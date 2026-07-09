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
#include <drm/fbdev.h>
#include <drm/kms/crtc.h>
#include <drm/kms/plane.h>
#include <drm/kms/framebuffer.h>
#include <drm/rapi/rapi.h>
#include <kernel/api/posix/posix.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mm/vm/pmap.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define PAGE_SIZE 4096
#define FBDEV_MAX_IO 0x7FFFFFFF

typedef struct fb_bitfield {
  u32 offset;
  u32 length;
  u32 msb_right;
} fb_bitfield_t;

typedef struct fb_fix_screeninfo {
  char id[16];
  unsigned long smem_start;
  u32 smem_len;
  u32 type;
  u32 type_aux;
  u32 visual;
  u16 xpanstep;
  u16 ypanstep;
  u16 ywrapstep;
  u32 line_length;
  unsigned long mmio_start;
  u32 mmio_len;
  u32 accel;
  u16 capabilities;
  u16 reserved[2];
} fb_fix_screeninfo_t;

typedef struct fb_var_screeninfo {
  u32 xres;
  u32 yres;
  u32 xres_virtual;
  u32 yres_virtual;
  u32 xoffset;
  u32 yoffset;
  u32 bits_per_pixel;
  u32 grayscale;
  fb_bitfield_t red;
  fb_bitfield_t green;
  fb_bitfield_t blue;
  fb_bitfield_t transp;
  u32 nonstd;
  u32 activate;
  u32 height;
  u32 width;
  u32 accel_flags;
  u32 pixclock;
  u32 left_margin;
  u32 right_margin;
  u32 upper_margin;
  u32 lower_margin;
  u32 hsync_len;
  u32 vsync_len;
  u32 sync;
  u32 vmode;
  u32 rotate;
  u32 colorspace;
  u32 reserved[4];
} fb_var_screeninfo_t;

typedef struct fb_cmap {
  u32 start;
  u32 len;
  u16 *red;
  u16 *green;
  u16 *blue;
  u16 *transp;
} fb_cmap_t;

typedef struct fb_con2fbmap {
  u32 console;
  u32 framebuffer;
} fb_con2fbmap_t;

typedef struct fb_vblank {
  u32 flags;
  u32 count;
  u32 vcount;
  u32 hcount;
  u32 reserved[4];
} fb_vblank_t;

static u8 *g_hw_base;
static u64 g_hw_phys;
static u64 g_hw_size;
static u32 g_hw_pitch;
static u32 g_hw_width;
static u32 g_hw_height;
static u32 g_fb_xoffset;
static u32 g_fb_yoffset;
static u32 g_fb_blank;
static u8 g_hw_bpp;
static int g_cursor_valid;
static u32 g_cursor_x, g_cursor_y, g_cursor_w, g_cursor_h;
static drm_id_t g_primary_fb_id;

static int fbdev_ready(void) {
  return (g_hw_base != NULL && g_hw_phys != 0 && g_hw_size != 0 &&
          g_hw_pitch != 0 && g_hw_width != 0 && g_hw_height != 0 &&
          g_hw_bpp != 0);
}

static int fbdev_require_privilege(void) {
  if (!proc_has_privilege(process_current())) {
    return -POSIX_EACCES;
  }
  return 0;
}

static void fbdev_bitfields(fb_var_screeninfo_t *var) {
  memset(&var->red, 0, sizeof(var->red));
  memset(&var->green, 0, sizeof(var->green));
  memset(&var->blue, 0, sizeof(var->blue));
  memset(&var->transp, 0, sizeof(var->transp));

  if (g_hw_bpp == 16) {
    var->red.offset = 11;
    var->red.length = 5;
    var->green.offset = 5;
    var->green.length = 6;
    var->blue.offset = 0;
    var->blue.length = 5;
  } else if (g_hw_bpp == 24 || g_hw_bpp == 32) {
    var->red.offset = 16;
    var->red.length = 8;
    var->green.offset = 8;
    var->green.length = 8;
    var->blue.offset = 0;
    var->blue.length = 8;
    if (g_hw_bpp == 32) {
      var->transp.offset = 24;
      var->transp.length = 8;
    }
  } else {
    var->red.length = g_hw_bpp;
    var->green.length = g_hw_bpp;
    var->blue.length = g_hw_bpp;
  }
}

static void fbdev_fill_fix(fb_fix_screeninfo_t *fix) {
  memset(fix, 0, sizeof(*fix));
  memcpy(fix->id, "otsos2-fb", 9);
  fix->smem_start = (unsigned long)g_hw_phys;
  fix->smem_len = (u32)g_hw_size;
  fix->type = DRM_FB_TYPE_PACKED_PIXELS;
  fix->visual = DRM_FB_VISUAL_TRUECOLOR;
  fix->line_length = g_hw_pitch;
  fix->accel = DRM_FB_ACCEL_NONE;
}

static void fbdev_fill_var(fb_var_screeninfo_t *var) {
  memset(var, 0, sizeof(*var));
  var->xres = g_hw_width;
  var->yres = g_hw_height;
  var->xres_virtual = g_hw_width;
  var->yres_virtual = g_hw_height;
  var->xoffset = g_fb_xoffset;
  var->yoffset = g_fb_yoffset;
  var->bits_per_pixel = g_hw_bpp;
  var->height = 0xFFFFFFFF;
  var->width = 0xFFFFFFFF;
  var->vmode = DRM_FB_VMODE_NONINTERLACED;
  fbdev_bitfields(var);
}

static int fbdev_check_var(const fb_var_screeninfo_t *var) {
  u32 activate;

  activate = var->activate & DRM_FB_ACTIVATE_MASK;
  if (activate != 0 && activate != DRM_FB_ACTIVATE_TEST) {
    return -POSIX_EINVAL;
  }
  if (var->xres != g_hw_width || var->yres != g_hw_height) {
    return -POSIX_EINVAL;
  }
  if (var->xres_virtual != 0 && var->xres_virtual != g_hw_width) {
    return -POSIX_EINVAL;
  }
  if (var->yres_virtual != 0 && var->yres_virtual != g_hw_height) {
    return -POSIX_EINVAL;
  }
  if (var->bits_per_pixel != 0 && var->bits_per_pixel != g_hw_bpp) {
    return -POSIX_EINVAL;
  }
  if (var->xoffset != 0 || var->yoffset != 0) {
    return -POSIX_EINVAL;
  }
  return 0;
}

static int fbdev_clip_io(u64 offset, u64 count, u64 *out_count) {
  if (!fbdev_ready()) {
    return -POSIX_ENODEV;
  }
  if (offset >= g_hw_size) {
    *out_count = 0;
    return 0;
  }
  if (count > g_hw_size - offset) {
    count = g_hw_size - offset;
  }
  if (count > FBDEV_MAX_IO) {
    count = FBDEV_MAX_IO;
  }
  *out_count = count;
  return 0;
}

static int fbdev_cmap_validate(const fb_cmap_t *cmap) {
  u64 bytes;

  if (cmap->len == 0) {
    return 0;
  }
  if (cmap->start >= 256 || cmap->len > 256 ||
      cmap->start + cmap->len > 256) {
    return -POSIX_EINVAL;
  }
  bytes = (u64)cmap->len * sizeof(u16);
  if (!cmap->red || !cmap->green || !cmap->blue) {
    return -POSIX_EFAULT;
  }
  if (!is_user_address(cmap->red, bytes) ||
      !is_user_address(cmap->green, bytes) ||
      !is_user_address(cmap->blue, bytes)) {
    return -POSIX_EFAULT;
  }
  if (cmap->transp && !is_user_address(cmap->transp, bytes)) {
    return -POSIX_EFAULT;
  }
  return 0;
}

static int fbdev_get_cmap(void *arg) {
  fb_cmap_t cmap;
  u32 i, idx;
  u16 val;
  int rc;

  if (!arg || !is_user_address(arg, sizeof(cmap))) {
    return -POSIX_EFAULT;
  }
  memcpy(&cmap, arg, sizeof(cmap));
  rc = fbdev_cmap_validate(&cmap);
  if (rc != 0 || cmap.len == 0) {
    return rc;
  }
  for (i = 0; i < cmap.len; i++) {
    idx = cmap.start + i;
    val = (u16)((idx << 8) | idx);
    cmap.red[i] = val;
    cmap.green[i] = val;
    cmap.blue[i] = val;
    if (cmap.transp) {
      cmap.transp[i] = 0xFFFF;
    }
  }
  return 0;
}

static int fbdev_put_cmap(void *arg) {
  fb_cmap_t cmap;

  if (!arg || !is_user_address(arg, sizeof(cmap))) {
    return -POSIX_EFAULT;
  }
  memcpy(&cmap, arg, sizeof(cmap));
  return fbdev_cmap_validate(&cmap);
}

static int fbdev_probe(const void *boot_info) {
  const drm_fbdev_boot_t *boot = (const drm_fbdev_boot_t *)boot_info;
  if (!boot) {
    return -1;
  }
  if (!boot->hw_address || !boot->pitch || !boot->width || !boot->height ||
      !boot->bpp) {
    return -1;
  }
  return 0;
}

static void fbdev_map_hw(u64 addr, u64 size) {
  u64 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
  for (u64 i = 0; i < pages; i++) {
    pmap_enter(addr + i * PAGE_SIZE, addr + i * PAGE_SIZE,
               PTE_PRESENT | PTE_RW);
  }
}

static int fbdev_init(const void *boot_info) {
  const drm_fbdev_boot_t *boot = (const drm_fbdev_boot_t *)boot_info;
  if (fbdev_probe(boot) != 0) {
    return -1;
  }

  u64 size = (u64)boot->pitch * boot->height;
  fbdev_map_hw(boot->hw_address, size);

  g_hw_base = (u8 *)boot->hw_address;
  g_hw_phys = boot->hw_address;
  g_hw_size = size;
  g_hw_pitch = boot->pitch;
  g_hw_width = boot->width;
  g_hw_height = boot->height;
  g_hw_bpp = boot->bpp;
  g_fb_xoffset = 0;
  g_fb_yoffset = 0;
  g_fb_blank = 0;
  g_cursor_valid = 0;
  g_primary_fb_id = DRM_ID_NONE;

  /* Publish geometry so the KMS layer knows the active mode. */
  drm_crtc_set_mode_geometry(boot->width, boot->height, boot->pitch,
                             boot->bpp, boot->hw_address);

  drivers_log("[FBDEV] hw fb mapped: %ux%u\n", boot->width, boot->height);
  return 0;
}

static void fbdev_blit_full(const drm_framebuffer_t *src, u32 src_y) {
  u32 line_bytes, copy, y;

  line_bytes = src->width * (u32)(src->bpp / 8);
  copy = line_bytes < src->pitch ? line_bytes : src->pitch;
  if (copy > g_hw_pitch) {
    copy = g_hw_pitch;
  }
  for (y = 0; y < src->height; y++) {
    memcpy(g_hw_base + (u64)y * g_hw_pitch,
           src->gem->data + (u64)(y + src_y) * src->pitch, copy);
  }
}

static void fbdev_blit_rect(const drm_framebuffer_t *src, u32 src_y,
                            u32 x, u32 y, u32 w, u32 h) {
  u32 bpp_bytes, x2, y2, copy, ry;

  if (x >= g_hw_width || y >= g_hw_height || w == 0 || h == 0) {
    return;
  }
  bpp_bytes = (u32)(src->bpp / 8);
  x2 = x + w;
  y2 = y + h;
  if (x2 > g_hw_width) {
    x2 = g_hw_width;
  }
  if (y2 > g_hw_height) {
    y2 = g_hw_height;
  }
  copy = (x2 - x) * bpp_bytes;
  for (ry = y; ry < y2; ry++) {
    memcpy(g_hw_base + (u64)ry * g_hw_pitch + (u64)x * bpp_bytes,
           src->gem->data + (u64)(ry + src_y) * src->pitch +
               (u64)x * bpp_bytes,
           copy);
  }
}

static void fbdev_rect_include(u32 *x, u32 *y, u32 *w, u32 *h,
                               int *valid, u32 rx, u32 ry, u32 rw,
                               u32 rh) {
  u32 x2, y2, rx2, ry2;

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
  if (rx < *x) *x = rx;
  if (ry < *y) *y = ry;
  if (rx2 > x2) x2 = rx2;
  if (ry2 > y2) y2 = ry2;
  *w = x2 - *x;
  *h = y2 - *y;
}

static int fbdev_atomic_commit(const drm_kms_state_t *state) {
  drm_framebuffer_t *src;
  drm_framebuffer_t *cursor;
  drm_id_t fb_id;
  drm_id_t cursor_id;
  rapi_rect_t cursor_dirty;
  u32 dx, dy, dw, dh, cx, cy, cw, ch;
  u32 src_y;
  int dirty_valid, full_update;

  if (!state || !g_hw_base) {
    return -1;
  }
  /* Use the primary plane for scanout. */
  fb_id = (drm_id_t)state->plane_props[0][DRM_PROP_PLANE_FB_ID];
  if (fb_id == DRM_ID_NONE) {
    return 0;
  }
  src = drm_framebuffer_get(fb_id);
  if (!src || !src->gem || !src->gem->data) {
    return -1;
  }
  if (src->width != g_hw_width || src->height != g_hw_height ||
      src->bpp != g_hw_bpp) {
    return -1;
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
    cursor_id = (drm_id_t)state->plane_props[1][DRM_PROP_PLANE_FB_ID];
    if (cursor_id != DRM_ID_NONE) {
      cursor = drm_framebuffer_get(cursor_id);
      cx = (u32)state->plane_props[1][DRM_PROP_PLANE_CRTC_X];
      cy = (u32)state->plane_props[1][DRM_PROP_PLANE_CRTC_Y];
      if (cursor) {
        cw = cursor->width;
        ch = cursor->height;
      }
    }
  }

  dirty_valid = 0;
  full_update = (fb_id != g_primary_fb_id);
  if (!full_update && dw > 0 && dh > 0 &&
      (dw < g_hw_width || dh < g_hw_height)) {
    fbdev_rect_include(&dx, &dy, &dw, &dh, &dirty_valid, dx, dy, dw, dh);
  }
  if (!full_update && g_cursor_valid) {
    fbdev_rect_include(&dx, &dy, &dw, &dh, &dirty_valid, g_cursor_x,
                       g_cursor_y, g_cursor_w, g_cursor_h);
  }
  if (!full_update && cursor) {
    fbdev_rect_include(&dx, &dy, &dw, &dh, &dirty_valid, cx, cy, cw, ch);
  }

  if (!full_update && dirty_valid) {
    fbdev_blit_rect(src, src_y, dx, dy, dw, dh);
  } else {
    fbdev_blit_full(src, src_y);
  }

  if (cursor) {
    if (rapi_blend_argb32_to_raw(cursor, g_hw_base, g_hw_pitch, g_hw_bpp,
                                 g_hw_width, g_hw_height, cx, cy,
                                 &cursor_dirty) != DRM_OK) {
      return -1;
    }
    g_cursor_valid = 1;
    g_cursor_x = cursor_dirty.x;
    g_cursor_y = cursor_dirty.y;
    g_cursor_w = cursor_dirty.width;
    g_cursor_h = cursor_dirty.height;
  } else {
    g_cursor_valid = 0;
  }
  g_primary_fb_id = fb_id;
  return 0;
}

static const drm_driver_t g_fbdev_driver = {
    .name = "fbdev",
    .priority = 10,
    .probe = fbdev_probe,
    .init = fbdev_init,
    .atomic_commit = fbdev_atomic_commit,
    .shutdown = NULL,
};

const drm_driver_t *drm_fbdev_driver_get(void) { return &g_fbdev_driver; }

int drm_fbdev_is_ready(void) { return fbdev_ready(); }

int drm_fbdev_get_info(drm_fbdev_info_t *info) {
  if (!info) {
    return -1;
  }
  if (!fbdev_ready()) {
    return -1;
  }
  info->hw_address = g_hw_phys;
  info->size = g_hw_size;
  info->pitch = g_hw_pitch;
  info->width = g_hw_width;
  info->height = g_hw_height;
  info->bpp = g_hw_bpp;
  return 0;
}

int
drm_fbdev_vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
  u64 n;
  int rc;

  (void)vn;
  rc = fbdev_require_privilege();
  if (rc != 0) {
    return rc;
  }
  rc = fbdev_clip_io(offset, count, &n);
  if (rc != 0 || n == 0) {
    return rc;
  }
  memcpy(buf, g_hw_base + offset, n);
  return (int)n;
}

int
drm_fbdev_vnode_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
  u64 n;
  int rc;

  (void)vn;
  if (count == 0) {
    return 0;
  }
  if (!buf) {
    return -POSIX_EFAULT;
  }
  rc = fbdev_require_privilege();
  if (rc != 0) {
    return rc;
  }
  rc = fbdev_clip_io(offset, count, &n);
  if (rc != 0 || n == 0) {
    return rc;
  }
  memcpy(g_hw_base + offset, buf, n);
  return (int)n;
}

int
drm_fbdev_vnode_ioctl(vnode_t *vn, u64 cmd, void *arg)
{
  fb_fix_screeninfo_t fix;
  fb_var_screeninfo_t var;
  fb_con2fbmap_t conmap;
  fb_vblank_t vblank;
  int rc;

  (void)vn;
  rc = fbdev_require_privilege();
  if (rc != 0) {
    return rc;
  }
  if (!fbdev_ready()) {
    return -POSIX_ENODEV;
  }

  switch (cmd) {
  case DRM_FBIOGET_FSCREENINFO:
    if (!arg || !is_user_address(arg, sizeof(fix))) {
      return -POSIX_EFAULT;
    }
    fbdev_fill_fix(&fix);
    memcpy(arg, &fix, sizeof(fix));
    return 0;
  case DRM_FBIOGET_VSCREENINFO:
    if (!arg || !is_user_address(arg, sizeof(var))) {
      return -POSIX_EFAULT;
    }
    fbdev_fill_var(&var);
    memcpy(arg, &var, sizeof(var));
    return 0;
  case DRM_FBIOPUT_VSCREENINFO:
    if (!arg || !is_user_address(arg, sizeof(var))) {
      return -POSIX_EFAULT;
    }
    memcpy(&var, arg, sizeof(var));
    rc = fbdev_check_var(&var);
    if (rc != 0) {
      return rc;
    }
    if ((var.activate & DRM_FB_ACTIVATE_MASK) != DRM_FB_ACTIVATE_TEST) {
      g_fb_xoffset = var.xoffset;
      g_fb_yoffset = var.yoffset;
    }
    fbdev_fill_var(&var);
    memcpy(arg, &var, sizeof(var));
    return 0;
  case DRM_FBIOPAN_DISPLAY:
    if (!arg || !is_user_address(arg, sizeof(var))) {
      return -POSIX_EFAULT;
    }
    memcpy(&var, arg, sizeof(var));
    if (var.xoffset != 0 || var.yoffset != 0) {
      return -POSIX_EINVAL;
    }
    g_fb_xoffset = 0;
    g_fb_yoffset = 0;
    fbdev_fill_var(&var);
    memcpy(arg, &var, sizeof(var));
    return 0;
  case DRM_FBIOGETCMAP:
    return fbdev_get_cmap(arg);
  case DRM_FBIOPUTCMAP:
    return fbdev_put_cmap(arg);
  case DRM_FBIOBLANK:
    if ((u64)arg > 4) {
      return -POSIX_EINVAL;
    }
    g_fb_blank = (u32)(u64)arg;
    return 0;
  case DRM_FBIOGET_CON2FBMAP:
    if (!arg || !is_user_address(arg, sizeof(conmap))) {
      return -POSIX_EFAULT;
    }
    memcpy(&conmap, arg, sizeof(conmap));
    conmap.framebuffer = 0;
    memcpy(arg, &conmap, sizeof(conmap));
    return 0;
  case DRM_FBIOPUT_CON2FBMAP:
    if (!arg || !is_user_address(arg, sizeof(conmap))) {
      return -POSIX_EFAULT;
    }
    memcpy(&conmap, arg, sizeof(conmap));
    return (conmap.framebuffer == 0) ? 0 : -POSIX_EINVAL;
  case DRM_FBIOGET_VBLANK:
    if (!arg || !is_user_address(arg, sizeof(vblank))) {
      return -POSIX_EFAULT;
    }
    memset(&vblank, 0, sizeof(vblank));
    memcpy(arg, &vblank, sizeof(vblank));
    return 0;
  case DRM_FBIO_WAITFORVSYNC:
    if (arg && !is_user_address(arg, sizeof(u32))) {
      return -POSIX_EFAULT;
    }
    return 0;
  default:
    return -POSIX_ENOTTY;
  }
}

int
drm_fbdev_vnode_stat(vnode_t *vn, posix_stat_t *st)
{
  (void)vn;
  if (!st) {
    return -1;
  }
  if (!fbdev_ready()) {
    return -POSIX_ENODEV;
  }
  memset(st, 0, sizeof(*st));
  st->st_mode = POSIX_S_IFCHR | 0600;
  st->st_size = (s64)g_hw_size;
  st->st_blksize = PAGE_SIZE;
  st->st_blocks = (s64)((g_hw_size + PAGE_SIZE - 1) / PAGE_SIZE);
  st->st_nlink = 1;
  st->st_uid = 0;
  st->st_gid = 0;
  return 0;
}
