/*
 * Copyright (c) 2026, otsos team
 *
 * [.BSD-2-clause license text...]
 */

/* !DEFINES!

$define %type drm_driver_t as struct with DRM driver vtable

$define %func drm_virtio_gpu_driver_get as function with args void
$define %func drm_virtio_gpu_pci_register as function with args void
$define %func drm_virtio_gpu_is_ready as function with args void
$define %func drm_virtio_gpu_display_init as function with args void
$define %func drm_virtio_gpu_display_shutdown as procedure with args void

*/

/* !SPACE!

$space %export drm_virtio_gpu_driver_get
$space %export drm_virtio_gpu_pci_register, drm_virtio_gpu_is_ready
$space %export drm_virtio_gpu_display_init
$space %export drm_virtio_gpu_display_shutdown

*/

#ifndef DRM_VIRTIO_GPU_H
#define DRM_VIRTIO_GPU_H

#include <drm/drm.h>

const drm_driver_t *drm_virtio_gpu_driver_get(void);
int	drm_virtio_gpu_pci_register(void);
int	drm_virtio_gpu_is_ready(void);
int	drm_virtio_gpu_display_init(void);
void	drm_virtio_gpu_display_shutdown(void);

#endif
