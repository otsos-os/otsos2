/* !DEFINES!

$define %type demo_vertex as fixed point colored 2D vertex
$define %type demo_terminal_guard as saved terminal power state
$define %func sleep_ms as procedure with args int
$define %func now_ms as function with args void
$define %func demo_terminal_suspend as procedure with args guard
$define %func demo_terminal_restore as procedure with args guard
$define %func color_cycle as function with args frame, phase
$define %func input_color as function with args old color, input event
$define %func update_input_color as function with args device, old color
$define %func write_vertices as procedure with args vertex array, frame
$define %func create_demo_pipeline as function with args device, outputs
$define %func add_clear_rect as function with args cmd, bounds, rect
$define %func clear_demo_regions as function with args cmd, frame, bounds
$define %func render_frame as function with args command state, frame
$define %func read_demo_mode as function with args void
$define %func run_srapi_demo as function with args void
$define %func draw_libg_cursor as procedure with args libg context
$define %func draw_libg_demo as function with args libg context, state
$define %func run_libg_demo as function with args void
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal sleep_ms, now_ms, color_cycle, input_color
$space %internal demo_terminal_suspend, demo_terminal_restore
$space %internal update_input_color, write_vertices, create_demo_pipeline
$space %internal add_clear_rect, clear_demo_regions, render_frame
$space %internal read_demo_mode, run_srapi_demo, draw_libg_cursor
$space %internal draw_libg_demo
$space %internal run_libg_demo
$space %export main

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <native.h>
#include <libg.h>
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
#define DEMO_MODE_SRAPI		1
#define DEMO_MODE_LIBG		2

struct demo_vertex {
	int32_t		x;
	int32_t		y;
	uint32_t	color;
};

struct demo_terminal_guard {
	int	tty;
	int	state;
	int	suspended;
};

struct libg_demo_state {
	char		text[64];
	int32_t		slider;
	uint32_t	clicks;
	uint32_t	frame;
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

static void
demo_terminal_suspend(struct demo_terminal_guard *guard)
{
	struct api_term_info	info;
	struct api_term_power	power;

	if (!guard) {
		return;
	}

	memset(guard, 0, sizeof(*guard));
	guard->tty = -1;
	if (termInfo(&info) != 0) {
		return;
	}

	guard->tty = info.tty;
	guard->state = info.state;
	if (info.state != TERM_STATE_ACTIVE) {
		return;
	}

	memset(&power, 0, sizeof(power));
	power.op = API_TERM_POWER_CHANGE;
	power.tty = info.tty;
	power.state = TERM_STATE_SUSPENDED;
	if (termPower(&power) == 0) {
		guard->suspended = 1;
	}
}

static void
demo_terminal_restore(struct demo_terminal_guard *guard)
{
	struct api_term_power	power;

	if (!guard || !guard->suspended) {
		return;
	}

	memset(&power, 0, sizeof(power));
	power.op = API_TERM_POWER_CHANGE;
	power.tty = guard->tty;
	power.state = guard->state;
	(void)termPower(&power);
	guard->suspended = 0;
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

static int
run_srapi_demo(void)
{
	struct srapi_instance_desc	idesc;
	struct srapi_device_desc		ddesc;
	struct srapi_device_info		info;
	struct srapi_buffer_desc		bdesc;
	struct demo_terminal_guard	term_guard;
	struct demo_vertex		verts[DEMO_VERTEX_COUNT];
	srapi_instance_t		*instance;
	srapi_device_t			*device;
	srapi_shader_t			*vs, *fs;
	srapi_pipeline_t		*pipeline;
	srapi_buffer_t			*vertex_buffer;
	srapi_cmd_buffer_t		*cmd;
	const char			*failure;
	uint64_t			start, now;
	uint32_t			frame, square_color;
	int				ret;

	memset(&idesc, 0, sizeof(idesc));
	memset(&ddesc, 0, sizeof(ddesc));
	memset(&bdesc, 0, sizeof(bdesc));
	memset(&info, 0, sizeof(info));
	memset(&term_guard, 0, sizeof(term_guard));
	term_guard.tty = -1;
	instance = NULL;
	device = NULL;
	vs = NULL;
	fs = NULL;
	pipeline = NULL;
	vertex_buffer = NULL;
	cmd = NULL;
	failure = NULL;

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

	demo_terminal_suspend(&term_guard);
	start = now_ms();
	frame = 0;
	square_color = 0xff3880ffU;
	for (;;) {
		square_color = update_input_color(device, square_color);
		write_vertices(verts, frame, square_color);
		ret = srapiBufferWrite(vertex_buffer, 0, verts, sizeof(verts));
		if (ret != SRAPI_OK) {
			failure = "upload";
			break;
		}
		ret = render_frame(cmd, pipeline, vertex_buffer, frame,
		    info.width, info.height);
		if (ret != SRAPI_OK) {
			failure = "render";
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

	demo_terminal_restore(&term_guard);
	srapiDestroyCommandBuffer(cmd);
	srapiDestroyBuffer(vertex_buffer);
	srapiDestroyPipeline(pipeline);
	srapiDestroyShader(fs);
	srapiDestroyShader(vs);
	srapiDestroyDevice(device);
	srapiDestroyInstance(instance);
	if (failure) {
		printf("srapi_demo: %s failed %d\n", failure, ret);
		return (1);
	}
	printf("srapi_demo: done\n");
	return (0);
}

static int
read_demo_mode(void)
{
	ssize_t	n;
	char	ch;

	termPrint("srapi_demo\n");
	termPrint("  1 - SRAPI render test\n");
	termPrint("  2 - LibG widget test\n");
	termPrint("select: ");
	for (;;) {
		n = termReadFlags(&ch, 1, TERM_READ_IGNORE_SIGINT);
		if (n <= 0) {
			termPrint("\n");
			return (DEMO_MODE_SRAPI);
		}
		if (ch == '1') {
			termPrint("1\n");
			return (DEMO_MODE_SRAPI);
		}
		if (ch == '2') {
			termPrint("2\n");
			return (DEMO_MODE_LIBG);
		}
	}
}

static void
draw_libg_cursor(libg_context_t *ui)
{
	int32_t		x, y;
	uint32_t	buttons, color, shadow;

	libgMousePosition(ui, &x, &y, &buttons);
	shadow = 0xff050607U;
	color = (buttons & SRAPI_MOUSE_LEFT) != 0 ? 0xff23a6d5U :
	    0xfff2f5f8U;

	libgLine(ui, x - 8, y + 1, x - 2, y + 1, shadow);
	libgLine(ui, x + 2, y + 1, x + 8, y + 1, shadow);
	libgLine(ui, x + 1, y - 8, x + 1, y - 2, shadow);
	libgLine(ui, x + 1, y + 2, x + 1, y + 8, shadow);
	libgStrokeCircle(ui, x + 1, y + 1, 4, shadow);

	libgLine(ui, x - 8, y, x - 2, y, color);
	libgLine(ui, x + 2, y, x + 8, y, color);
	libgLine(ui, x, y - 8, x, y - 2, color);
	libgLine(ui, x, y + 2, x, y + 8, color);
	libgFillCircle(ui, x, y, 2, color);
}

static int
draw_libg_demo(libg_context_t *ui, struct libg_demo_state *state)
{
	libg_rect_t	panel, rect;
	uint32_t	width, height, button_state, field_state, slider_state;
	int32_t		margin, x, y, right_x, shape_y;
	char		line[80];
	int		close_clicked;

	width = libgWidth(ui);
	height = libgHeight(ui);
	margin = width >= 640 ? 28 : 14;
	close_clicked = 0;

	panel.x = margin;
	panel.y = margin;
	panel.width = (int32_t)width - margin * 2;
	panel.height = (int32_t)height - margin * 2;
	if (panel.width < 260) {
		panel.width = (int32_t)width;
		panel.x = 0;
	}
	if (panel.height < 220) {
		panel.height = (int32_t)height;
		panel.y = 0;
	}

	libgPanel(ui, panel);
	rect.x = panel.x + panel.width - 42;
	rect.y = panel.y + 10;
	rect.width = 32;
	rect.height = 28;
	if (libgButton(ui, 100, rect, "X") & LIBG_WIDGET_CLICKED) {
		close_clicked = 1;
	}

	x = panel.x + 20;
	y = panel.y + 18;
	libgTextScale(ui, x, y, "TEST", 0xfff2f5f8U, 2);
	y += 34;
	libgText(ui, x, y, "GUI ", 0xffa7b0bbU);
	y += 28;

	rect.x = x;
	rect.y = y;
	rect.width = 150;
	rect.height = 42;
	button_state = libgButton(ui, 1, rect, "BUTTON");
	if (button_state & LIBG_WIDGET_CLICKED) {
		state->clicks++;
	}
	snprintf(line, sizeof(line), "CLICKS %u", state->clicks);
	libgTextScale(ui, x + 170, y + 12, line, 0xfff2f5f8U, 2);
	y += 60;

	libgText(ui, x, y, "TEXT FIELD", 0xffa7b0bbU);
	y += 14;
	rect.x = x;
	rect.y = y;
	rect.width = panel.width > 390 ? 320 : panel.width - 40;
	rect.height = 36;
	field_state = libgTextField(ui, 2, rect, state->text,
	    sizeof(state->text));
	y += 52;

	snprintf(line, sizeof(line), "SLIDER %d", state->slider);
	libgText(ui, x, y, line, 0xffa7b0bbU);
	y += 14;
	rect.x = x;
	rect.y = y;
	rect.width = panel.width > 390 ? 320 : panel.width - 40;
	rect.height = 32;
	slider_state = libgSlider(ui, 3, rect, 0, 100, &state->slider);
	y += 54;

	snprintf(line, sizeof(line), "TEXT: %s", state->text);
	libgText(ui, x, y, line, 0xfff2f5f8U);
	if (field_state & LIBG_WIDGET_SUBMIT) {
		libgText(ui, x, y + 14, "ENTER PRESSED", 0xff23a6d5U);
	} else if (slider_state & LIBG_WIDGET_CHANGED) {
		libgText(ui, x, y + 14, "SLIDER CHANGED", 0xff23a6d5U);
	} else {
		libgText(ui, x, y + 14, "CLICK FIELD TO TYPE", 0xffa7b0bbU);
	}

	right_x = panel.x + panel.width - 190;
	if (right_x < x + 350) {
		right_x = x;
		shape_y = y + 46;
	} else {
		shape_y = panel.y + 92;
	}

	libgText(ui, right_x, shape_y - 22, "PRIMITIVES", 0xffa7b0bbU);
	rect.x = right_x;
	rect.y = shape_y;
	rect.width = 74;
	rect.height = 74;
	libgFillRect(ui, rect, 0xfff2c14eU);
	libgStrokeRect(ui, rect, 0xff2a3038U);
	libgFillCircle(ui, right_x + 130, shape_y + 37, 38, 0xff75d97cU);
	libgStrokeCircle(ui, right_x + 130, shape_y + 37, 38, 0xff2a3038U);
	libgLine(ui, right_x, shape_y + 100, right_x + 168, shape_y + 132,
	    0xff23a6d5U);
	libgLine(ui, right_x, shape_y + 132, right_x + 168, shape_y + 100,
	    0xffe66a5cU);

	draw_libg_cursor(ui);
	state->frame++;
	return (close_clicked);
}

static int
run_libg_demo(void)
{
	struct srapi_instance_desc	idesc;
	struct srapi_device_desc		ddesc;
	struct srapi_device_info		info;
	struct demo_terminal_guard	term_guard;
	struct libg_demo_state		state;
	srapi_instance_t		*instance;
	srapi_device_t			*device;
	libg_context_t			*ui;
	libg_style_t			style;
	const char			*failure;
	int				ret, rc, close_requested;

	memset(&idesc, 0, sizeof(idesc));
	memset(&ddesc, 0, sizeof(ddesc));
	memset(&info, 0, sizeof(info));
	memset(&term_guard, 0, sizeof(term_guard));
	memset(&state, 0, sizeof(state));
	term_guard.tty = -1;
	instance = NULL;
	device = NULL;
	ui = NULL;
	failure = NULL;
	ret = 0;

	ret = srapiCreateInstance(&idesc, &instance);
	if (ret != SRAPI_OK) {
		printf("srapi_demo: instance failed %d\n", ret);
		return (1);
	}
	ret = srapiCreateDevice(instance, &ddesc, &device);
	if (ret != SRAPI_OK) {
		printf("srapi_demo: device failed %d\n", ret);
		srapiDestroyInstance(instance);
		return (1);
	}
	ret = srapiDeviceInfo(device, &info);
	if (ret == SRAPI_OK) {
		printf("srapi_demo: libg %ux%u %ubpp driver=%s\n",
		    info.width, info.height, info.bpp, info.driver_name);
	}

	libgDefaultStyle(&style);
	rc = libgCreate(device, &style, &ui);
	if (rc != LIBG_OK) {
		printf("srapi_demo: libg create failed %d\n", rc);
		srapiDestroyDevice(device);
		srapiDestroyInstance(instance);
		return (1);
	}

	strncpy(state.text, "hello libg", sizeof(state.text) - 1);
	state.slider = 50;
	(void)srapiFlushInput(device);

	demo_terminal_suspend(&term_guard);
	close_requested = 0;
	for (;;) {
		rc = libgBegin(ui, style.background);
		if (rc != LIBG_OK) {
			failure = "libg begin";
			break;
		}
		close_requested = draw_libg_demo(ui, &state);
		rc = libgPresent(ui);
		if (rc != LIBG_OK) {
			failure = "libg present";
			break;
		}
		if (close_requested) {
			break;
		}
		sleep_ms(DEMO_FRAME_MS);
	}

	libgDestroy(ui);
	srapiDestroyDevice(device);
	srapiDestroyInstance(instance);
	demo_terminal_restore(&term_guard);
	if (failure) {
		printf("srapi_demo: %s failed %d\n", failure, rc);
		return (1);
	}
	printf("srapi_demo: libg done\n");
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	int	mode;

	(void)argc;
	(void)argv;
	(void)envp;

	(void)personality(API_PERSONALITY_NATIVE);
	mode = read_demo_mode();
	if (mode == DEMO_MODE_LIBG) {
		return (run_libg_demo());
	}
	return (run_srapi_demo());
}
