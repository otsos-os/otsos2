/* !DEFINES!

$define %type html_image_draw_fn as host callback that paints replaced elements
$define %func html_layout_render as procedure with args libg_context *, const html_layout *, int32_t, int32_t, int32_t, int32_t, int32_t, html_image_draw_fn, void *
$define %func html_layout_hit_test as function with args const html_layout *, int32_t, int32_t

*/

/* !SPACE!

$space %internal html_layout_render_boxes, html_layout_draw_placeholder
$space %internal html_render_ctrl
$space %export html_layout_render, html_layout_hit_test

*/

#include <html.h>
#include <libg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Must match LibG's glyph box; strike and underline are placed off it. */
#define HTML_GLYPH_HEIGHT	7
#define HTML_GLYPH_ADVANCE	6

/*
 * Widest control text painted in one call.  Text past this is truncated, not
 * wrapped: a single-line field has nowhere to wrap to, and the visible window
 * is bounded by the field width anyway.
 */
#define HTML_CTRL_TEXT_MAX	512

/* Must match the layout's defaults; only used for placeholder text here. */
#define HTML_COLOR_CTRL_HINT	0xFF8C8880
#define HTML_COLOR_CARET	0xFF202020

static void
html_layout_draw_placeholder(libg_context_t *ctx, libg_rect_t r,
    uint32_t color)
{
	libgStrokeRect(ctx, r, color);
	if (r.height > 14) {
		libgLine(ctx, r.x, r.y + 7, r.x + r.width - 1, r.y + 7,
		    color);
	}
}

/*
 * Non-text geometry first, in one pass, so every rule and cell border ends up
 * beneath the glyphs regardless of the order layout emitted them in.
 */
static void
html_layout_render_boxes(libg_context_t *ctx, const html_layout_t *layout,
    int32_t view_x, int32_t view_y, int32_t view_h, int32_t scroll_y,
    html_image_draw_fn draw, void *draw_user)
{
	const html_layout_box_t	*box;
	libg_rect_t		r;
	int			painted;

	for (box = layout->boxes; box != NULL; box = box->next) {
		r = box->rect;
		r.y = r.y - scroll_y + view_y;
		r.x += view_x;
		if (r.y + r.height < view_y || r.y > view_y + view_h) {
			continue;
		}
		switch (box->kind) {
		case HTML_BOX_FILL:
			libgFillRect(ctx, r, box->color);
			break;
		case HTML_BOX_STROKE:
			libgStrokeRect(ctx, r, box->color);
			break;
		case HTML_BOX_HLINE:
			libgLine(ctx, r.x, r.y, r.x + r.width, r.y,
			    box->color);
			break;
		case HTML_BOX_IMAGE:
			painted = 0;
			if (draw != NULL && box->ref != NULL &&
			    box->ref[0] != '\0') {
				painted = (draw(draw_user, ctx, r,
				    box->ref) == 0);
			}
			if (painted == 0) {
				html_layout_draw_placeholder(ctx, r,
				    box->color);
			}
			break;
		}
	}
}

/*
 * Paints one control: fill, frame, then its text.
 *
 * The text is truncated by character count rather than clipped, because
 * libgSetClip() replaces the active clip instead of intersecting it and there
 * is no way to read the current one back - setting a clip here would silently
 * discard the viewport clip the caller installed, and the rest of the page
 * would paint over the browser chrome.  Truncating is exact anyway: the font is
 * fixed-advance.
 */
static void
html_render_ctrl(libg_context_t *ctx, const html_layout_t *layout,
    const html_ctrl_box_t *c, int32_t view_x, int32_t view_y, int32_t scroll_y)
{
	char		buf[HTML_CTRL_TEXT_MAX];
	libg_rect_t	r;
	const char	*text;
	uint32_t	fg;
	int32_t		adv, inner_w, cols, tx, ty, i;
	size_t		len, start, caret, n;
	int		focused, hint;

	r = c->rect;
	r.x += view_x;
	r.y = r.y - scroll_y + view_y;

	if ((c->bg >> 24) != 0) {
		libgFillRect(ctx, r, c->bg);
	}
	if ((c->border >> 24) != 0) {
		/*
		 * Nested strokes rather than a filled ring: a 1px border is the
		 * overwhelming common case and this keeps it a single call,
		 * while a thick border still comes out the right weight.
		 */
		for (i = 0; i < c->border_width && i * 2 < r.height &&
		    i * 2 < r.width; i++) {
			libg_rect_t	b = r;

			b.x += i;
			b.y += i;
			b.width -= i * 2;
			b.height -= i * 2;
			libgStrokeRect(ctx, b, c->border);
		}
	}

	if (c->kind == HTML_CTRL_CHECKBOX || c->kind == HTML_CTRL_RADIO) {
		/* A tick, not a glyph: the 5x7 font has no box-drawing cell. */
		if (html_node_get_attr(c->node, "checked") != NULL) {
			libg_rect_t	t = r;

			t.x += 3;
			t.y += 3;
			t.width -= 6;
			t.height -= 6;
			if (t.width > 0 && t.height > 0) {
				libgFillRect(ctx, t, c->fg);
			}
		}
		return;
	}

	focused = (layout->focus == c);
	hint = 0;
	fg = c->fg;

	if (c->label != NULL) {
		text = c->label;
	} else {
		text = html_node_get_attr(c->node, "value");
		if ((text == NULL || text[0] == '\0') && focused == 0) {
			const char	*ph = html_node_get_attr(c->node,
					    "placeholder");

			if (ph != NULL && ph[0] != '\0') {
				text = ph;
				hint = 1;
				fg = HTML_COLOR_CTRL_HINT;
			}
		}
		if (text == NULL) {
			text = "";
		}
	}

	adv = (int32_t)c->scale * HTML_GLYPH_ADVANCE;
	if (adv <= 0) {
		adv = HTML_GLYPH_ADVANCE;
	}
	inner_w = r.width - c->pad_left - c->border_width;
	if (inner_w <= 0) {
		return;
	}
	cols = inner_w / adv;
	if (cols <= 0) {
		return;
	}

	len = strlen(text);
	caret = (focused != 0 && hint == 0) ? layout->caret : 0;
	if (caret > len) {
		caret = len;
	}

	/*
	 * Horizontal scroll: keep the caret in view by showing the window that
	 * ends at it.  Without this a query longer than the field looks empty
	 * from the first overflowing character on, and the user cannot see what
	 * they are typing.
	 */
	start = 0;
	if (focused != 0 && caret > (size_t)cols) {
		start = caret - (size_t)cols;
	}
	n = len - start;
	if (n > (size_t)cols) {
		n = (size_t)cols;
	}
	if (n >= sizeof(buf)) {
		n = sizeof(buf) - 1;
	}
	memcpy(buf, text + start, n);
	buf[n] = '\0';

	if (c->password != 0 && hint == 0) {
		memset(buf, '*', n);
	}

	tx = r.x + c->pad_left;
	ty = r.y + c->pad_top;

	/* Button faces are centred; field text is left-aligned at the pad. */
	if (c->label != NULL) {
		int32_t	slack = inner_w - (int32_t)n * adv;

		if (slack > 0) {
			tx += slack / 2;
		}
	}

	if (buf[0] != '\0') {
		if (c->scale > 1) {
			libgTextScale(ctx, tx, ty, buf, fg, c->scale);
		} else {
			libgText(ctx, tx, ty, buf, fg);
		}
	}

	if (focused != 0 && layout->caret_on != 0 && c->label == NULL) {
		int32_t	cx = r.x + c->pad_left +
			    (int32_t)(caret - start) * adv;
		int32_t	cy0 = ty;
		int32_t	cy1 = ty + (int32_t)c->scale * HTML_GLYPH_HEIGHT;

		if (cx >= r.x && cx < r.x + r.width) {
			libgLine(ctx, cx, cy0, cx, cy1, HTML_COLOR_CARET);
		}
	}
}

void
html_layout_render(libg_context_t *ctx, const html_layout_t *layout,
    int32_t view_x, int32_t view_y, int32_t view_w, int32_t view_h,
    int32_t scroll_y, html_image_draw_fn draw, void *draw_user)
{
	const html_layout_line_t	*line;
	int32_t				sx, sy, sh, rule_y;

	(void)view_w;
	if (ctx == NULL || layout == NULL) {
		return;
	}

	html_layout_render_boxes(ctx, layout, view_x, view_y, view_h,
	    scroll_y, draw, draw_user);

	/*
	 * Controls go after the boxes and before the text runs.  They paint
	 * their own label, so ordering them with the boxes would put a page
	 * background fill on top of a button face.
	 */
	{
		const html_ctrl_box_t	*c;

		for (c = layout->ctrls; c != NULL; c = c->next) {
			int32_t	cy = c->rect.y - scroll_y + view_y;

			if (cy + c->rect.height < view_y ||
			    cy > view_y + view_h) {
				continue;
			}
			html_render_ctrl(ctx, layout, c, view_x, view_y,
			    scroll_y);
		}
	}

	for (line = layout->lines; line != NULL; line = line->next) {
		sy = line->y - scroll_y + view_y;
		sh = line->height;
		sx = line->x + view_x;

		/* Clipping check */
		if (sy + sh < view_y || sy > view_y + view_h) {
			continue;
		}

		if (line->scale > 1) {
			libgTextScale(ctx, sx, sy, line->text, line->color, line->scale);
			if (line->bold) {
				libgTextScale(ctx, sx + 1, sy, line->text, line->color, line->scale);
			}
		} else {
			libgText(ctx, sx, sy, line->text, line->color);
			if (line->bold) {
				libgText(ctx, sx + 1, sy, line->text, line->color);
			}
		}

		if (line->underline) {
			rule_y = sy + (int32_t)line->scale * HTML_GLYPH_HEIGHT
			    + 1;
			if (rule_y >= view_y && rule_y < view_y + view_h) {
				libgLine(ctx, sx, rule_y, sx + line->width,
				    rule_y, line->color);
			}
		}

		if (line->strike) {
			/* Through the glyph middle, not the line box middle:
			 * the line box carries leading below the glyphs. */
			rule_y = sy + ((int32_t)line->scale *
			    HTML_GLYPH_HEIGHT) / 2;
			if (rule_y >= view_y && rule_y < view_y + view_h) {
				libgLine(ctx, sx, rule_y, sx + line->width,
				    rule_y, line->color);
			}
		}
	}
}

const char *
html_layout_hit_test(const html_layout_t *layout, int32_t doc_x, int32_t doc_y)
{
	const html_link_box_t *link;

	if (layout == NULL) {
		return (NULL);
	}

	/*
	 * Exact bounds, no slop.  The old +-4px tolerance made adjacent links
	 * in one sentence overlap, so a click near a word boundary opened the
	 * wrong target.  Link rects now cover the glyph run precisely.
	 */
	for (link = layout->links; link != NULL; link = link->next) {
		if (doc_x >= link->rect.x &&
		    doc_x < link->rect.x + link->rect.width &&
		    doc_y >= link->rect.y &&
		    doc_y < link->rect.y + link->rect.height) {
			return (link->href);
		}
	}
	return (NULL);
}
