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

$define %type sysaudio_device_t as one endpoint device over a ks_device
$define %type sysaudio_endpoint_t as one render/capture endpoint
$define %type ks_device_t as one audio device in the streaming graph
$define %type pc_port_t as one managed device port and its stream set
$define %type uint as 32 bit unsigned

$define %func sysaudio_init as procedure with args void
$define %func sysaudio_publish_device as function with args ks_device_t *, pc_port_t *
$define %func sysaudio_publish_endpoint as function with args sysaudio_device_t *, uint, uint
$define %func sysaudio_count as function with args void
*/

/* !SPACE!

$space %export sysaudio_init, sysaudio_publish_device
$space %export sysaudio_publish_endpoint, sysaudio_count

*/

#include <kernel/audio/sysaudio/sysaudio.h>
#include <kernel/audio/api/api_audio.h>

static sysaudio_device_t	sysaudio_devices[SYSAUDIO_MAX_DEVICES];
static u32			sysaudio_device_count;

void
sysaudio_init(void)
{
	sysaudio_device_count = 0;
	memset(sysaudio_devices, 0, sizeof(sysaudio_devices));
}

int
sysaudio_publish_device(ks_device_t *ksdev, pc_port_t *port)
{
	sysaudio_device_t	*dev;
	u32			i;

	if (ksdev == NULL || port == NULL ||
	    sysaudio_device_count >= SYSAUDIO_MAX_DEVICES) {
		return (-1);
	}
	dev = &sysaudio_devices[sysaudio_device_count];
	dev->ksdev = ksdev;
	dev->port = port;
	dev->endpoint_count = 0;
	drivers_log("[AUDIO] sysaudio_publish_device: ksdev=%s "
	    "pin_count=%u\n",
	    ksdev->obj.name[0] ? ksdev->obj.name : "?",
	    ksdev->pin_count);
	for (i = 0; i < ksdev->pin_count; i++) {
		drivers_log("[AUDIO]   pin %u flow=%u\n",
		    ksdev->pins[i]->pin_id,
		    ksdev->pins[i]->flow);
		sysaudio_publish_endpoint(dev, ksdev->pins[i]->pin_id,
		    ksdev->pins[i]->flow);
	}
	sysaudio_device_count++;
	return (0);
}

int
sysaudio_publish_endpoint(sysaudio_device_t *dev, u32 pin_id, u32 flow)
{
	sysaudio_endpoint_t	*ep;
	ks_pin_t		*pin;

	if (dev == NULL || dev->ksdev == NULL ||
	    dev->endpoint_count >= SYSAUDIO_MAX_ENDPOINTS) {
		return (-1);
	}
	pin = ks_device_find_pin(dev->ksdev, pin_id);
	if (pin == NULL) {
		return (-1);
	}
	ep = &dev->endpoints[dev->endpoint_count];
	ep->pin = pin;
	ep->stream = NULL;
	ep->flow = (ks_dataflow_t)flow;
	ep->pin_id = pin_id;
	ep->state = KS_STATE_STOP;
	snprintf(ep->name, sizeof(ep->name), "%s-pin%u",
	    dev->ksdev->obj.name[0] != '\0' ? dev->ksdev->obj.name : "hda",
	    pin_id);
	dev->endpoint_count++;

	api_audio_publish(ep->name, flow, pin_id, dev->port);
	return (0);
}

u32
sysaudio_count(void)
{
	return (sysaudio_device_count);
}