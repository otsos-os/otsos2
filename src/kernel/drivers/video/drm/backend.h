#ifndef DRM_BACKEND_H
#define DRM_BACKEND_H

#include <kernel/drivers/video/drm/driver.h>

const drm_driver_t *drm_fbdev_driver_get(void);

#endif
