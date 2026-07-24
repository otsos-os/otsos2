/* !DEFINES!

$define %type srapi_cmd_buffer as command recording buffer
$define %func srapiCmdBegin as function with args command buffer
$define %func srapiCmdClearRect as function with args command buffer, rect, color
$define %func srapiCmdBlitSurface as function with args command buffer, surface, regions
$define %func srapiCmdDraw as function with args command buffer, draw range
$define %func srapiSubmit as function with args command buffer

*/

/* !SPACE!

$space %internal cmd_reserve, cmd_append
$space %export srapiCmdReset, srapiCmdBegin, srapiCmdEnd
$space %export srapiCmdClearColor, srapiCmdClearRect
$space %export srapiCmdBlitSurface, srapiCmdBindPipeline
$space %export srapiCmdBindVertexBuffer
$space %export srapiCmdPushConstants, srapiCmdSetViewport, srapiCmdDraw
$space %export srapiCmdPresent, srapiSubmit

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <srapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "srapi_private.h"

static int
cmd_reserve(srapi_cmd_buffer_t *cmd, uint32_t need)
{
	struct srapi_cmd_record	*next;
	uint32_t		capacity;

	if (need <= cmd->capacity) {
		return (SRAPI_OK);
	}
	capacity = cmd->capacity == 0 ? SRAPI_CMD_INITIAL_CAP : cmd->capacity;
	while (capacity < need) {
		capacity *= 2;
	}
	next = realloc(cmd->records, sizeof(*cmd->records) * capacity);
	if (!next) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	cmd->records = next;
	cmd->capacity = capacity;
	return (SRAPI_OK);
}

static int
cmd_append(srapi_cmd_buffer_t *cmd, struct srapi_cmd_record **out)
{
	int	ret;

	if (!cmd || !cmd->recording || cmd->ended) {
		return (SRAPI_ERR_STATE);
	}
	ret = cmd_reserve(cmd, cmd->count + 1);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	*out = &cmd->records[cmd->count++];
	memset(*out, 0, sizeof(**out));
	return (SRAPI_OK);
}

int
srapiCmdReset(srapi_cmd_buffer_t *cmd)
{
	if (!cmd) {
		return (SRAPI_ERR_INVALID);
	}
	cmd->count = 0;
	cmd->recording = 0;
	cmd->ended = 0;
	return (SRAPI_OK);
}

int
srapiCmdBegin(srapi_cmd_buffer_t *cmd)
{
	if (!cmd) {
		return (SRAPI_ERR_INVALID);
	}
	cmd->count = 0;
	cmd->recording = 1;
	cmd->ended = 0;
	return (SRAPI_OK);
}

int
srapiCmdEnd(srapi_cmd_buffer_t *cmd)
{
	if (!cmd || !cmd->recording) {
		return (SRAPI_ERR_STATE);
	}
	cmd->recording = 0;
	cmd->ended = 1;
	return (SRAPI_OK);
}

int
srapiCmdClearColor(srapi_cmd_buffer_t *cmd, uint32_t color)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_CLEAR;
	rec->u.clear.color = color;
	rec->u.clear.rect_valid = 0;
	return (SRAPI_OK);
}

int
srapiCmdClearRect(srapi_cmd_buffer_t *cmd, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, uint32_t color)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	if (width == 0 || height == 0) {
		return (SRAPI_ERR_INVALID);
	}
	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_CLEAR;
	rec->u.clear.color = color;
	rec->u.clear.x = x;
	rec->u.clear.y = y;
	rec->u.clear.width = width;
	rec->u.clear.height = height;
	rec->u.clear.rect_valid = 1;
	return (SRAPI_OK);
}

int
srapiCmdBlitSurface(srapi_cmd_buffer_t *cmd, srapi_surface_t *surface,
    const struct srapi_region *src, const struct srapi_region *dst,
    uint32_t flags)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	if (!surface) {
		return (SRAPI_ERR_INVALID);
	}
	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_BLIT_SURFACE;
	rec->u.blit.surface = surface;
	rec->u.blit.flags = flags;
	rec->u.blit.src_valid = 0;
	rec->u.blit.dst_valid = 0;
	if (src) {
		rec->u.blit.src = *src;
		rec->u.blit.src_valid = 1;
	}
	if (dst) {
		rec->u.blit.dst = *dst;
		rec->u.blit.dst_valid = 1;
	}
	return (SRAPI_OK);
}

int
srapiCmdBindPipeline(srapi_cmd_buffer_t *cmd, srapi_pipeline_t *pipeline)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	if (!pipeline) {
		return (SRAPI_ERR_INVALID);
	}
	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_BIND_PIPELINE;
	rec->u.bind_pipeline.pipeline = pipeline;
	return (SRAPI_OK);
}

int
srapiCmdBindVertexBuffer(srapi_cmd_buffer_t *cmd, srapi_buffer_t *buffer)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	if (!buffer) {
		return (SRAPI_ERR_INVALID);
	}
	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_BIND_VERTEX_BUFFER;
	rec->u.bind_vertex_buffer.buffer = buffer;
	return (SRAPI_OK);
}

int
srapiCmdPushConstants(srapi_cmd_buffer_t *cmd, uint32_t first,
    const int32_t *values, uint32_t count)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	if (!values || count == 0 ||
	    first >= SRAPI_MAX_PUSH_CONSTANTS ||
	    count > SRAPI_MAX_PUSH_CONSTANTS - first) {
		return (SRAPI_ERR_INVALID);
	}
	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_PUSH_CONSTANTS;
	rec->u.push.first = first;
	rec->u.push.count = count;
	memcpy(rec->u.push.values, values, sizeof(int32_t) * count);
	return (SRAPI_OK);
}

int
srapiCmdSetViewport(srapi_cmd_buffer_t *cmd,
    const struct srapi_viewport *viewport)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	if (!viewport) {
		return (SRAPI_ERR_INVALID);
	}
	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_SET_VIEWPORT;
	rec->u.viewport.viewport = *viewport;
	return (SRAPI_OK);
}

int
srapiCmdDraw(srapi_cmd_buffer_t *cmd, uint32_t first_vertex,
    uint32_t vertex_count)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_DRAW;
	rec->u.draw.first_vertex = first_vertex;
	rec->u.draw.vertex_count = vertex_count;
	return (SRAPI_OK);
}

int
srapiCmdPresent(srapi_cmd_buffer_t *cmd)
{
	struct srapi_cmd_record	*rec;
	int			ret;

	ret = cmd_append(cmd, &rec);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	rec->type = SRAPI_CMD_PRESENT;
	return (SRAPI_OK);
}

int
srapiSubmit(srapi_cmd_buffer_t *cmd)
{
	struct srapi_viewport	viewport;
	srapi_pipeline_t	*pipeline;
	srapi_buffer_t		*vertex_buffer;
	uint32_t		i;
	int			ret;

	if (!cmd || !cmd->ended) {
		return (SRAPI_ERR_STATE);
	}
	pipeline = NULL;
	vertex_buffer = NULL;
	memset(&viewport, 0, sizeof(viewport));
	viewport.width = cmd->device->backbuffer.width;
	viewport.height = cmd->device->backbuffer.height;

	for (i = 0; i < cmd->count; i++) {
		switch (cmd->records[i].type) {
		case SRAPI_CMD_CLEAR:
			if (cmd->records[i].u.clear.rect_valid) {
				ret = srapi_raster_clear_rect(
				    &cmd->device->backbuffer,
				    cmd->records[i].u.clear.x,
				    cmd->records[i].u.clear.y,
				    cmd->records[i].u.clear.width,
				    cmd->records[i].u.clear.height,
				    cmd->records[i].u.clear.color);
			} else {
				ret = srapi_raster_clear(&cmd->device->backbuffer,
				    cmd->records[i].u.clear.color);
			}
			if (ret != SRAPI_OK) {
				return (ret);
			}
			break;
		case SRAPI_CMD_BLIT_SURFACE:
			ret = srapi_raster_blit_surface(
			    &cmd->device->backbuffer,
			    cmd->records[i].u.blit.surface,
			    cmd->records[i].u.blit.src_valid ?
			    &cmd->records[i].u.blit.src : NULL,
			    cmd->records[i].u.blit.dst_valid ?
			    &cmd->records[i].u.blit.dst : NULL,
			    cmd->records[i].u.blit.flags);
			if (ret != SRAPI_OK) {
				return (ret);
			}
			break;
		case SRAPI_CMD_BIND_PIPELINE:
			pipeline = cmd->records[i].u.bind_pipeline.pipeline;
			if (pipeline->viewport.width != 0 &&
			    pipeline->viewport.height != 0) {
				viewport = pipeline->viewport;
			}
			break;
		case SRAPI_CMD_BIND_VERTEX_BUFFER:
			vertex_buffer = cmd->records[i].u.bind_vertex_buffer.
			    buffer;
			break;
		case SRAPI_CMD_PUSH_CONSTANTS:
			if (!pipeline) {
				return (SRAPI_ERR_STATE);
			}
			memcpy(&pipeline->push[cmd->records[i].u.push.first],
			    cmd->records[i].u.push.values,
			    sizeof(int32_t) * cmd->records[i].u.push.count);
			break;
		case SRAPI_CMD_SET_VIEWPORT:
			viewport = cmd->records[i].u.viewport.viewport;
			break;
		case SRAPI_CMD_DRAW:
			if (!pipeline || !vertex_buffer) {
				return (SRAPI_ERR_STATE);
			}
			ret = srapi_raster_draw(cmd->device,
			    &cmd->device->backbuffer, pipeline, &viewport,
			    vertex_buffer, cmd->records[i].u.draw.first_vertex,
			    cmd->records[i].u.draw.vertex_count);
			if (ret != SRAPI_OK) {
				return (ret);
			}
			break;
		case SRAPI_CMD_PRESENT:
			ret = srapi_backend_present(cmd->device,
			    &cmd->device->backbuffer);
			if (ret != SRAPI_OK) {
				return (ret);
			}
			break;
		default:
			return (SRAPI_ERR_STATE);
		}
	}
	return (SRAPI_OK);
}
