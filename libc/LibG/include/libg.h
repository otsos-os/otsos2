/* !DEFINES!

$define %type libg_context as opaque LibG UI context
$define %type libg_rect as integer rectangle
$define %type libg_style as immediate UI color palette
$define %type libg_present_fn as external target present callback
$define %type uint32_t as 32 bit unsigned
$define %type int32_t as 32 bit signed
$define %func libgDefaultStyle as procedure with args libg_style *
$define %func libgCreate as function with args srapi_device_t *, style, out
$define %func libgCreateForImage as function with args srapi_image_t *, style, out
$define %func libgCreateForTarget as function with args pixels, width, height, pitch, callback, userdata, style, out
$define %func libgDestroy as procedure with args context
$define %func libgBegin as function with args context, color
$define %func libgBeginOverlay as function with args context
$define %func libgHandleInput as procedure with args context, input event
$define %func libgPresent as function with args context
$define %func libgWidth as function with args context
$define %func libgHeight as function with args context
$define %func libgMousePosition as procedure with args context, outputs
$define %type libg_point as single precision 2d point
$define %func libgFillRect as procedure with args context, rect, color
$define %func libgStrokeRect as procedure with args context, rect, color
$define %func libgLine as procedure with args context, endpoints, color
$define %func libgFillCircle as procedure with args context, center, radius
$define %func libgStrokeCircle as procedure with args context, center, radius
$define %func libgSvgShape as procedure with args context, svg_shape *, affine map
$define %func libgSvgDoc as procedure with args context, svg_doc *, rect
$define %func libgSetClip as procedure with args context, rect
$define %func libgClearClip as procedure with args context
$define %func libgText as procedure with args context, position, text, color
$define %func libgTextScale as procedure with args context, position, text
$define %func libgMeasureText as procedure with args text, scale, out size
$define %func libgUtf8Decode as function with args text, out codepoint
$define %func libgUtf8Encode as function with args codepoint, out buffer
$define %func libgUtf8Prev as function with args text, byte offset
$define %func libgPanel as procedure with args context, rect
$define %func libgButton as function with args context, id, rect, label
$define %func libgTextField as function with args context, id, rect, buffer
$define %func libgPasswordField as function with args context, id, rect, buffer
$define %func libgSlider as function with args context, id, rect, value
$define %func libgScrollbar as function with args context, id, rect, axis, visible, total, step, value
$define %func libgScrollbarThickness as function with args void

$define %type libg_anim as animation state structure
$define %func libgAnimStart as procedure with args anim, from, to, duration, easing, now
$define %func libgAnimUpdate as function with args anim, now, out_value
$define %func libgLerp as function with args a, b, progress
$define %func libgBlendColor as function with args color1, color2, alpha
$define %type libg_image as opaque decoded raster image
$define %func libgImageProbe as function with args bytes, length
$define %func libgImageLoad as function with args bytes, length, out
$define %func libgImageLoadFile as function with args path, out
$define %func libgImageFree as procedure with args image
$define %func libgImageWidth as function with args image
$define %func libgImageHeight as function with args image
$define %func libgImageDraw as procedure with args context, image, x, y
$define %func libgImageDrawScaled as procedure with args context, image, rect
$define %func libgImageStrerror as function with args error code

*/

/* !SPACE!

$space %export libg_context_t, libg_rect_t, libg_style_t, libg_anim_t
$space %export libgDefaultStyle, libgCreate, libgDestroy
$space %export libgCreateForImage, libgCreateForTarget
$space %export libgBegin, libgBeginOverlay, libgHandleInput
$space %export libgPresent, libgWidth, libgHeight
$space %export libgSetStyle, libgGetStyle, libgMousePosition
$space %export libgFillRect, libgStrokeRect, libgLine
$space %export libgFillCircle, libgStrokeCircle
$space %export libgSvgShape, libgSvgDoc
$space %export libgSetClip, libgClearClip
$space %export libgText, libgTextScale, libgMeasureText
$space %export libgUtf8Decode, libgUtf8Encode, libgUtf8Prev
$space %export libgPanel, libgButton, libgTextField, libgSlider
$space %export libgPasswordField
$space %export libgScrollbar, libgScrollbarThickness
$space %export libgAnimStart, libgAnimUpdate, libgLerp, libgBlendColor
$space %export libg_image_t, libgImageProbe, libgImageLoad
$space %export libgImageLoadFile
$space %export libgImageFree, libgImageWidth, libgImageHeight
$space %export libgImageDraw, libgImageDrawScaled, libgImageStrerror

*/

/*
 * Copyright (c) 2026, otsos team
 */

#ifndef LIBG_H
#define LIBG_H

#include <srapi.h>
#include <svg.h>
#include <stddef.h>
#include <stdint.h>

#define LIBG_VERSION_MAJOR	0
#define LIBG_VERSION_MINOR	1

#define LIBG_OK		0
#define LIBG_ERR_INVAL		-1
#define LIBG_ERR_NOMEM		-2
#define LIBG_ERR_DRIVER		-3

#define LIBG_WIDGET_NONE	0x00000000U
#define LIBG_WIDGET_CLICKED	0x00000001U
#define LIBG_WIDGET_CHANGED	0x00000002U
#define LIBG_WIDGET_SUBMIT	0x00000004U
#define LIBG_WIDGET_HOT		0x00000008U
#define LIBG_WIDGET_ACTIVE	0x00000010U
#define LIBG_SCROLL_VERTICAL	0
#define LIBG_SCROLL_HORIZONTAL	1
#define LIBG_EASE_LINEAR	0
#define LIBG_EASE_IN		1
#define LIBG_EASE_OUT		2
#define LIBG_EASE_IN_OUT	3

typedef struct libg_context libg_context_t;
typedef struct libg_image libg_image_t;

typedef int (*libg_present_fn)(void *userdata,
    const struct srapi_region *region);

typedef struct libg_rect {
	int32_t	x;
	int32_t	y;
	int32_t	width;
	int32_t	height;
} libg_rect_t;

typedef struct libg_anim {
	int32_t		from;
	int32_t		to;
	int32_t		current;
	uint64_t	start_ms;
	uint32_t	duration_ms;
	uint32_t	easing;
	int		active;
} libg_anim_t;

typedef struct libg_style {
	uint32_t	background;
	uint32_t	panel;
	uint32_t	panel_border;
	uint32_t	text;
	uint32_t	text_muted;
	uint32_t	control;
	uint32_t	control_hot;
	uint32_t	control_active;
	uint32_t	field;
	uint32_t	field_focus;
	uint32_t	accent;
	uint32_t	accent_hot;
	uint32_t	danger;
} libg_style_t;

void	libgDefaultStyle(libg_style_t *out);
int	libgCreate(srapi_device_t *device, const libg_style_t *style,
	    libg_context_t **out);
int	libgCreateForImage(srapi_image_t *image, const libg_style_t *style,
    libg_context_t **out);
int	libgCreateForTarget(void *pixels, uint32_t width, uint32_t height,
    uint32_t pitch, libg_present_fn present, void *userdata,
    const libg_style_t *style, libg_context_t **out);
void	libgDestroy(libg_context_t *ctx);

int	libgBegin(libg_context_t *ctx, uint32_t clear_color);
int	libgBeginOverlay(libg_context_t *ctx);
int	libgHandleInput(libg_context_t *ctx,
    const struct srapi_input_event *event);
int	libgPresent(libg_context_t *ctx);
uint32_t	libgWidth(const libg_context_t *ctx);
uint32_t	libgHeight(const libg_context_t *ctx);

void	libgSetStyle(libg_context_t *ctx, const libg_style_t *style);
void	libgGetStyle(const libg_context_t *ctx, libg_style_t *out);
void	libgMousePosition(const libg_context_t *ctx, int32_t *out_x,
	    int32_t *out_y, uint32_t *out_buttons);

void	libgFillRect(libg_context_t *ctx, libg_rect_t rect, uint32_t color);
void	libgStrokeRect(libg_context_t *ctx, libg_rect_t rect, uint32_t color);
void	libgLine(libg_context_t *ctx, int32_t x0, int32_t y0,
	    int32_t x1, int32_t y1, uint32_t color);
void	libgFillCircle(libg_context_t *ctx, int32_t cx, int32_t cy,
	    int32_t radius, uint32_t color);
void	libgStrokeCircle(libg_context_t *ctx, int32_t cx, int32_t cy,
	    int32_t radius, uint32_t color);
void	libgSetClip(libg_context_t *ctx, libg_rect_t rect);
void	libgClearClip(libg_context_t *ctx);
void	libgSvgShape(libg_context_t *ctx, const svg_shape_t *shape,
	    const double m[6]);
void	libgSvgDoc(libg_context_t *ctx, const svg_doc_t *doc,
	    libg_rect_t rect);

void	libgText(libg_context_t *ctx, int32_t x, int32_t y,
	    const char *text, uint32_t color);
void	libgTextScale(libg_context_t *ctx, int32_t x, int32_t y,
	    const char *text, uint32_t color, uint32_t scale);
void	libgMeasureText(const char *text, uint32_t scale,
	    int32_t *out_w, int32_t *out_h);

#define LIBG_UTF8_MAX		4
#define LIBG_UTF8_INVALID	0xFFFDU

size_t	libgUtf8Decode(const char *text, uint32_t *out_cp);
size_t	libgUtf8Encode(uint32_t cp, char out[LIBG_UTF8_MAX]);
size_t	libgUtf8Prev(const char *text, size_t offset);

void	libgPanel(libg_context_t *ctx, libg_rect_t rect);
uint32_t	libgButton(libg_context_t *ctx, uint32_t id,
	    libg_rect_t rect, const char *label);
uint32_t	libgTextField(libg_context_t *ctx, uint32_t id,
	    libg_rect_t rect, char *buf, size_t cap);
uint32_t	libgPasswordField(libg_context_t *ctx, uint32_t id,
	    libg_rect_t rect, char *buf, size_t cap);
uint32_t	libgSlider(libg_context_t *ctx, uint32_t id,
	    libg_rect_t rect, int32_t min, int32_t max, int32_t *value);

uint32_t	libgScrollbar(libg_context_t *ctx, uint32_t id,
	    libg_rect_t rect, int axis, int32_t visible, int32_t total,
	    int32_t step, int32_t *value);
int32_t		libgScrollbarThickness(void);

void	libgAnimStart(libg_anim_t *anim, int32_t from, int32_t to,
	    uint32_t duration_ms, uint32_t easing, uint64_t now_ms);
int	libgAnimUpdate(libg_anim_t *anim, uint64_t now_ms, int32_t *out_val);
int32_t	libgLerp(int32_t a, int32_t b, int32_t progress_256);
uint32_t libgBlendColor(uint32_t c1, uint32_t c2, int32_t alpha_256);
int	libgImageProbe(const void *data, size_t len);
int	libgImageLoad(const void *data, size_t len, libg_image_t **out);
int	libgImageLoadFile(const char *path, libg_image_t **out);
void	libgImageFree(libg_image_t *img);
int32_t	libgImageWidth(const libg_image_t *img);
int32_t	libgImageHeight(const libg_image_t *img);
void	libgImageDraw(libg_context_t *ctx, const libg_image_t *img,
	    int32_t x, int32_t y);
void	libgImageDrawScaled(libg_context_t *ctx, const libg_image_t *img,
	    libg_rect_t rect);
const char	*libgImageStrerror(int err);

#endif
