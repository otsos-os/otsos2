#ifndef DRM_INIT_H
#define DRM_INIT_H

#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>

int drm_boot_init_mb2(multiboot2_info_t *mb_info, const char *preferred_driver);
int drm_boot_init_mb1(multiboot_info_t *mb_info, const char *preferred_driver);
int drm_switch_driver_by_id(int id);
int drm_get_active_driver_id(void);
const char *drm_get_active_driver_name(void);

#endif
