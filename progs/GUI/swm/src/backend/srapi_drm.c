/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

#include "backend.h"

#include <stdlib.h>

struct swm_output {
	srapi_instance_t		*instance;
	srapi_device_t		*device;
	srapi_cmd_buffer_t	*commands;
	srapi_image_t		*backbuffer;
	struct srapi_device_info info;
};

int
swm_output_create_default(swm_output_t **out)
{
	struct srapi_instance_desc instance_desc;
	struct srapi_device_desc device_desc;
	swm_output_t *output;
	int ret;

	if (out == NULL) {
		return (SRAPI_ERR_INVALID);
	}
	*out = NULL;
	output = calloc(1, sizeof(*output));
	if (output == NULL) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	instance_desc.flags = 0;
	ret = srapiCreateInstance(&instance_desc, &output->instance);
	if (ret != SRAPI_OK) {
		free(output);
		return (ret);
	}
	device_desc.flags = 0;
	ret = srapiCreateDevice(output->instance, &device_desc,
	    &output->device);
	if (ret != SRAPI_OK) {
		srapiDestroyInstance(output->instance);
		free(output);
		return (ret);
	}
	ret = srapiDeviceInfo(output->device, &output->info);
	if (ret != SRAPI_OK) {
		swm_output_destroy(output);
		return (ret);
	}
	output->backbuffer = srapiDeviceBackbuffer(output->device);
	if (output->backbuffer == NULL) {
		swm_output_destroy(output);
		return (SRAPI_ERR_NO_DEVICE);
	}
	ret = srapiCreateCommandBuffer(output->device, &output->commands);
	if (ret != SRAPI_OK) {
		swm_output_destroy(output);
		return (ret);
	}
	*out = output;
	return (SRAPI_OK);
}

void
swm_output_destroy(swm_output_t *output)
{
	if (output == NULL) {
		return;
	}
	if (output->commands != NULL) {
		srapiDestroyCommandBuffer(output->commands);
	}
	if (output->device != NULL) {
		srapiDestroyDevice(output->device);
	}
	if (output->instance != NULL) {
		srapiDestroyInstance(output->instance);
	}
	free(output);
}

uint32_t
swm_output_width(const swm_output_t *output)
{
	return (output != NULL ? output->info.width : 0);
}

uint32_t
swm_output_height(const swm_output_t *output)
{
	return (output != NULL ? output->info.height : 0);
}

srapi_device_t *
swm_output_device(swm_output_t *output)
{
	return (output != NULL ? output->device : NULL);
}

srapi_image_t *
swm_output_backbuffer(swm_output_t *output)
{
	return (output != NULL ? output->backbuffer : NULL);
}

int
swm_output_present(swm_output_t *output, const struct srapi_region *region)
{
	int ret;

	if (output == NULL || output->commands == NULL ||
	    output->backbuffer == NULL) {
		return (SRAPI_ERR_INVALID);
	}

	ret = srapiImageDamage(output->backbuffer, region);
	if (ret == SRAPI_OK) {
		ret = srapiCmdReset(output->commands);
	}
	if (ret == SRAPI_OK) {
		ret = srapiCmdBegin(output->commands);
	}
	if (ret == SRAPI_OK) {
		ret = srapiCmdPresent(output->commands);
	}
	if (ret == SRAPI_OK) {
		ret = srapiCmdEnd(output->commands);
	}
	if (ret == SRAPI_OK) {
		ret = srapiSubmit(output->commands);
	}
	return (ret);
}
