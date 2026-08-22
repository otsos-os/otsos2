/* !DEFINES!

$define %type srapi_instance as opaque SRAPI instance
$define %type srapi_device as opaque SRAPI device
$define %type srapi_buffer as opaque SRAPI buffer
$define %type srapi_image as opaque SRAPI image
$define %type srapi_surface as opaque CPU pixel surface
$define %type srapi_shader as opaque SRAPI shader module
$define %type srapi_pipeline as opaque SRAPI graphics pipeline
$define %type srapi_cmd_buffer as opaque SRAPI command buffer
$define %type srapi_input_event as SRAPI input event
$define %type srapi_input_state as cached SRAPI input state
$define %type srapi_region as rectangular pixel region
$define %type srapi_surface_desc as CPU pixel surface descriptor
$define %type srapi_vm_inst as one shader VM instruction
$define %func srapiCreateInstance as function with args desc, out instance
$define %func srapiCreateDevice as function with args instance, desc, out device
$define %func srapiCreateBuffer as function with args device, desc, out buffer
$define %func srapiCreateSurface as function with args device, desc, out surface
$define %func srapiDestroySurface as procedure with args surface
$define %func srapiMapSurface as function with args surface, out, out pitch
$define %func srapiUnmapSurface as procedure with args surface
$define %func srapiSurfaceWrite as function with args surface, region, pixels, pitch
$define %func srapiSurfaceSetPalette as function with args surface, first, colors, count
$define %func srapiImagePixels as function with args image
$define %func srapiImageDamage as function with args image, region
$define %func srapiCreateShader as function with args device, desc, out shader
$define %func srapiComputeShader as function with args shader
$define %func srapiCreatePipeline as function with args device, desc, out pipeline
$define %func srapiCreateCommandBuffer as function with args device, out command buffer
$define %func srapiPollInput as function with args device, events, max events
$define %func srapiFlushInput as function with args device
$define %func srapiGetInputState as function with args device, out state
$define %func srapiInputKeyDown as function with args state, key
$define %func srapiCmdBlitSurface as function with args command buffer, surface, regions
$define %func srapiSubmit as function with args command buffer

*/

/* !SPACE!

$space %export srapi_instance, srapi_device, srapi_buffer, srapi_image
$space %export srapi_surface, srapi_shader, srapi_pipeline
$space %export srapi_cmd_buffer, srapi_input_event, srapi_input_state
$space %export srapi_region, srapi_surface_desc, srapi_vm_inst
$space %export srapiCreateInstance, srapiDestroyInstance
$space %export srapiCreateDevice, srapiDestroyDevice, srapiDeviceInfo
$space %export srapiDeviceBackbuffer, srapiCreateBuffer, srapiDestroyBuffer
$space %export srapiImageWidth, srapiImageHeight, srapiImagePitch
$space %export srapiImageBpp, srapiImagePixels, srapiImageDamage
$space %export srapiMapBuffer, srapiUnmapBuffer, srapiBufferWrite
$space %export srapiCreateSurface, srapiDestroySurface, srapiMapSurface
$space %export srapiUnmapSurface, srapiSurfaceWrite
$space %export srapiSurfaceSetPalette
$space %export srapiCreateShader, srapiComputeShader, srapiDestroyShader
$space %export srapiCreatePipeline
$space %export srapiDestroyPipeline, srapiCreateCommandBuffer
$space %export srapiDestroyCommandBuffer, srapiCmdReset, srapiCmdBegin
$space %export srapiCmdEnd, srapiCmdClearColor, srapiCmdClearRect
$space %export srapiCmdBlitSurface, srapiCmdBindPipeline
$space %export srapiCmdBindVertexBuffer
$space %export srapiCmdPushConstants, srapiCmdSetViewport, srapiCmdDraw
$space %export srapiCmdPresent, srapiSubmit
$space %export srapiPollInput, srapiFlushInput, srapiGetInputState
$space %export srapiInputKeyDown

*/

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

#ifndef SRAPI_H
#define SRAPI_H

#include <stddef.h>
#include <stdint.h>

#define SRAPI_VERSION_MAJOR	0
#define SRAPI_VERSION_MINOR	1
#define SRAPI_FIXED_ONE		65536
#define SRAPI_MAX_VERTEX_ATTRS	8
#define SRAPI_MAX_PUSH_CONSTANTS 16
#define SRAPI_VM_REGS		32
#define SRAPI_VM_IO_SLOTS	16
#define SRAPI_INPUT_KEY_BITS	256
#define SRAPI_INPUT_KEY_WORDS	(SRAPI_INPUT_KEY_BITS / 32)

#define SRAPI_FIXED_FROM_INT(v)	((int32_t)((v) * SRAPI_FIXED_ONE))
#define SRAPI_FIXED_FROM_RATIO(n, d) \
	((int32_t)(((int64_t)(n) * SRAPI_FIXED_ONE) / (d)))

typedef struct srapi_instance	srapi_instance_t;
typedef struct srapi_device	srapi_device_t;
typedef struct srapi_buffer	srapi_buffer_t;
typedef struct srapi_image	srapi_image_t;
typedef struct srapi_surface	srapi_surface_t;
typedef struct srapi_shader	srapi_shader_t;
typedef struct srapi_pipeline	srapi_pipeline_t;
typedef struct srapi_cmd_buffer	srapi_cmd_buffer_t;

enum srapi_result {
	SRAPI_OK = 0,
	SRAPI_ERR_INVALID = -1,
	SRAPI_ERR_NO_MEMORY = -2,
	SRAPI_ERR_NO_DEVICE = -3,
	SRAPI_ERR_UNSUPPORTED = -4,
	SRAPI_ERR_DRIVER = -5,
	SRAPI_ERR_STATE = -6,
	SRAPI_ERR_RANGE = -7,
	SRAPI_ERR_SHADER = -8
};

enum srapi_buffer_usage {
	SRAPI_BUFFER_VERTEX = 0x00000001,
	SRAPI_BUFFER_INDEX = 0x00000002,
	SRAPI_BUFFER_UNIFORM = 0x00000004
};

enum srapi_surface_format {
	SRAPI_SURFACE_FORMAT_ARGB8888 = 1,
	SRAPI_SURFACE_FORMAT_INDEX8 = 2
};

enum srapi_blit_flags {
	SRAPI_BLIT_NEAREST = 0x00000001
};

enum srapi_shader_stage {
	SRAPI_SHADER_VERTEX = 1,
	SRAPI_SHADER_FRAGMENT = 2
};

enum srapi_input_type {
	SRAPI_INPUT_NONE = 0,
	SRAPI_INPUT_KEYBOARD = 1,
	SRAPI_INPUT_MOUSE = 2
};

enum srapi_key_event_flags {
	SRAPI_KEY_PRESS = 0x00000001,
	SRAPI_KEY_RELEASE = 0x00000002,
	SRAPI_KEY_REPEAT = 0x00000004,
	SRAPI_KEY_EXTENDED = 0x00000008
};

#define SRAPI_KEY_ENTER		0x0028
#define SRAPI_KEY_ESCAPE	0x0029
#define SRAPI_KEY_BACKSPACE	0x002A
#define SRAPI_KEY_F11		0x0044
#define SRAPI_KEY_LALT		0x00E2
#define SRAPI_KEY_LSUPER	0x00E3
#define SRAPI_KEY_RSUPER	0x00E7

#define SRAPI_MOD_LSHIFT	0x00000001
#define SRAPI_MOD_RSHIFT	0x00000002
#define SRAPI_MOD_LCTRL	0x00000004
#define SRAPI_MOD_RCTRL	0x00000008
#define SRAPI_MOD_LALT		0x00000010
#define SRAPI_MOD_RALT		0x00000020
#define SRAPI_MOD_LSUPER	0x00000040
#define SRAPI_MOD_RSUPER	0x00000080
#define SRAPI_MOD_CTRL	(SRAPI_MOD_LCTRL | SRAPI_MOD_RCTRL)
#define SRAPI_MOD_ALT		(SRAPI_MOD_LALT | SRAPI_MOD_RALT)
#define SRAPI_MOD_SUPER	(SRAPI_MOD_LSUPER | SRAPI_MOD_RSUPER)

enum srapi_mouse_event_flags {
	SRAPI_MOUSE_MOVE = 0x00000001,
	SRAPI_MOUSE_BUTTON = 0x00000002,
	SRAPI_MOUSE_WHEEL = 0x00000004,
	SRAPI_MOUSE_OVERFLOW = 0x00000008,
	SRAPI_MOUSE_ABSOLUTE = 0x00000010
};

enum srapi_input_flags {
	SRAPI_INPUT_DROPPED = 0x80000000U
};

enum srapi_mouse_button {
	SRAPI_MOUSE_LEFT = 0x00000001,
	SRAPI_MOUSE_RIGHT = 0x00000002,
	SRAPI_MOUSE_MIDDLE = 0x00000004,
	SRAPI_MOUSE_X1 = 0x00000008,
	SRAPI_MOUSE_X2 = 0x00000010
};

enum srapi_vertex_format {
	SRAPI_VERTEX_FORMAT_FIXED2 = 1,
	SRAPI_VERTEX_FORMAT_UNORM8_4 = 2,
	SRAPI_VERTEX_FORMAT_FIXED4 = 3
};

enum srapi_vertex_location {
	SRAPI_VERTEX_LOCATION_POSITION = 0,
	SRAPI_VERTEX_LOCATION_COLOR = 1,
	SRAPI_VERTEX_LOCATION_USER0 = 8
};

enum srapi_vm_op {
	SRAPI_VM_NOP = 0,
	SRAPI_VM_END = 1,
	SRAPI_VM_MOV = 2,
	SRAPI_VM_MOV_IMM = 3,
	SRAPI_VM_LOAD_IN = 4,
	SRAPI_VM_LOAD_PUSH = 5,
	SRAPI_VM_STORE_OUT = 6,
	SRAPI_VM_ADD = 7,
	SRAPI_VM_SUB = 8,
	SRAPI_VM_MUL = 9,
	SRAPI_VM_DIV = 10,
	SRAPI_VM_MIN = 11,
	SRAPI_VM_MAX = 12,
	SRAPI_VM_CLAMP01 = 13
};

enum srapi_io_slot {
	SRAPI_IO_X = 0,
	SRAPI_IO_Y = 1,
	SRAPI_IO_Z = 2,
	SRAPI_IO_W = 3,
	SRAPI_IO_R = 4,
	SRAPI_IO_G = 5,
	SRAPI_IO_B = 6,
	SRAPI_IO_A = 7,
	SRAPI_IO_U0 = 8,
	SRAPI_IO_U1 = 9,
	SRAPI_IO_U2 = 10,
	SRAPI_IO_U3 = 11,
	SRAPI_IO_PIXEL_X = 12,
	SRAPI_IO_PIXEL_Y = 13
};

struct srapi_vm_inst {
	uint8_t		op;
	uint8_t		dst;
	uint8_t		src0;
	uint8_t		src1;
	int32_t		imm;
};

#define SRAPI_VM_INST(op, dst, src0, src1, imm) \
	{ (uint8_t)(op), (uint8_t)(dst), (uint8_t)(src0), \
	    (uint8_t)(src1), (int32_t)(imm) }

struct srapi_instance_desc {
	uint32_t	flags;
};

struct srapi_device_desc {
	uint32_t	flags;
};

struct srapi_device_info {
	uint32_t	width;
	uint32_t	height;
	uint32_t	pitch;
	uint32_t	bpp;
	char		driver_name[32];
};

struct srapi_region {
	uint32_t	x;
	uint32_t	y;
	uint32_t	width;
	uint32_t	height;
};

struct srapi_input_event {
	uint64_t	timestamp;
	uint64_t	seq;
	uint32_t	type;
	uint32_t	device;
	uint32_t	flags;
	uint32_t	lost;
	int32_t		x;
	int32_t		y;
	int32_t		dx;
	int32_t		dy;
	int32_t		dz;
	uint32_t	buttons;
	uint32_t	key;
	uint32_t	raw;
	uint32_t	mods;
	uint32_t	ch;
};

struct srapi_input_state {
	uint64_t	last_timestamp;
	uint32_t	keys[SRAPI_INPUT_KEY_WORDS];
	int32_t		mouse_x;
	int32_t		mouse_y;
	int32_t		mouse_dx;
	int32_t		mouse_dy;
	int32_t		mouse_dz;
	uint32_t	mouse_buttons;
	uint32_t	mods;
	uint32_t	events;
	uint32_t	dropped;
};

struct srapi_buffer_desc {
	size_t		size;
	uint32_t	usage;
};

struct srapi_surface_desc {
	uint32_t	width;
	uint32_t	height;
	uint32_t	format;
	uint32_t	pitch;
	uint32_t	flags;
};

struct srapi_shader_desc {
	uint32_t			stage;
	const struct srapi_vm_inst	*code;
	uint32_t			code_count;
};

struct srapi_vertex_attr {
	uint32_t	location;
	uint32_t	format;
	uint32_t	offset;
};

struct srapi_vertex_layout {
	uint32_t			stride;
	uint32_t			attr_count;
	struct srapi_vertex_attr	attrs[SRAPI_MAX_VERTEX_ATTRS];
};

struct srapi_viewport {
	uint32_t	x;
	uint32_t	y;
	uint32_t	width;
	uint32_t	height;
};

struct srapi_pipeline_desc {
	srapi_shader_t			*vertex_shader;
	srapi_shader_t			*fragment_shader;
	struct srapi_vertex_layout	vertex_layout;
	struct srapi_viewport		viewport;
	uint32_t			flags;
};

int	srapiCreateInstance(const struct srapi_instance_desc *desc,
	    srapi_instance_t **out);
void	srapiDestroyInstance(srapi_instance_t *instance);

int	srapiCreateDevice(srapi_instance_t *instance,
	    const struct srapi_device_desc *desc, srapi_device_t **out);
void	srapiDestroyDevice(srapi_device_t *device);
int	srapiDeviceInfo(srapi_device_t *device, struct srapi_device_info *out);
srapi_image_t	*srapiDeviceBackbuffer(srapi_device_t *device);
uint32_t	srapiImageWidth(const srapi_image_t *image);
uint32_t	srapiImageHeight(const srapi_image_t *image);
uint32_t	srapiImagePitch(const srapi_image_t *image);
uint32_t	srapiImageBpp(const srapi_image_t *image);
void	*srapiImagePixels(srapi_image_t *image);
int	srapiImageDamage(srapi_image_t *image,
	    const struct srapi_region *region);

int	srapiCreateBuffer(srapi_device_t *device,
	    const struct srapi_buffer_desc *desc, srapi_buffer_t **out);
void	srapiDestroyBuffer(srapi_buffer_t *buffer);
int	srapiMapBuffer(srapi_buffer_t *buffer, void **out);
void	srapiUnmapBuffer(srapi_buffer_t *buffer);
int	srapiBufferWrite(srapi_buffer_t *buffer, size_t offset,
	    const void *data, size_t size);

int	srapiCreateSurface(srapi_device_t *device,
	    const struct srapi_surface_desc *desc, srapi_surface_t **out);
void	srapiDestroySurface(srapi_surface_t *surface);
int	srapiMapSurface(srapi_surface_t *surface, void **out,
	    uint32_t *out_pitch);
void	srapiUnmapSurface(srapi_surface_t *surface);
int	srapiSurfaceWrite(srapi_surface_t *surface,
	    const struct srapi_region *region, const void *pixels,
	    uint32_t src_pitch);
int	srapiSurfaceSetPalette(srapi_surface_t *surface, uint32_t first,
	    const uint32_t *colors, uint32_t count);

int	srapiCreateShader(srapi_device_t *device,
	    const struct srapi_shader_desc *desc, srapi_shader_t **out);
int	srapiComputeShader(srapi_shader_t *shader);
void	srapiDestroyShader(srapi_shader_t *shader);

int	srapiCreatePipeline(srapi_device_t *device,
	    const struct srapi_pipeline_desc *desc, srapi_pipeline_t **out);
void	srapiDestroyPipeline(srapi_pipeline_t *pipeline);

int	srapiCreateCommandBuffer(srapi_device_t *device,
	    srapi_cmd_buffer_t **out);
void	srapiDestroyCommandBuffer(srapi_cmd_buffer_t *cmd);
int	srapiCmdReset(srapi_cmd_buffer_t *cmd);
int	srapiCmdBegin(srapi_cmd_buffer_t *cmd);
int	srapiCmdEnd(srapi_cmd_buffer_t *cmd);
int	srapiCmdClearColor(srapi_cmd_buffer_t *cmd, uint32_t color);
int	srapiCmdClearRect(srapi_cmd_buffer_t *cmd, uint32_t x, uint32_t y,
	    uint32_t width, uint32_t height, uint32_t color);
int	srapiCmdBlitSurface(srapi_cmd_buffer_t *cmd,
	    srapi_surface_t *surface, const struct srapi_region *src,
	    const struct srapi_region *dst, uint32_t flags);
int	srapiCmdBindPipeline(srapi_cmd_buffer_t *cmd,
	    srapi_pipeline_t *pipeline);
int	srapiCmdBindVertexBuffer(srapi_cmd_buffer_t *cmd,
	    srapi_buffer_t *buffer);
int	srapiCmdPushConstants(srapi_cmd_buffer_t *cmd, uint32_t first,
	    const int32_t *values, uint32_t count);
int	srapiCmdSetViewport(srapi_cmd_buffer_t *cmd,
	    const struct srapi_viewport *viewport);
int	srapiCmdDraw(srapi_cmd_buffer_t *cmd, uint32_t first_vertex,
	    uint32_t vertex_count);
int	srapiCmdPresent(srapi_cmd_buffer_t *cmd);
int	srapiSubmit(srapi_cmd_buffer_t *cmd);

int	srapiPollInput(srapi_device_t *device,
	    struct srapi_input_event *events, uint32_t max_events);
int	srapiFlushInput(srapi_device_t *device);
int	srapiGetInputState(srapi_device_t *device,
	    struct srapi_input_state *out);
int	srapiInputKeyDown(const struct srapi_input_state *state, uint32_t key);

#endif
