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

/* !DEFINES!

$define %type sysaudio_endpoint_t as one render/capture endpoint
$define %type sysaudio_device_t as one endpoint device over a ks_device
$define %type ks_device_t as one audio device in the streaming graph
$define %type pc_port_t as one managed device port and its stream set

$const SYSAUDIO_MAX_DEVICES as largest device set the endpoint layer tracks
$const SYSAUDIO_MAX_ENDPOINTS as endpoints one device may publish

$define %func sysaudio_init as procedure with args void
$define %func sysaudio_publish_device as function with args ks_device_t *, pc_port_t *
$define %func sysaudio_publish_endpoint as function with args sysaudio_device_t *, uint, uint
$define %func sysaudio_count as function with args void
*/

/* !SPACE!

$space %export sysaudio_init, sysaudio_publish_device
$space %export sysaudio_publish_endpoint, sysaudio_count

*/

#ifndef KERNEL_AUDIO_SYSAUDIO_SYSAUDIO_H
#define KERNEL_AUDIO_SYSAUDIO_SYSAUDIO_H

#include <mlibc/mlibc.h>
#include <kernel/audio/ks/ksobj.h>
#include <kernel/audio/portcls/pcport.h>

#define	SYSAUDIO_MAX_DEVICES	16
#define	SYSAUDIO_MAX_ENDPOINTS	64

typedef struct sysaudio_endpoint {
	ks_pin_t	*pin;
	pc_stream_t	*stream;
	ks_dataflow_t	flow;
	u32		pin_id;
	u32		state;
	char		name[KS_DEVICE_NAME_MAX];
} sysaudio_endpoint_t;

typedef struct sysaudio_device {
	ks_device_t	*ksdev;
	pc_port_t	*port;
	sysaudio_endpoint_t endpoints[SYSAUDIO_MAX_ENDPOINTS];
	u32		endpoint_count;
} sysaudio_device_t;

void		sysaudio_init(void);
int		sysaudio_publish_device(ks_device_t *ksdev, pc_port_t *port);
int		sysaudio_publish_endpoint(sysaudio_device_t *dev,
		    u32 pin_id, u32 flow);
u32		sysaudio_count(void);

#endif
