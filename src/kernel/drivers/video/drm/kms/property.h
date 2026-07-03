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

#ifndef DRM_KMS_PROPERTY_H
#define DRM_KMS_PROPERTY_H
#define DRM_PROP_PLANE_FB_ID      1
#define DRM_PROP_PLANE_CRTC_ID    2
#define DRM_PROP_PLANE_SRC_X      3
#define DRM_PROP_PLANE_SRC_Y      4
#define DRM_PROP_PLANE_SRC_W      5
#define DRM_PROP_PLANE_SRC_H      6
#define DRM_PROP_PLANE_CRTC_X     7
#define DRM_PROP_PLANE_CRTC_Y     8
#define DRM_PROP_PLANE_CRTC_W     9
#define DRM_PROP_PLANE_CRTC_H     10
#define DRM_PROP_PLANE_DIRTY_X    11
#define DRM_PROP_PLANE_DIRTY_Y    12
#define DRM_PROP_PLANE_DIRTY_W    13
#define DRM_PROP_PLANE_DIRTY_H    14
#define DRM_PROP_CRTC_ACTIVE      15
#define DRM_PROP_CRTC_MODE_W      16
#define DRM_PROP_CRTC_MODE_H      17
#define DRM_PROP_CRTC_MODE_ID     18
#define DRM_PROP_CONNECTOR_CRTC_ID    19
#define DRM_PROP_CONNECTOR_CONNECTED  20
#define DRM_KMS_PROP_COUNT        21
#define DRM_ATOMIC_NONBLOCK       0x01
#define DRM_ATOMIC_BLOCK          0x02
#define DRM_ATOMIC_ALLOW_MODESET  0x04
#define DRM_ATOMIC_TEST_ONLY      0x08

#endif
