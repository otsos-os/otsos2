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

$define %type u32 as 32 bit unsigned

$define %func audio_init as procedure with args void
$define %func audio_is_initialized as function with args void
$define %func audio_device_count as function with args void

*/

/* !SPACE!

$space %internal audio_core_identify, audio_core_attach
$space %export audio_init, audio_is_initialized, audio_device_count

*/

#include <kernel/audio/audio.h>
#include <kernel/audio/api/api_audio.h>
#include <kernel/audio/sysaudio/sysaudio.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/stdio.h>

static int	audio_ready;

static void
audio_core_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "audio_core", 0) == NULL) {
		device_add_child(parent, "audio_core", 0);
	}
}

static int
audio_core_attach(device_t dev)
{
	(void)dev;
	audio_init();
	return (0);
}

static devclass_t audio_core_devclass = {
	.name		= "audio",
	.maxunit	= 1,
};

static driver_t audio_core_driver = {
	.name		= "audio_core",
	.identify	= audio_core_identify,
	.probe		= NULL,
	.attach		= audio_core_attach,
};

PSEUDO_DRIVER_MODULE(audio_core, audio_core_driver,
    audio_core_devclass, NEWBUS_PASS_CORE, NEWBUS_ORDER_MIDDLE);

void
audio_init(void)
{
	if (audio_ready) {
		return;
	}
	api_audio_init();
	sysaudio_init();
	audio_ready = 1;
	drivers_log("[AUDIO] subsystem initialized\n");
}

int
audio_is_initialized(void)
{
	return (audio_ready);
}

u32
audio_device_count(void)
{
	return (sysaudio_count());
}