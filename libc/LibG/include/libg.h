/* !DEFINES!

$define %type libg_context as opaque LibG UI context
$define %type libg_rect as integer rectangle
$define %type libg_style as immediate UI color palette
$define %type uint32_t as 32 bit unsigned
$define %type int32_t as 32 bit signed
$define %func libgDefaultStyle as procedure with args libg_style *
$define %func libgCreate as function with args srapi_device_t *, style, out
$define %func libgDestroy as procedure with args context
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

$space %export libg_context_t, libg_rect_t, libg_style_t
$space %export libgDefaultStyle, libgCreate, libgDestroy
$space %export libgBegin, libgPresent, libgWidth, libgHeight
$space %export libgSetStyle, libgGetStyle, libgMousePosition
$space %export libgFillRect, libgStrokeRect, libgLine
$space %export libgFillCircle, libgStrokeCircle
$space %export libgText, libgTextScale, libgMeasureText
$space %export libgPanel, libgButton, libgTextField, libgSlider

*/

/*
 * Copyright (c) 2026, otsos team
 */

#ifndef LIBG_H
#define LIBG_H

#include <srapi.h>
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

typedef struct libg_context	libg_context_t;

typedef struct libg_rect {
	int32_t	x;
	int32_t	y;
	int32_t	width;
	int32_t	height;
} libg_rect_t;

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
void	libgDestroy(libg_context_t *ctx);

int	libgBegin(libg_context_t *ctx, uint32_t clear_color);
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
void	libgText(libg_context_t *ctx, int32_t x, int32_t y,
	    const char *text, uint32_t color);
void	libgTextScale(libg_context_t *ctx, int32_t x, int32_t y,
	    const char *text, uint32_t color, uint32_t scale);
void	libgMeasureText(const char *text, uint32_t scale,
	    int32_t *out_w, int32_t *out_h);

void	libgPanel(libg_context_t *ctx, libg_rect_t rect);
uint32_t	libgButton(libg_context_t *ctx, uint32_t id,
	    libg_rect_t rect, const char *label);
uint32_t	libgTextField(libg_context_t *ctx, uint32_t id,
	    libg_rect_t rect, char *buf, size_t cap);
uint32_t	libgSlider(libg_context_t *ctx, uint32_t id,
	    libg_rect_t rect, int32_t min, int32_t max, int32_t *value);

#endif
