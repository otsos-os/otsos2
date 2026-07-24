/* !DEFINES!

$define %type srapi_rect as dirty rectangle
$define %type srapi_vertex_out as post vertex shader data
$define %type srapi_cmd_record as recorded command
$define %type srapi_surface as CPU pixel surface
$define %type srapi_shader_cpu_fn as compiled shader entry point
$define %func srapi_surface_bytes_per_pixel as function with args format
$define %func srapi_input_state_reset as procedure with args state
$define %func srapi_input_state_begin_poll as procedure with args state
$define %func srapi_input_state_apply as procedure with args state, event
$define %func srapi_input_shutdown as procedure with args device
$define %func srapi_image_mark_dirty as procedure with args image, rect
$define %func srapi_backend_present as function with args device, image
$define %func srapi_backend_unbind as function with args device, image
$define %func srapiComputeShader as function with args shader
$define %func srapi_vm_run as function with args shader, input, push, output
$define %func srapi_raster_clear as function with args image, color
$define %func srapi_raster_clear_rect as function with args image, rect, color
$define %func srapi_raster_blit_surface as function with args image, surface, regions
$define %func srapi_raster_draw as function with args draw state

*/

/* !SPACE!

$space %internal srapi_rect, srapi_vertex_out, srapi_cmd_record
$space %internal srapi_shader_cpu_fn
$space %internal srapi_surface_bytes_per_pixel
$space %internal srapi_input_state_reset, srapi_input_state_begin_poll
$space %internal srapi_input_state_apply, srapi_input_shutdown
$space %internal srapi_image_mark_dirty, srapi_backend_present
$space %internal srapi_backend_unbind, srapiComputeShader, srapi_vm_run
$space %internal srapi_raster_clear, srapi_raster_clear_rect
$space %internal srapi_raster_blit_surface, srapi_raster_draw

*/

/*
 * Copyright (c) 2026, otsos team
 */

#ifndef SRAPI_PRIVATE_H
#define SRAPI_PRIVATE_H

#include <native.h>
#include <srapi.h>
#include <stddef.h>
#include <stdint.h>

#define SRAPI_CMD_INITIAL_CAP	32
#define SRAPI_SHADER_CPU_COMPILED 0x00000001

typedef int (*srapi_shader_cpu_fn)(const int32_t *input,
    const int32_t *push, int32_t *output);

enum srapi_cmd_type {
	SRAPI_CMD_CLEAR = 1,
	SRAPI_CMD_BIND_PIPELINE = 2,
	SRAPI_CMD_BIND_VERTEX_BUFFER = 3,
	SRAPI_CMD_PUSH_CONSTANTS = 4,
	SRAPI_CMD_SET_VIEWPORT = 5,
	SRAPI_CMD_DRAW = 6,
	SRAPI_CMD_PRESENT = 7,
	SRAPI_CMD_BLIT_SURFACE = 8
};

struct srapi_rect {
	uint32_t	x;
	uint32_t	y;
	uint32_t	width;
	uint32_t	height;
};

struct srapi_instance {
	uint32_t	flags;
};

struct srapi_image {
	srapi_device_t	*device;
	void		*pixels;
	size_t		size;
	uint32_t	gem;
	uint32_t	fb;
	uint32_t	width;
	uint32_t	height;
	uint32_t	pitch;
	uint32_t	bpp;
	struct srapi_rect dirty;
	int		dirty_valid;
};

struct srapi_device {
	srapi_instance_t		*instance;
	struct api_drm_info		info;
	struct api_drm_objects		objects;
	struct srapi_image		backbuffer;
	struct srapi_input_state	input_state;
	uint32_t			flags;
	int				input_kq;
	int				input_ready;
};

struct srapi_buffer {
	srapi_device_t	*device;
	void		*data;
	size_t		size;
	uint32_t	usage;
};

struct srapi_surface {
	srapi_device_t	*device;
	void		*pixels;
	size_t		size;
	uint32_t	palette[256];
	uint32_t	width;
	uint32_t	height;
	uint32_t	pitch;
	uint32_t	format;
	uint32_t	flags;
};

struct srapi_shader {
	srapi_device_t			*device;
	struct srapi_vm_inst		*code;
	void				*cpu_code;
	srapi_shader_cpu_fn		cpu_entry;
	size_t				cpu_code_size;
	uint32_t			stage;
	uint32_t			code_count;
	uint32_t			cpu_flags;
};

struct srapi_pipeline {
	srapi_device_t			*device;
	srapi_shader_t			*vertex_shader;
	srapi_shader_t			*fragment_shader;
	struct srapi_vertex_layout	vertex_layout;
	struct srapi_viewport		viewport;
	int32_t				push[SRAPI_MAX_PUSH_CONSTANTS];
	uint32_t			flags;
};

struct srapi_cmd_record {
	uint32_t	type;
	union {
		struct {
			uint32_t	color;
			uint32_t	x;
			uint32_t	y;
			uint32_t	width;
			uint32_t	height;
			int		rect_valid;
		} clear;
		struct {
			srapi_pipeline_t *pipeline;
		} bind_pipeline;
		struct {
			srapi_buffer_t *buffer;
		} bind_vertex_buffer;
		struct {
			int32_t		values[SRAPI_MAX_PUSH_CONSTANTS];
			uint32_t	first;
			uint32_t	count;
		} push;
		struct {
			struct srapi_viewport viewport;
		} viewport;
		struct {
			uint32_t	first_vertex;
			uint32_t	vertex_count;
		} draw;
		struct {
			srapi_surface_t		*surface;
			struct srapi_region	src;
			struct srapi_region	dst;
			uint32_t		flags;
			int			src_valid;
			int			dst_valid;
		} blit;
	} u;
};

struct srapi_cmd_buffer {
	srapi_device_t			*device;
	struct srapi_cmd_record		*records;
	uint32_t			count;
	uint32_t			capacity;
	int				recording;
	int				ended;
};

struct srapi_vertex_out {
	int32_t		io[SRAPI_VM_IO_SLOTS];
	int32_t		sx;
	int32_t		sy;
};

void	srapi_image_mark_dirty(srapi_image_t *image, uint32_t x,
	    uint32_t y, uint32_t width, uint32_t height);
uint32_t	srapi_surface_bytes_per_pixel(uint32_t format);
void	srapi_input_state_reset(struct srapi_input_state *state);
void	srapi_input_state_begin_poll(struct srapi_input_state *state);
void	srapi_input_state_apply(struct srapi_input_state *state,
	    const struct srapi_input_event *event);
void	srapi_input_shutdown(srapi_device_t *device);
int	srapi_backend_present(srapi_device_t *device, srapi_image_t *image);
int	srapi_backend_unbind(srapi_device_t *device, srapi_image_t *image);
int	srapiComputeShader(srapi_shader_t *shader);
int	srapi_vm_run(const srapi_shader_t *shader, const int32_t *input,
	    const int32_t *push, int32_t *output);
int	srapi_raster_clear(srapi_image_t *image, uint32_t color);
int	srapi_raster_clear_rect(srapi_image_t *image, uint32_t x, uint32_t y,
	    uint32_t width, uint32_t height, uint32_t color);
int	srapi_raster_blit_surface(srapi_image_t *image,
	    srapi_surface_t *surface, const struct srapi_region *src_region,
	    const struct srapi_region *dst_region, uint32_t flags);
int	srapi_raster_draw(srapi_device_t *device, srapi_image_t *image,
	    srapi_pipeline_t *pipeline, const struct srapi_viewport *viewport,
	    srapi_buffer_t *vertex_buffer, uint32_t first_vertex,
	    uint32_t vertex_count);

#endif
