/* !DEFINES!

$define %type srapi_instance as SRAPI loader object
$define %type srapi_device as DRM backed SRAPI device
$define %func srapiCreateInstance as function with args desc, out instance
$define %func srapiDestroyInstance as procedure with args instance
$define %func srapiCreateDevice as function with args instance, desc, out
$define %func srapiDestroyDevice as procedure with args device
$define %func srapiCreateBuffer as function with args device, desc, out
$define %func srapiCreateShader as function with args device, desc, out
$define %func srapiCreatePipeline as function with args device, desc, out
$define %func srapiCreateCommandBuffer as function with args device, out

*/

/* !SPACE!

$space %internal image_release, image_init_backbuffer
$space %export srapiCreateInstance, srapiDestroyInstance
$space %export srapiCreateDevice, srapiDestroyDevice, srapiDeviceInfo
$space %export srapiDeviceBackbuffer, srapiCreateBuffer, srapiDestroyBuffer
$space %export srapiMapBuffer, srapiUnmapBuffer, srapiBufferWrite
$space %export srapiCreateShader, srapiDestroyShader, srapiCreatePipeline
$space %export srapiDestroyPipeline, srapiCreateCommandBuffer
$space %export srapiDestroyCommandBuffer

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <native.h>
#include <srapi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "srapi_private.h"

static void
image_release(struct srapi_image *image)
{
	if (!image) {
		return;
	}
	if (image->fb != 0) {
		(void)srapi_backend_unbind(image->device, image);
		(void)drmFbDestroy(image->fb);
		image->fb = 0;
	}
	if (image->pixels && image->size != 0) {
		(void)memUnmap(image->pixels, image->size);
		image->pixels = NULL;
	}
	if (image->gem != 0) {
		(void)drmGemClose(image->gem);
		image->gem = 0;
	}
}

static int
image_init_backbuffer(srapi_device_t *device)
{
	struct srapi_image	*image;
	size_t			size;
	void			*pixels;
	uint32_t		gem, fb, bytes_pp;

	image = &device->backbuffer;
	bytes_pp = device->info.bpp / 8;
	if (device->info.width == 0 || device->info.height == 0 ||
	    bytes_pp == 0 || device->info.pitch == 0) {
		return (SRAPI_ERR_NO_DEVICE);
	}
	if (device->info.bpp != 16 && device->info.bpp != 24 &&
	    device->info.bpp != 32) {
		return (SRAPI_ERR_UNSUPPORTED);
	}

	size = (size_t)device->info.pitch * (size_t)device->info.height;
	gem = 0;
	if (drmGemCreate(size, &gem) != 0 || gem == 0) {
		return (SRAPI_ERR_DRIVER);
	}
	pixels = drmGemMmap(gem, size, API_MAP_READ | API_MAP_WRITE);
	if (!pixels) {
		(void)drmGemClose(gem);
		return (SRAPI_ERR_DRIVER);
	}
	fb = 0;
	if (drmFbCreate(gem, device->info.width, device->info.height,
	    device->info.pitch, (uint8_t)device->info.bpp, &fb) != 0 ||
	    fb == 0) {
		(void)memUnmap(pixels, size);
		(void)drmGemClose(gem);
		return (SRAPI_ERR_DRIVER);
	}

	memset(image, 0, sizeof(*image));
	image->device = device;
	image->pixels = pixels;
	image->size = size;
	image->gem = gem;
	image->fb = fb;
	image->width = device->info.width;
	image->height = device->info.height;
	image->pitch = device->info.pitch;
	image->bpp = device->info.bpp;
	srapi_image_mark_dirty(image, 0, 0, image->width, image->height);
	return (SRAPI_OK);
}

int
srapiCreateInstance(const struct srapi_instance_desc *desc,
    srapi_instance_t **out)
{
	srapi_instance_t	*instance;

	if (!out) {
		return (SRAPI_ERR_INVALID);
	}
	instance = calloc(1, sizeof(*instance));
	if (!instance) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	if (desc) {
		instance->flags = desc->flags;
	}
	*out = instance;
	return (SRAPI_OK);
}

void
srapiDestroyInstance(srapi_instance_t *instance)
{
	free(instance);
}

int
srapiCreateDevice(srapi_instance_t *instance,
    const struct srapi_device_desc *desc, srapi_device_t **out)
{
	srapi_device_t	*device;
	int		ret;

	if (!instance || !out) {
		return (SRAPI_ERR_INVALID);
	}
	device = calloc(1, sizeof(*device));
	if (!device) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	device->instance = instance;
	if (desc) {
		device->flags = desc->flags;
	}
	if (drmInfo(&device->info) != 0 || !device->info.available) {
		free(device);
		return (SRAPI_ERR_NO_DEVICE);
	}
	if (drmGetObjects(&device->objects) != 0 ||
	    device->objects.primary_plane_id == 0) {
		free(device);
		return (SRAPI_ERR_NO_DEVICE);
	}
	ret = image_init_backbuffer(device);
	if (ret != SRAPI_OK) {
		free(device);
		return (ret);
	}
	*out = device;
	return (SRAPI_OK);
}

void
srapiDestroyDevice(srapi_device_t *device)
{
	if (!device) {
		return;
	}
	image_release(&device->backbuffer);
	free(device);
}

int
srapiDeviceInfo(srapi_device_t *device, struct srapi_device_info *out)
{
	if (!device || !out) {
		return (SRAPI_ERR_INVALID);
	}
	memset(out, 0, sizeof(*out));
	out->width = device->info.width;
	out->height = device->info.height;
	out->pitch = device->info.pitch;
	out->bpp = device->info.bpp;
	memcpy(out->driver_name, device->info.driver_name,
	    sizeof(out->driver_name));
	return (SRAPI_OK);
}

srapi_image_t *
srapiDeviceBackbuffer(srapi_device_t *device)
{
	if (!device) {
		return (NULL);
	}
	return (&device->backbuffer);
}

int
srapiCreateBuffer(srapi_device_t *device,
    const struct srapi_buffer_desc *desc, srapi_buffer_t **out)
{
	srapi_buffer_t	*buffer;

	if (!device || !desc || !out || desc->size == 0) {
		return (SRAPI_ERR_INVALID);
	}
	buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	buffer->data = calloc(1, desc->size);
	if (!buffer->data) {
		free(buffer);
		return (SRAPI_ERR_NO_MEMORY);
	}
	buffer->device = device;
	buffer->size = desc->size;
	buffer->usage = desc->usage;
	*out = buffer;
	return (SRAPI_OK);
}

void
srapiDestroyBuffer(srapi_buffer_t *buffer)
{
	if (!buffer) {
		return;
	}
	free(buffer->data);
	free(buffer);
}

int
srapiMapBuffer(srapi_buffer_t *buffer, void **out)
{
	if (!buffer || !out) {
		return (SRAPI_ERR_INVALID);
	}
	*out = buffer->data;
	return (SRAPI_OK);
}

void
srapiUnmapBuffer(srapi_buffer_t *buffer)
{
	(void)buffer;
}

int
srapiBufferWrite(srapi_buffer_t *buffer, size_t offset, const void *data,
    size_t size)
{
	if (!buffer || !data || offset > buffer->size ||
	    size > buffer->size - offset) {
		return (SRAPI_ERR_RANGE);
	}
	memcpy((uint8_t *)buffer->data + offset, data, size);
	return (SRAPI_OK);
}

int
srapiCreateShader(srapi_device_t *device,
    const struct srapi_shader_desc *desc, srapi_shader_t **out)
{
	srapi_shader_t	*shader;
	size_t		size;

	if (!device || !desc || !out || !desc->code ||
	    desc->code_count == 0) {
		return (SRAPI_ERR_INVALID);
	}
	if (desc->stage != SRAPI_SHADER_VERTEX &&
	    desc->stage != SRAPI_SHADER_FRAGMENT) {
		return (SRAPI_ERR_INVALID);
	}
	shader = calloc(1, sizeof(*shader));
	if (!shader) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	size = sizeof(*shader->code) * (size_t)desc->code_count;
	shader->code = malloc(size);
	if (!shader->code) {
		free(shader);
		return (SRAPI_ERR_NO_MEMORY);
	}
	memcpy(shader->code, desc->code, size);
	shader->device = device;
	shader->stage = desc->stage;
	shader->code_count = desc->code_count;
	*out = shader;
	return (SRAPI_OK);
}

void
srapiDestroyShader(srapi_shader_t *shader)
{
	if (!shader) {
		return;
	}
	free(shader->code);
	free(shader);
}

int
srapiCreatePipeline(srapi_device_t *device,
    const struct srapi_pipeline_desc *desc, srapi_pipeline_t **out)
{
	srapi_pipeline_t	*pipeline;

	if (!device || !desc || !out || !desc->vertex_shader ||
	    !desc->fragment_shader || desc->vertex_layout.stride == 0 ||
	    desc->vertex_layout.attr_count > SRAPI_MAX_VERTEX_ATTRS) {
		return (SRAPI_ERR_INVALID);
	}
	if (desc->vertex_shader->stage != SRAPI_SHADER_VERTEX ||
	    desc->fragment_shader->stage != SRAPI_SHADER_FRAGMENT) {
		return (SRAPI_ERR_INVALID);
	}
	pipeline = calloc(1, sizeof(*pipeline));
	if (!pipeline) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	pipeline->device = device;
	pipeline->vertex_shader = desc->vertex_shader;
	pipeline->fragment_shader = desc->fragment_shader;
	pipeline->vertex_layout = desc->vertex_layout;
	pipeline->viewport = desc->viewport;
	pipeline->flags = desc->flags;
	*out = pipeline;
	return (SRAPI_OK);
}

void
srapiDestroyPipeline(srapi_pipeline_t *pipeline)
{
	free(pipeline);
}

int
srapiCreateCommandBuffer(srapi_device_t *device, srapi_cmd_buffer_t **out)
{
	srapi_cmd_buffer_t	*cmd;

	if (!device || !out) {
		return (SRAPI_ERR_INVALID);
	}
	cmd = calloc(1, sizeof(*cmd));
	if (!cmd) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	cmd->records = calloc(SRAPI_CMD_INITIAL_CAP, sizeof(*cmd->records));
	if (!cmd->records) {
		free(cmd);
		return (SRAPI_ERR_NO_MEMORY);
	}
	cmd->device = device;
	cmd->capacity = SRAPI_CMD_INITIAL_CAP;
	*out = cmd;
	return (SRAPI_OK);
}

void
srapiDestroyCommandBuffer(srapi_cmd_buffer_t *cmd)
{
	if (!cmd) {
		return;
	}
	free(cmd->records);
	free(cmd);
}
