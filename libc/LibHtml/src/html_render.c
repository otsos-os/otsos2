/* !DEFINES!

$define %func html_layout_render as procedure with args libg_context *, const html_layout *, int32_t, int32_t, int32_t, int32_t, int32_t
$define %func html_layout_hit_test as function with args const html_layout *, int32_t, int32_t

*/

/* !SPACE!

$space %internal html_layout_render_boxes
$space %export html_layout_render, html_layout_hit_test

*/

#include <html.h>
#include <libg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Must match LibG's glyph box; strike and underline are placed off it. */
#define HTML_GLYPH_HEIGHT	7

/*
 * Non-text geometry first, in one pass, so every rule and cell border ends up
 * beneath the glyphs regardless of the order layout emitted them in.
 */
static void
html_layout_render_boxes(libg_context_t *ctx, const html_layout_t *layout,
    int32_t view_x, int32_t view_y, int32_t view_h, int32_t scroll_y)
{
	const html_layout_box_t	*box;
	libg_rect_t		r;

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
		}
	}
}

void
html_layout_render(libg_context_t *ctx, const html_layout_t *layout,
    int32_t view_x, int32_t view_y, int32_t view_w, int32_t view_h,
    int32_t scroll_y)
{
	const html_layout_line_t	*line;
	int32_t				sx, sy, sh, rule_y;

	(void)view_w;
	if (ctx == NULL || layout == NULL) {
		return;
	}

	html_layout_render_boxes(ctx, layout, view_x, view_y, view_h,
	    scroll_y);

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
