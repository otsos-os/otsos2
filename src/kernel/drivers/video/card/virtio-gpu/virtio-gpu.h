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
 *    and/or other materials provided with the distribution.
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

/*
 * virtio-gpu DRM driver — public API.
 *
 * This driver implements the drm_driver_t interface and bridges the
 * virtio-gpu PCI device to the DRM subsystem. It operates in 2D-only
 * mode (no VIRGL/3D). The driver is registered as a PCI driver for
 * auto-discovery, and exposes a DRM driver struct for the DRM core
 * to select at boot or after PCI enumeration.
 */

#ifndef DRM_VIRTIO_GPU_H
#define DRM_VIRTIO_GPU_H

#include <drm/drm.h>

/* Return the DRM driver struct for virtio-gpu. */
const drm_driver_t *drm_virtio_gpu_driver_get(void);

/* Register the virtio-gpu PCI driver (call before pci_init). */
int drm_virtio_gpu_pci_register(void);

/* Check if the virtio-gpu hardware has been probed and initialised. */
int drm_virtio_gpu_is_ready(void);

/* Initialise the virtio-gpu display (called by DRM init or reinit).
 * Returns 0 on success. This sets up the resource, backing store, and
 * scanout, and publishes the display geometry to the DRM CRTC layer. */
int drm_virtio_gpu_display_init(void);

/* Shut down the virtio-gpu display. */
void drm_virtio_gpu_display_shutdown(void);

#endif
