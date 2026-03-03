#include <kernel/drivers/video/drm/atomic.h>
#include <kernel/drivers/video/drm/driver.h>
#include <kernel/drivers/video/drm/init.h>
#include <lib/com1.h>

#define PAGE_SIZE 4096

static void drm_map_hw_buffer(const drm_boot_display_t *boot) {
  u64 fb_size = (u64)boot->pitch * (u64)boot->height;
  u64 fb_pages = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

  extern void mmu_map_page(u64 vaddr, u64 paddr, u64 flags);

  for (u64 i = 0; i < fb_pages; i++) {
    mmu_map_page(boot->hw_address + i * PAGE_SIZE,
                 boot->hw_address + i * PAGE_SIZE, 0x3);
  }
}

static drm_boot_display_t g_boot_display;
static int g_boot_display_ready = 0;

static int drm_boot_init_common(const drm_boot_display_t *boot,
                                const char *preferred_driver) {
  if (!boot) {
    return -1;
  }

  drm_map_hw_buffer(boot);

  const drm_driver_t *driver = drm_driver_select(boot, preferred_driver);
  if (!driver) {
    com1_write_string("[DRM] no suitable drm_driver found\n");
    return -1;
  }

  if (drm_atomic_init(driver, boot) != 0) {
    com1_printf("[DRM] atomic init failed for driver '%s'\n", driver->name);
    return -1;
  }

  g_boot_display = *boot;
  g_boot_display_ready = 1;
  com1_printf("[DRM] initialized driver: %s\n", drm_atomic_get_driver_name());
  return 0;
}

int drm_boot_init_mb2(multiboot2_info_t *mb_info, const char *preferred_driver) {
  multiboot2_tag_framebuffer_t *fb_tag =
      (multiboot2_tag_framebuffer_t *)multiboot2_find_tag(
          mb_info, MULTIBOOT2_TAG_TYPE_FRAMEBUFFER);

  if (!fb_tag) {
    com1_write_string("[DRM] MB2 framebuffer tag not found\n");
    return -1;
  }

  drm_boot_display_t boot = {
      .hw_address = (u64)fb_tag->framebuffer_addr,
      .pitch = fb_tag->framebuffer_pitch,
      .width = fb_tag->framebuffer_width,
      .height = fb_tag->framebuffer_height,
      .bpp = fb_tag->framebuffer_bpp,
  };

  return drm_boot_init_common(&boot, preferred_driver);
}

int drm_boot_init_mb1(multiboot_info_t *mb_info, const char *preferred_driver) {
  if ((mb_info->flags & MULTIBOOT_FLAG_FRAMEBUFFER) == 0 ||
      mb_info->framebuffer_addr == 0) {
    com1_write_string("[DRM] MB1 framebuffer info not available\n");
    return -1;
  }

  drm_boot_display_t boot = {
      .hw_address = (u64)mb_info->framebuffer_addr,
      .pitch = mb_info->framebuffer_pitch,
      .width = mb_info->framebuffer_width,
      .height = mb_info->framebuffer_height,
      .bpp = mb_info->framebuffer_bpp,
  };

  return drm_boot_init_common(&boot, preferred_driver);
}

int drm_switch_driver_by_id(int id) {
  if (id < 0 || !g_boot_display_ready) {
    return -1;
  }

  const drm_driver_t *candidate = drm_driver_get_by_index((u32)id);
  if (!candidate || !candidate->name) {
    return -1;
  }

  const drm_driver_t *driver =
      drm_driver_select(&g_boot_display, candidate->name);
  if (!driver) {
    return -1;
  }

  if (drm_atomic_init(driver, &g_boot_display) != 0) {
    return -1;
  }

  return 0;
}

int drm_get_active_driver_id(void) { return drm_driver_get_selected_index(); }

const char *drm_get_active_driver_name(void) {
  return drm_driver_get_selected_name();
}
