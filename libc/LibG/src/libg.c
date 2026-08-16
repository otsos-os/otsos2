/* !DEFINES!

$define %type libg_context as LibG immediate UI context
$define %type libg_present_state as target damage callback state
$define %type libg_rect_t as integer rectangle
$define %type libg_style_t as UI color palette
$define %type uint32_t as 32 bit unsigned
$define %type int32_t as 32 bit signed
$define %func libgDefaultStyle as procedure with args libg_style_t *
$define %func libgCreate as function with args srapi_device_t *, style, out
$define %func libgCreateForTarget as function with args pixels, width, height, pitch, callback, userdata, style, out
$define %func libgHandleInput as function with args context, input event
$define %func libgDestroy as procedure with args libg_context_t *
$define %func libg_update_mouse as procedure with args context, event
$define %func libgBegin as function with args context, color
$define %func libgPresent as function with args context
$define %func libgWidth as function with args context
$define %func libgHeight as function with args context
$define %func libgMousePosition as procedure with args context, outputs
$define %func libgFillRect as procedure with args context, rect, color
$define %func libgStrokeRect as procedure with args context, rect, color
$define %func libgLine as procedure with args context, endpoints, color
$define %func libgFillCircle as procedure with args context, center, radius
$define %func libgStrokeCircle as procedure with args context, center, radius
$define %func libgText as procedure with args context, position, text, color
$define %func libgTextScale as procedure with args context, position, text
$define %func libgMeasureText as procedure with args text, scale, out size
$define %func libgPanel as procedure with args context, rect
$define %func libgButton as function with args context, id, rect, label
$define %func libgTextField as function with args context, id, rect, buffer
$define %func libgSlider as function with args context, id, rect, value

*/

/* !SPACE!

$space %internal libg_from_srapi, libg_image_present, libg_abs_i32
$space %internal libg_clamp_i32
$space %internal libg_rect_contains, libg_blend, libg_put_pixel
$space %internal libg_update_mouse, libg_poll_input, libg_draw_text_cell
$space %internal libg_circle_points, libg_draw_centered_text
$space %internal libg_finish_button_state, libg_apply_text_input
$space %export libgDefaultStyle, libgCreate, libgDestroy
$space %export libgCreateForTarget
$space %export libgBegin, libgPresent, libgHandleInput, libgWidth, libgHeight
$space %export libgSetStyle, libgGetStyle, libgMousePosition
$space %export libgFillRect, libgStrokeRect, libgLine
$space %export libgFillCircle, libgStrokeCircle
$space %export libgText, libgTextScale, libgMeasureText
$space %export libgPanel, libgButton, libgTextField, libgSlider

*/


#include <font.h>
#include <libg.h>
#include <srapi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LIBG_INPUT_BATCH		32
#define LIBG_TEXT_INPUT_MAX		32
#define LIBG_MOUSE_LEFT		SRAPI_MOUSE_LEFT
#define LIBG_KEY_ENTER		0x0028
#define LIBG_KEY_BACKSPACE		0x002a

struct libg_context {
	srapi_device_t		*device;
	libg_present_fn		present;
	void			*present_userdata;
	srapi_surface_t		*surface;
	srapi_cmd_buffer_t	*cmd;
	libg_style_t		style;
	uint8_t			*pixels;
	uint32_t		width;
	uint32_t		height;
	uint32_t		pitch;
	int32_t			mouse_x;
	int32_t			mouse_y;
	int32_t			raw_mouse_x;
	int32_t			raw_mouse_y;
	uint32_t		mouse_buttons;
	int			mouse_pressed;
	int			mouse_released;
	int			have_raw_mouse;
	uint32_t		hot_id;
	uint32_t		active_id;
	uint32_t		focus_id;
	char			text_input[LIBG_TEXT_INPUT_MAX];
	uint32_t		text_count;
	uint32_t		backspace_count;
	int			submit_pressed;
};

static int
libg_image_present(void *userdata, const struct srapi_region *region)
{
	return (srapiImageDamage((srapi_image_t *)userdata, region));
}

static void
libg_frame_reset(libg_context_t *ctx)
{
	ctx->hot_id = 0;
	ctx->mouse_pressed = 0;
	ctx->mouse_released = 0;
	ctx->text_count = 0;
	ctx->backspace_count = 0;
	ctx->submit_pressed = 0;
}

static int
libg_from_srapi(int ret)
{
	if (ret == SRAPI_OK) {
		return (LIBG_OK);
	}
	if (ret == SRAPI_ERR_NO_MEMORY) {
		return (LIBG_ERR_NOMEM);
	}
	if (ret == SRAPI_ERR_INVALID || ret == SRAPI_ERR_RANGE) {
		return (LIBG_ERR_INVAL);
	}
	return (LIBG_ERR_DRIVER);
}

static int32_t
libg_abs_i32(int32_t value)
{
	return (value < 0 ? -value : value);
}

static int32_t
libg_clamp_i32(int32_t value, int32_t min, int32_t max)
{
	if (value < min) {
		return (min);
	}
	if (value > max) {
		return (max);
	}
	return (value);
}

static int
libg_rect_contains(libg_rect_t rect, int32_t x, int32_t y)
{
	if (rect.width <= 0 || rect.height <= 0) {
		return (0);
	}
	return (x >= rect.x && y >= rect.y &&
	    x < rect.x + rect.width && y < rect.y + rect.height);
}

static uint32_t
libg_blend(uint32_t dst, uint32_t src)
{
	uint32_t	a, inv;
	uint32_t	sr, sg, sb;
	uint32_t	dr, dg, db;
	uint32_t	r, g, b;

	a = (src >> 24) & 0xff;
	if (a == 0xff) {
		return (src);
	}
	if (a == 0) {
		return (dst);
	}

	inv = 255 - a;
	sr = (src >> 16) & 0xff;
	sg = (src >> 8) & 0xff;
	sb = src & 0xff;
	dr = (dst >> 16) & 0xff;
	dg = (dst >> 8) & 0xff;
	db = dst & 0xff;

	r = (sr * a + dr * inv) / 255;
	g = (sg * a + dg * inv) / 255;
	b = (sb * a + db * inv) / 255;
	return (0xff000000U | (r << 16) | (g << 8) | b);
}

static void
libg_put_pixel(libg_context_t *ctx, int32_t x, int32_t y, uint32_t color)
{
	uint32_t	*pixel;

	if (!ctx || !ctx->pixels || x < 0 || y < 0 ||
	    x >= (int32_t)ctx->width || y >= (int32_t)ctx->height) {
		return;
	}
	pixel = (uint32_t *)(void *)(ctx->pixels + (uint32_t)y * ctx->pitch +
	    (uint32_t)x * sizeof(uint32_t));
	*pixel = libg_blend(*pixel, color);
}

static int
libg_update_mouse(libg_context_t *ctx, const struct srapi_input_event *event)
{
	int32_t	dx, dy;

	if (!ctx || !event) {
		return (LIBG_ERR_INVAL);
	}

	if ((event->flags & SRAPI_MOUSE_ABSOLUTE) != 0) {
		ctx->mouse_x = libg_clamp_i32(event->x, 0,
		    ctx->width > 0 ? (int32_t)ctx->width - 1 : 0);
		ctx->mouse_y = libg_clamp_i32(event->y, 0,
		    ctx->height > 0 ? (int32_t)ctx->height - 1 : 0);
		ctx->raw_mouse_x = event->x;
		ctx->raw_mouse_y = event->y;
		ctx->have_raw_mouse = 0;
		return (LIBG_OK);
	}

	if (ctx->have_raw_mouse) {
		dx = event->x - ctx->raw_mouse_x;
		dy = event->y - ctx->raw_mouse_y;
	} else {
		dx = event->dx;
		dy = event->dy;
		ctx->have_raw_mouse = 1;
	}
	ctx->raw_mouse_x = event->x;
	ctx->raw_mouse_y = event->y;

	ctx->mouse_x = libg_clamp_i32(ctx->mouse_x + dx, 0,
	    ctx->width > 0 ? (int32_t)ctx->width - 1 : 0);
	ctx->mouse_y = libg_clamp_i32(ctx->mouse_y + dy, 0,
	    ctx->height > 0 ? (int32_t)ctx->height - 1 : 0);
	return (LIBG_OK);
}

int
libgHandleInput(libg_context_t *ctx, const struct srapi_input_event *event)
{
	uint32_t old_buttons;
	int ret;

	if (ctx == NULL || event == NULL) {
		return (LIBG_ERR_INVAL);
	}
	if (event->type == SRAPI_INPUT_MOUSE) {
		old_buttons = ctx->mouse_buttons;
		ret = libg_update_mouse(ctx, event);
		if (ret != LIBG_OK) {
			return (ret);
		}
		ctx->mouse_buttons = event->buttons;
		if ((ctx->mouse_buttons & LIBG_MOUSE_LEFT) != 0 &&
		    (old_buttons & LIBG_MOUSE_LEFT) == 0) {
			ctx->mouse_pressed = 1;
		}
		if ((ctx->mouse_buttons & LIBG_MOUSE_LEFT) == 0 &&
		    (old_buttons & LIBG_MOUSE_LEFT) != 0) {
			ctx->mouse_released = 1;
		}
	} else if (event->type == SRAPI_INPUT_KEYBOARD &&
	    (event->flags & (SRAPI_KEY_PRESS | SRAPI_KEY_REPEAT)) != 0) {
		if (event->key == LIBG_KEY_ENTER || event->ch == '\n' ||
		    event->ch == '\r') {
			ctx->submit_pressed = 1;
		} else if (event->key == LIBG_KEY_BACKSPACE ||
		    event->ch == '\b') {
			ctx->backspace_count++;
		} else if (event->ch >= 32 && event->ch < 127 &&
		    ctx->text_count < LIBG_TEXT_INPUT_MAX - 1) {
			ctx->text_input[ctx->text_count] = (char)event->ch;
			ctx->text_count++;
			ctx->text_input[ctx->text_count] = '\0';
		}
	}
	return (LIBG_OK);
}

static int
libg_poll_input(libg_context_t *ctx)
{
	struct srapi_input_event	events[LIBG_INPUT_BATCH];
	uint32_t		i;
	int			ret;

	ctx->mouse_pressed = 0;
	ctx->mouse_released = 0;
	ctx->text_count = 0;
	ctx->backspace_count = 0;
	ctx->submit_pressed = 0;

	ret = srapiPollInput(ctx->device, events, LIBG_INPUT_BATCH);
	if (ret < 0) {
		return (libg_from_srapi(ret));
	}

	for (i = 0; i < (uint32_t)ret; i++) {
		(void)libgHandleInput(ctx, &events[i]);
	}
	ctx->text_input[ctx->text_count] = '\0';
	return (LIBG_OK);
}

static void
libg_draw_text_cell(libg_context_t *ctx, int32_t x, int32_t y,
    uint32_t scale, uint32_t color)
{
	uint32_t	ix, iy;

	for (iy = 0; iy < scale; iy++) {
		for (ix = 0; ix < scale; ix++) {
			libg_put_pixel(ctx, x + (int32_t)ix, y + (int32_t)iy,
			    color);
		}
	}
}

static void
libg_circle_points(libg_context_t *ctx, int32_t cx, int32_t cy,
    int32_t x, int32_t y, uint32_t color)
{
	libg_put_pixel(ctx, cx + x, cy + y, color);
	libg_put_pixel(ctx, cx + y, cy + x, color);
	libg_put_pixel(ctx, cx - y, cy + x, color);
	libg_put_pixel(ctx, cx - x, cy + y, color);
	libg_put_pixel(ctx, cx - x, cy - y, color);
	libg_put_pixel(ctx, cx - y, cy - x, color);
	libg_put_pixel(ctx, cx + y, cy - x, color);
	libg_put_pixel(ctx, cx + x, cy - y, color);
}

static void
libg_draw_centered_text(libg_context_t *ctx, libg_rect_t rect,
    const char *text, uint32_t color, uint32_t scale)
{
	int32_t	tw, th, x, y;

	libgMeasureText(text, scale, &tw, &th);
	x = rect.x + (rect.width - tw) / 2;
	y = rect.y + (rect.height - th) / 2;
	libgTextScale(ctx, x, y, text, color, scale);
}

static uint32_t
libg_finish_button_state(libg_context_t *ctx, uint32_t id, libg_rect_t rect)
{
	uint32_t	state;
	int		inside;

	state = LIBG_WIDGET_NONE;
	inside = libg_rect_contains(rect, ctx->mouse_x, ctx->mouse_y);
	if (inside) {
		ctx->hot_id = id;
	}
	if (id != 0 && inside && ctx->mouse_pressed) {
		ctx->active_id = id;
	}
	if (id != 0 && ctx->mouse_released && ctx->active_id == id) {
		if (inside) {
			state |= LIBG_WIDGET_CLICKED;
		}
		ctx->active_id = 0;
	}
	if (ctx->active_id == id &&
	    (ctx->mouse_buttons & LIBG_MOUSE_LEFT) == 0 &&
	    !ctx->mouse_pressed) {
		ctx->active_id = 0;
	}
	return (state);
}

static uint32_t
libg_apply_text_input(libg_context_t *ctx, char *buf, size_t cap)
{
	size_t		len;
	uint32_t	i;
	uint32_t	state;

	state = LIBG_WIDGET_NONE;
	if (!buf || cap == 0) {
		return (state);
	}
	len = strlen(buf);
	for (i = 0; i < ctx->backspace_count; i++) {
		if (len == 0) {
			break;
		}
		len--;
		buf[len] = '\0';
		state |= LIBG_WIDGET_CHANGED;
	}
	for (i = 0; i < ctx->text_count; i++) {
		if (len + 1 >= cap) {
			break;
		}
		buf[len] = ctx->text_input[i];
		len++;
		buf[len] = '\0';
		state |= LIBG_WIDGET_CHANGED;
	}
	if (ctx->submit_pressed) {
		state |= LIBG_WIDGET_SUBMIT;
	}
	return (state);
}

void
libgDefaultStyle(libg_style_t *out)
{
	if (!out) {
		return;
	}
	out->background = 0xff15171aU;
	out->panel = 0xff20242aU;
	out->panel_border = 0xff3a424dU;
	out->text = 0xfff2f5f8U;
	out->text_muted = 0xffa7b0bbU;
	out->control = 0xff2a3038U;
	out->control_hot = 0xff343d48U;
	out->control_active = 0xff1f5f74U;
	out->field = 0xff111418U;
	out->field_focus = 0xff0d2f3cU;
	out->accent = 0xff23a6d5U;
	out->accent_hot = 0xff4ac4e8U;
	out->danger = 0xffe66a5cU;
}

int
libgCreate(srapi_device_t *device, const libg_style_t *style,
    libg_context_t **out)
{
	struct srapi_surface_desc	sdesc;
	struct srapi_device_info		info;
	libg_context_t			*ctx;
	void				*pixels;
	uint32_t			pitch;
	int				ret;

	if (!device || !out) {
		return (LIBG_ERR_INVAL);
	}
	*out = NULL;
	memset(&info, 0, sizeof(info));
	ret = srapiDeviceInfo(device, &info);
	if (ret != SRAPI_OK || info.width == 0 || info.height == 0) {
		return (libg_from_srapi(ret));
	}

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		return (LIBG_ERR_NOMEM);
	}
	ctx->device = device;
	ctx->width = info.width;
	ctx->height = info.height;
	ctx->mouse_x = (int32_t)(info.width / 2);
	ctx->mouse_y = (int32_t)(info.height / 2);
	libgDefaultStyle(&ctx->style);
	if (style) {
		ctx->style = *style;
	}

	memset(&sdesc, 0, sizeof(sdesc));
	sdesc.width = info.width;
	sdesc.height = info.height;
	sdesc.format = SRAPI_SURFACE_FORMAT_ARGB8888;
	ret = srapiCreateSurface(device, &sdesc, &ctx->surface);
	if (ret != SRAPI_OK) {
		free(ctx);
		return (libg_from_srapi(ret));
	}
	ret = srapiCreateCommandBuffer(device, &ctx->cmd);
	if (ret != SRAPI_OK) {
		srapiDestroySurface(ctx->surface);
		free(ctx);
		return (libg_from_srapi(ret));
	}
	pixels = NULL;
	pitch = 0;
	ret = srapiMapSurface(ctx->surface, &pixels, &pitch);
	if (ret != SRAPI_OK || !pixels || pitch == 0) {
		srapiDestroyCommandBuffer(ctx->cmd);
		srapiDestroySurface(ctx->surface);
		free(ctx);
		return (libg_from_srapi(ret));
	}
	ctx->pixels = pixels;
	ctx->pitch = pitch;
	*out = ctx;
	return (LIBG_OK);
}

int
libgCreateForImage(srapi_image_t *image, const libg_style_t *style,
    libg_context_t **out)
{
	if (image == NULL || out == NULL || srapiImagePixels(image) == NULL ||
	    srapiImageWidth(image) == 0 || srapiImageHeight(image) == 0 ||
	    srapiImagePitch(image) < srapiImageWidth(image) * sizeof(uint32_t) ||
	    srapiImageBpp(image) != 32) {
		return (LIBG_ERR_INVAL);
	}
	return (libgCreateForTarget(srapiImagePixels(image),
	    srapiImageWidth(image), srapiImageHeight(image),
	    srapiImagePitch(image), libg_image_present, image, style, out));
}

int
libgCreateForTarget(void *pixels, uint32_t width, uint32_t height,
    uint32_t pitch, libg_present_fn present, void *userdata,
    const libg_style_t *style, libg_context_t **out)
{
	libg_context_t *ctx;

	if (pixels == NULL || width == 0 || height == 0 ||
	    pitch < width * sizeof(uint32_t) || present == NULL || out == NULL) {
		return (LIBG_ERR_INVAL);
	}
	*out = NULL;
	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return (LIBG_ERR_NOMEM);
	}
	ctx->pixels = pixels;
	ctx->width = width;
	ctx->height = height;
	ctx->pitch = pitch;
	ctx->present = present;
	ctx->present_userdata = userdata;
	ctx->mouse_x = (int32_t)(width / 2);
	ctx->mouse_y = (int32_t)(height / 2);
	libgDefaultStyle(&ctx->style);
	if (style != NULL) {
		ctx->style = *style;
	}
	*out = ctx;
	return (LIBG_OK);
}

void
libgDestroy(libg_context_t *ctx)
{
	if (!ctx) {
		return;
	}
	if (ctx->surface) {
		srapiUnmapSurface(ctx->surface);
	}
	if (ctx->cmd) {
		srapiDestroyCommandBuffer(ctx->cmd);
	}
	if (ctx->surface) {
		srapiDestroySurface(ctx->surface);
	}
	free(ctx);
}

int
libgBegin(libg_context_t *ctx, uint32_t clear_color)
{
	libg_rect_t	rect;
	int		ret;

	if (!ctx) {
		return (LIBG_ERR_INVAL);
	}
	libg_frame_reset(ctx);
	ret = libg_poll_input(ctx);
	if (ret != LIBG_OK) {
		return (ret);
	}

	rect.x = 0;
	rect.y = 0;
	rect.width = (int32_t)ctx->width;
	rect.height = (int32_t)ctx->height;
	libgFillRect(ctx, rect, clear_color);
	return (LIBG_OK);
}

int
libgBeginOverlay(libg_context_t *ctx)
{
	if (ctx == NULL || ctx->pixels == NULL) {
		return (LIBG_ERR_INVAL);
	}
	libg_frame_reset(ctx);
	return (LIBG_OK);
}

int
libgPresent(libg_context_t *ctx)
{
	int	ret;

	if (ctx == NULL) {
		return (LIBG_ERR_INVAL);
	}
	if (ctx->present != NULL) {
		return (libg_from_srapi(ctx->present(ctx->present_userdata,
		    NULL)));
	}
	if (ctx->cmd == NULL || ctx->surface == NULL) {
		return (LIBG_ERR_INVAL);
	}
	ret = srapiCmdBegin(ctx->cmd);
	if (ret != SRAPI_OK) {
		return (libg_from_srapi(ret));
	}
	ret = srapiCmdBlitSurface(ctx->cmd, ctx->surface, NULL, NULL, 0);
	if (ret != SRAPI_OK) {
		return (libg_from_srapi(ret));
	}
	ret = srapiCmdPresent(ctx->cmd);
	if (ret != SRAPI_OK) {
		return (libg_from_srapi(ret));
	}
	ret = srapiCmdEnd(ctx->cmd);
	if (ret != SRAPI_OK) {
		return (libg_from_srapi(ret));
	}
	return (libg_from_srapi(srapiSubmit(ctx->cmd)));
}

uint32_t
libgWidth(const libg_context_t *ctx)
{
	return (ctx ? ctx->width : 0);
}

uint32_t
libgHeight(const libg_context_t *ctx)
{
	return (ctx ? ctx->height : 0);
}

void
libgSetStyle(libg_context_t *ctx, const libg_style_t *style)
{
	if (!ctx || !style) {
		return;
	}
	ctx->style = *style;
}

void
libgGetStyle(const libg_context_t *ctx, libg_style_t *out)
{
	if (!ctx || !out) {
		return;
	}
	*out = ctx->style;
}

void
libgMousePosition(const libg_context_t *ctx, int32_t *out_x,
    int32_t *out_y, uint32_t *out_buttons)
{
	if (out_x) {
		*out_x = ctx ? ctx->mouse_x : 0;
	}
	if (out_y) {
		*out_y = ctx ? ctx->mouse_y : 0;
	}
	if (out_buttons) {
		*out_buttons = ctx ? ctx->mouse_buttons : 0;
	}
}

void
libgFillRect(libg_context_t *ctx, libg_rect_t rect, uint32_t color)
{
	uint32_t	*p;
	int32_t		x0, y0, x1, y1, x, y;

	if (!ctx || !ctx->pixels || rect.width <= 0 || rect.height <= 0) {
		return;
	}
	x0 = rect.x;
	y0 = rect.y;
	x1 = rect.x + rect.width;
	y1 = rect.y + rect.height;
	if (x0 < 0) {
		x0 = 0;
	}
	if (y0 < 0) {
		y0 = 0;
	}
	if (x1 > (int32_t)ctx->width) {
		x1 = (int32_t)ctx->width;
	}
	if (y1 > (int32_t)ctx->height) {
		y1 = (int32_t)ctx->height;
	}
	if (x0 >= x1 || y0 >= y1) {
		return;
	}

	for (y = y0; y < y1; y++) {
		p = (uint32_t *)(void *)(ctx->pixels + (uint32_t)y *
		    ctx->pitch + (uint32_t)x0 * sizeof(uint32_t));
		for (x = x0; x < x1; x++) {
			*p = libg_blend(*p, color);
			p++;
		}
	}
}

void
libgStrokeRect(libg_context_t *ctx, libg_rect_t rect, uint32_t color)
{
	libg_rect_t	part;

	if (!ctx || rect.width <= 0 || rect.height <= 0) {
		return;
	}

	part = rect;
	part.height = 1;
	libgFillRect(ctx, part, color);
	part.y = rect.y + rect.height - 1;
	libgFillRect(ctx, part, color);

	part.x = rect.x;
	part.y = rect.y;
	part.width = 1;
	part.height = rect.height;
	libgFillRect(ctx, part, color);
	part.x = rect.x + rect.width - 1;
	libgFillRect(ctx, part, color);
}

void
libgLine(libg_context_t *ctx, int32_t x0, int32_t y0,
    int32_t x1, int32_t y1, uint32_t color)
{
	int32_t	dx, dy, sx, sy, err, e2;

	if (!ctx) {
		return;
	}

	dx = libg_abs_i32(x1 - x0);
	dy = -libg_abs_i32(y1 - y0);
	sx = x0 < x1 ? 1 : -1;
	sy = y0 < y1 ? 1 : -1;
	err = dx + dy;

	for (;;) {
		libg_put_pixel(ctx, x0, y0, color);
		if (x0 == x1 && y0 == y1) {
			break;
		}
		e2 = err * 2;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

void
libgFillCircle(libg_context_t *ctx, int32_t cx, int32_t cy,
    int32_t radius, uint32_t color)
{
	int32_t	x, y, r2;

	if (!ctx || radius <= 0) {
		return;
	}
	r2 = radius * radius;
	for (y = -radius; y <= radius; y++) {
		for (x = -radius; x <= radius; x++) {
			if (x * x + y * y <= r2) {
				libg_put_pixel(ctx, cx + x, cy + y, color);
			}
		}
	}
}

void
libgStrokeCircle(libg_context_t *ctx, int32_t cx, int32_t cy,
    int32_t radius, uint32_t color)
{
	int32_t	x, y, err;

	if (!ctx || radius <= 0) {
		return;
	}
	x = radius;
	y = 0;
	err = 0;
	while (x >= y) {
		libg_circle_points(ctx, cx, cy, x, y, color);
		y++;
		if (err <= 0) {
			err += 2 * y + 1;
		}
		if (err > 0) {
			x--;
			err -= 2 * x + 1;
		}
	}
}

void
libgText(libg_context_t *ctx, int32_t x, int32_t y,
    const char *text, uint32_t color)
{
	libgTextScale(ctx, x, y, text, color, 1);
}

void
libgTextScale(libg_context_t *ctx, int32_t x, int32_t y,
    const char *text, uint32_t color, uint32_t scale)
{
	const char	*p;
	int32_t		origin_x;
	uint32_t	row, col, bits;

	if (!ctx || !text) {
		return;
	}
	if (scale == 0) {
		scale = 1;
	}

	origin_x = x;
	for (p = text; *p != '\0'; p++) {
		if (*p == '\n') {
			x = origin_x;
			y += (LIBG_FONT_HEIGHT + 2) * (int32_t)scale;
			continue;
		}
		if (*p == '\t') {
			x += LIBG_FONT_ADVANCE * (int32_t)scale * 4;
			continue;
		}
		for (row = 0; row < LIBG_FONT_HEIGHT; row++) {
			bits = libg_font_row((uint32_t)(unsigned char)*p, row);
			for (col = 0; col < LIBG_FONT_WIDTH; col++) {
				if ((bits & (1U << (LIBG_FONT_WIDTH - 1 - col)))
				    != 0) {
					libg_draw_text_cell(ctx,
					    x + (int32_t)(col * scale),
					    y + (int32_t)(row * scale),
					    scale, color);
				}
			}
		}
		x += LIBG_FONT_ADVANCE * (int32_t)scale;
	}
}

void
libgMeasureText(const char *text, uint32_t scale, int32_t *out_w,
    int32_t *out_h)
{
	const char	*p;
	int32_t		line_w, max_w, lines;

	if (scale == 0) {
		scale = 1;
	}
	line_w = 0;
	max_w = 0;
	lines = 1;
	if (text) {
		for (p = text; *p != '\0'; p++) {
			if (*p == '\n') {
				if (line_w > max_w) {
					max_w = line_w;
				}
				line_w = 0;
				lines++;
			} else if (*p == '\t') {
				line_w += LIBG_FONT_ADVANCE *
				    (int32_t)scale * 4;
			} else {
				line_w += LIBG_FONT_ADVANCE *
				    (int32_t)scale;
			}
		}
	}
	if (line_w > max_w) {
		max_w = line_w;
	}
	if (max_w > 0) {
		max_w -= (int32_t)scale;
	}
	if (out_w) {
		*out_w = max_w;
	}
	if (out_h) {
		*out_h = lines * LIBG_FONT_HEIGHT * (int32_t)scale +
		    (lines - 1) * 2 * (int32_t)scale;
	}
}

void
libgPanel(libg_context_t *ctx, libg_rect_t rect)
{
	if (!ctx) {
		return;
	}
	libgFillRect(ctx, rect, ctx->style.panel);
	libgStrokeRect(ctx, rect, ctx->style.panel_border);
}

uint32_t
libgButton(libg_context_t *ctx, uint32_t id, libg_rect_t rect,
    const char *label)
{
	uint32_t	state, fill;

	if (!ctx) {
		return (LIBG_WIDGET_NONE);
	}
	state = libg_finish_button_state(ctx, id, rect);
	fill = ctx->style.control;
	if (ctx->active_id == id && id != 0) {
		fill = ctx->style.control_active;
	} else if (ctx->hot_id == id && id != 0) {
		fill = ctx->style.control_hot;
	}

	libgFillRect(ctx, rect, fill);
	libgStrokeRect(ctx, rect, ctx->style.panel_border);
	libg_draw_centered_text(ctx, rect, label, ctx->style.text, 2);
	return (state);
}

uint32_t
libgTextField(libg_context_t *ctx, uint32_t id, libg_rect_t rect,
    char *buf, size_t cap)
{
	libg_rect_t	caret;
	uint32_t	state, color;
	int32_t		tw, th;
	int		inside;

	if (!ctx || !buf || cap == 0) {
		return (LIBG_WIDGET_NONE);
	}

	inside = libg_rect_contains(rect, ctx->mouse_x, ctx->mouse_y);
	if (inside) {
		ctx->hot_id = id;
	}
	if (id != 0 && ctx->mouse_pressed) {
		if (inside) {
			ctx->focus_id = id;
		} else if (ctx->focus_id == id) {
			ctx->focus_id = 0;
		}
	}

	state = LIBG_WIDGET_NONE;
	if (ctx->focus_id == id && id != 0) {
		state |= libg_apply_text_input(ctx, buf, cap);
	}

	color = ctx->focus_id == id ? ctx->style.field_focus :
	    ctx->style.field;
	libgFillRect(ctx, rect, color);
	libgStrokeRect(ctx, rect, ctx->style.panel_border);
	libgTextScale(ctx, rect.x + 8, rect.y + (rect.height - 14) / 2,
	    buf, ctx->style.text, 2);

	if (ctx->focus_id == id && id != 0) {
		libgMeasureText(buf, 2, &tw, &th);
		caret.x = rect.x + 8 + tw + 4;
		caret.y = rect.y + (rect.height - 14) / 2;
		caret.width = 2;
		caret.height = 14;
		libgFillRect(ctx, caret, ctx->style.accent_hot);
		(void)th;
	}
	return (state);
}

uint32_t
libgSlider(libg_context_t *ctx, uint32_t id, libg_rect_t rect,
    int32_t min, int32_t max, int32_t *value)
{
	libg_rect_t	track, fill, knob;
	int64_t		range, pos, next;
	uint32_t	state;
	int32_t		old;
	int		inside;

	if (!ctx || !value || max <= min) {
		return (LIBG_WIDGET_NONE);
	}

	inside = libg_rect_contains(rect, ctx->mouse_x, ctx->mouse_y);
	if (inside) {
		ctx->hot_id = id;
	}
	if (id != 0 && inside && ctx->mouse_pressed) {
		ctx->active_id = id;
	}

	state = LIBG_WIDGET_NONE;
	old = *value;
	*value = libg_clamp_i32(*value, min, max);
	range = (int64_t)max - min;

	if (ctx->active_id == id && id != 0 &&
	    (ctx->mouse_buttons & LIBG_MOUSE_LEFT) != 0) {
		next = ((int64_t)(ctx->mouse_x - rect.x) * range) /
		    (rect.width > 1 ? rect.width - 1 : 1);
		*value = libg_clamp_i32(min + (int32_t)next, min, max);
	}
	if (ctx->mouse_released && ctx->active_id == id) {
		ctx->active_id = 0;
	}
	if (*value != old) {
		state |= LIBG_WIDGET_CHANGED;
	}

	track.x = rect.x;
	track.y = rect.y + rect.height / 2 - 3;
	track.width = rect.width;
	track.height = 6;
	fill = track;
	pos = ((int64_t)(*value - min) * rect.width) / range;
	fill.width = (int32_t)pos;
	if (fill.width < 0) {
		fill.width = 0;
	}
	if (fill.width > rect.width) {
		fill.width = rect.width;
	}
	knob.x = rect.x + fill.width - 6;
	knob.y = rect.y + rect.height / 2 - 6;
	knob.width = 12;
	knob.height = 12;

	libgFillRect(ctx, track, ctx->style.control);
	libgFillRect(ctx, fill, ctx->style.accent);
	libgFillCircle(ctx, knob.x + 6, knob.y + 6, 7,
	    ctx->active_id == id ? ctx->style.accent_hot :
	    ctx->style.text_muted);
	libgStrokeCircle(ctx, knob.x + 6, knob.y + 6, 7,
	    ctx->style.panel_border);
	return (state);
}
