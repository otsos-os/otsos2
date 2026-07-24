/* !DEFINES!

$define %type demo_vertex as fixed point colored 2D vertex
$define %func sleep_ms as procedure with args int
$define %func now_ms as function with args void
$define %func color_cycle as function with args frame, phase
$define %func input_color as function with args old color, input event
$define %func update_input_color as function with args device, old color
$define %func write_vertices as procedure with args vertex array, frame
$define %func create_demo_pipeline as function with args device, outputs
$define %func add_clear_rect as function with args cmd, bounds, rect
$define %func clear_demo_regions as function with args cmd, frame, bounds
$define %func render_frame as function with args command state, frame
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal sleep_ms, now_ms, color_cycle, input_color
$space %internal update_input_color, write_vertices, create_demo_pipeline
$space %internal add_clear_rect, clear_demo_regions, render_frame
$space %export main

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <native.h>
#include <srapi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEMO_VERTEX_COUNT	9
#define DEMO_FRAME_MS		50
#define DEMO_DURATION_MS	10000
#define DEMO_INPUT_BATCH	16
#define FX(n, d)		SRAPI_FIXED_FROM_RATIO(n, d)

struct demo_vertex {
	int32_t		x;
	int32_t		y;
	uint32_t	color;
};

static const struct srapi_vm_inst demo_vs[] = {
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_X, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_X, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_Y, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_Y, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_R, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_R, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_G, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_G, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_B, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_B, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_A, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_A, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_END, 0, 0, 0, 0)
};

static const struct srapi_vm_inst demo_fs[] = {
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_R, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_R, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_G, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_G, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_B, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_B, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_LOAD_IN, 0, SRAPI_IO_A, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_STORE_OUT, SRAPI_IO_A, 0, 0, 0),
	SRAPI_VM_INST(SRAPI_VM_END, 0, 0, 0, 0)
};

static void
sleep_ms(int ms)
{
	struct kevent	change;
	struct kevent	event;
	int		kq;

	kq = eventKqueue();
	if (kq < 0) {
		return;
	}
	memset(&change, 0, sizeof(change));
	change.ident = 1;
	change.filter = EVFILT_TIMER;
	change.flags = EV_ADD | EV_ONESHOT;
	change.data = ms;
	(void)eventWait(kq, &change, 1, &event, 1, ms + 50);
	eventClose(kq);
}

static uint64_t
now_ms(void)
{
	struct api_timeinfo	ti;

	if (sysTimeInfo(&ti) != 0) {
		return (0);
	}
	return (ti.uptime_sec * 1000ULL + ti.uptime_nsec / 1000000ULL);
}

static uint32_t
color_cycle(uint32_t frame, uint32_t phase)
{
	uint32_t	t, r, g, b;

	t = (frame * 13 + phase) % 768;
	r = 0;
	g = 0;
	b = 0;
	if (t < 256) {
		r = 255 - t;
		g = t;
	} else if (t < 512) {
		g = 511 - t;
		b = t - 256;
	} else {
		b = 767 - t;
		r = t - 512;
	}
	return (0xff000000U | (r << 16) | (g << 8) | b);
}

static uint32_t
input_color(uint32_t old_color, const struct srapi_input_event *event)
{
	uint32_t	seed, r, g, b;

	seed = old_color ^ (uint32_t)event->seq;
	seed ^= (uint32_t)event->timestamp;
	seed ^= (uint32_t)event->x * 1103515245U;
	seed ^= (uint32_t)event->y * 12345U;
	seed ^= (uint32_t)event->dx * 2654435761U;
	seed ^= (uint32_t)event->dy * 2246822519U;
	seed ^= (uint32_t)event->dz * 3266489917U;
	seed ^= event->buttons * 668265263U;
	seed ^= event->key * 374761393U;
	seed ^= event->mods * 1597334677U;
	seed ^= event->ch * 3812015801U;
	seed ^= seed >> 16;
	seed *= 2246822519U;
	seed ^= seed >> 13;
	seed *= 3266489917U;
	seed ^= seed >> 16;
	r = 64 + (seed & 0xbf);
	g = 64 + ((seed >> 8) & 0xbf);
	b = 64 + ((seed >> 16) & 0xbf);
	return (0xff000000U | (r << 16) | (g << 8) | b);
}

static uint32_t
update_input_color(srapi_device_t *device, uint32_t old_color)
{
	struct srapi_input_event	events[DEMO_INPUT_BATCH];
	uint32_t		color;
	int			n, i;

	color = old_color;
	n = srapiPollInput(device, events, DEMO_INPUT_BATCH);
	if (n <= 0) {
		return (color);
	}
	for (i = 0; i < n; i++) {
		color = input_color(color, &events[i]);
	}
	return (color);
}

static void
write_vertices(struct demo_vertex *v, uint32_t frame, uint32_t square_color)
{
	uint32_t	t0, t1, t2;

	t0 = color_cycle(frame, 128);
	t1 = color_cycle(frame, 512);
	t2 = color_cycle(frame, 640);

	v[0].x = FX(-4, 5);
	v[0].y = FX(-11, 20);
	v[0].color = square_color;
	v[1].x = FX(-1, 5);
	v[1].y = FX(-11, 20);
	v[1].color = square_color;
	v[2].x = FX(-1, 5);
	v[2].y = FX(9, 20);
	v[2].color = square_color;
	v[3].x = FX(-4, 5);
	v[3].y = FX(-11, 20);
	v[3].color = square_color;
	v[4].x = FX(-1, 5);
	v[4].y = FX(9, 20);
	v[4].color = square_color;
	v[5].x = FX(-4, 5);
	v[5].y = FX(9, 20);
	v[5].color = square_color;

	v[6].x = FX(1, 4);
	v[6].y = FX(-11, 20);
	v[6].color = t1;
	v[7].x = FX(17, 20);
	v[7].y = FX(-11, 20);
	v[7].color = t2;
	v[8].x = FX(11, 20);
	v[8].y = FX(1, 2);
	v[8].color = t0;
}

static int
create_demo_pipeline(srapi_device_t *device, srapi_shader_t **vs,
    srapi_shader_t **fs, srapi_pipeline_t **pipeline)
{
	struct srapi_pipeline_desc	pdesc;
	struct srapi_shader_desc	sdesc;
	int				ret;

	memset(&sdesc, 0, sizeof(sdesc));
	sdesc.stage = SRAPI_SHADER_VERTEX;
	sdesc.code = demo_vs;
	sdesc.code_count = sizeof(demo_vs) / sizeof(demo_vs[0]);
	ret = srapiCreateShader(device, &sdesc, vs);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	memset(&sdesc, 0, sizeof(sdesc));
	sdesc.stage = SRAPI_SHADER_FRAGMENT;
	sdesc.code = demo_fs;
	sdesc.code_count = sizeof(demo_fs) / sizeof(demo_fs[0]);
	ret = srapiCreateShader(device, &sdesc, fs);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = srapiComputeShader(*vs);
	if (ret != SRAPI_OK && ret != SRAPI_ERR_UNSUPPORTED) {
		return (ret);
	}
	ret = srapiComputeShader(*fs);
	if (ret != SRAPI_OK && ret != SRAPI_ERR_UNSUPPORTED) {
		return (ret);
	}

	memset(&pdesc, 0, sizeof(pdesc));
	pdesc.vertex_shader = *vs;
	pdesc.fragment_shader = *fs;
	pdesc.vertex_layout.stride = sizeof(struct demo_vertex);
	pdesc.vertex_layout.attr_count = 2;
	pdesc.vertex_layout.attrs[0].location = SRAPI_VERTEX_LOCATION_POSITION;
	pdesc.vertex_layout.attrs[0].format = SRAPI_VERTEX_FORMAT_FIXED2;
	pdesc.vertex_layout.attrs[0].offset = offsetof(struct demo_vertex, x);
	pdesc.vertex_layout.attrs[1].location = SRAPI_VERTEX_LOCATION_COLOR;
	pdesc.vertex_layout.attrs[1].format = SRAPI_VERTEX_FORMAT_UNORM8_4;
	pdesc.vertex_layout.attrs[1].offset =
	    offsetof(struct demo_vertex, color);
	return (srapiCreatePipeline(device, &pdesc, pipeline));
}

static int
add_clear_rect(srapi_cmd_buffer_t *cmd, uint32_t screen_w, uint32_t screen_h,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
	if (x >= screen_w || y >= screen_h || width == 0 || height == 0) {
		return (SRAPI_OK);
	}
	if (width > screen_w - x) {
		width = screen_w - x;
	}
	if (height > screen_h - y) {
		height = screen_h - y;
	}
	return (srapiCmdClearRect(cmd, x, y, width, height, color));
}

static int
clear_demo_regions(srapi_cmd_buffer_t *cmd, uint32_t frame,
    uint32_t screen_w, uint32_t screen_h)
{
	uint32_t	bg;
	uint32_t	x, y, w, h;
	int	ret;

	bg = 0xff101018U;
	if (frame == 0 || screen_w < 16 || screen_h < 16) {
		return (srapiCmdClearColor(cmd, bg));
	}
	x = screen_w / 10;
	y = screen_h / 4;
	if (x > 4) {
		x -= 4;
	}
	if (y > 4) {
		y -= 4;
	}
	w = screen_w / 3 + 8;
	h = screen_h * 56 / 100 + 8;
	ret = add_clear_rect(cmd, screen_w, screen_h, x, y, w, h, bg);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	x = screen_w * 3 / 5;
	y = screen_h / 4;
	if (x > 4) {
		x -= 4;
	}
	if (y > 4) {
		y -= 4;
	}
	w = screen_w * 37 / 100 + 8;
	h = screen_h * 56 / 100 + 8;
	return (add_clear_rect(cmd, screen_w, screen_h, x, y, w, h, bg));
}

static int
render_frame(srapi_cmd_buffer_t *cmd, srapi_pipeline_t *pipeline,
    srapi_buffer_t *vertices, uint32_t frame, uint32_t screen_w,
    uint32_t screen_h)
{
	int	ret;

	ret = srapiCmdBegin(cmd);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = clear_demo_regions(cmd, frame, screen_w, screen_h);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = srapiCmdBindPipeline(cmd, pipeline);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = srapiCmdBindVertexBuffer(cmd, vertices);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = srapiCmdDraw(cmd, 0, DEMO_VERTEX_COUNT);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = srapiCmdPresent(cmd);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = srapiCmdEnd(cmd);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	(void)frame;
	return (srapiSubmit(cmd));
}

int
main(int argc, char **argv, char **envp)
{
	struct srapi_instance_desc	idesc;
	struct srapi_device_desc		ddesc;
	struct srapi_device_info		info;
	struct srapi_buffer_desc		bdesc;
	struct demo_vertex		verts[DEMO_VERTEX_COUNT];
	srapi_instance_t		*instance;
	srapi_device_t			*device;
	srapi_shader_t			*vs, *fs;
	srapi_pipeline_t		*pipeline;
	srapi_buffer_t			*vertex_buffer;
	srapi_cmd_buffer_t		*cmd;
	uint64_t			start, now;
	uint32_t			frame, square_color;
	int				ret;

	(void)argc;
	(void)argv;
	(void)envp;

	(void)personality(API_PERSONALITY_NATIVE);
	memset(&idesc, 0, sizeof(idesc));
	memset(&ddesc, 0, sizeof(ddesc));
	memset(&bdesc, 0, sizeof(bdesc));
	memset(&info, 0, sizeof(info));
	instance = NULL;
	device = NULL;
	vs = NULL;
	fs = NULL;
	pipeline = NULL;
	vertex_buffer = NULL;
	cmd = NULL;

	ret = srapiCreateInstance(&idesc, &instance);
	if (ret != SRAPI_OK) {
		printf("srapi_demo: instance failed %d\n", ret);
		return (1);
	}
	ret = srapiCreateDevice(instance, &ddesc, &device);
	if (ret != SRAPI_OK) {
		printf("srapi_demo: device failed %d\n", ret);
		return (1);
	}
	ret = srapiDeviceInfo(device, &info);
	if (ret == SRAPI_OK) {
		printf("srapi_demo: %ux%u %ubpp driver=%s\n", info.width,
		    info.height, info.bpp, info.driver_name);
	}
	ret = create_demo_pipeline(device, &vs, &fs, &pipeline);
	if (ret != SRAPI_OK) {
		printf("srapi_demo: pipeline failed %d\n", ret);
		return (1);
	}
	bdesc.size = sizeof(verts);
	bdesc.usage = SRAPI_BUFFER_VERTEX;
	ret = srapiCreateBuffer(device, &bdesc, &vertex_buffer);
	if (ret != SRAPI_OK) {
		printf("srapi_demo: vertex buffer failed %d\n", ret);
		return (1);
	}
	ret = srapiCreateCommandBuffer(device, &cmd);
	if (ret != SRAPI_OK) {
		printf("srapi_demo: cmd failed %d\n", ret);
		return (1);
	}

	start = now_ms();
	frame = 0;
	square_color = 0xff3880ffU;
	for (;;) {
		square_color = update_input_color(device, square_color);
		write_vertices(verts, frame, square_color);
		ret = srapiBufferWrite(vertex_buffer, 0, verts, sizeof(verts));
		if (ret != SRAPI_OK) {
			printf("srapi_demo: upload failed %d\n", ret);
			break;
		}
		ret = render_frame(cmd, pipeline, vertex_buffer, frame,
		    info.width, info.height);
		if (ret != SRAPI_OK) {
			printf("srapi_demo: render failed %d\n", ret);
			break;
		}
		sleep_ms(DEMO_FRAME_MS);
		frame++;
		now = now_ms();
		if ((start != 0 && now >= start + DEMO_DURATION_MS) ||
		    (start == 0 && frame >= DEMO_DURATION_MS / DEMO_FRAME_MS)) {
			break;
		}
	}

	srapiDestroyCommandBuffer(cmd);
	srapiDestroyBuffer(vertex_buffer);
	srapiDestroyPipeline(pipeline);
	srapiDestroyShader(fs);
	srapiDestroyShader(vs);
	srapiDestroyDevice(device);
	srapiDestroyInstance(instance);
	printf("srapi_demo: done\n");
	return (0);
}
