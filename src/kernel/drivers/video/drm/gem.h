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
 * GEM — Graphics Execution Manager.
 *
 * GEM buffers are chunks of kernel memory that back scanout framebuffers and
 * user render targets. A buffer is referenced by a process-local handle.
 * Userspace mmaps the handle (via memMap) and writes pixels directly; the
 * DRM layer never interprets the contents. There is no VRAM path yet — all
 * buffers live in system RAM (which the fbdev driver can blit to the hw
 * linear framebuffer).
 */

#ifndef DRM_GEM_H
#define DRM_GEM_H

#include <drm/drm.h>

/* Allocate a buffer of `size` bytes. Returns a handle or 0 on failure. */
drm_handle_t drm_gem_create(u64 size);

/* Look up a buffer by handle. Returns NULL if invalid. */
drm_gem_buffer_t *drm_gem_lookup(drm_handle_t handle);

/* Kernel virtual address of the buffer (NULL if invalid). */
void *drm_gem_vaddr(drm_handle_t handle);

/* Size in bytes of the buffer (0 if invalid). */
u64 drm_gem_size(drm_handle_t handle);

/* Release a handle (decrements refcount, frees when last). */
int drm_gem_close(drm_handle_t handle);

#endif
