/* !DEFINES!

$define %func html_layout_render as procedure with args libg_context *, const html_layout *, int32_t, int32_t, int32_t, int32_t, int32_t
$define %func html_layout_hit_test as function with args const html_layout *, int32_t, int32_t

*/

/* !SPACE!

$space %export html_layout_render, html_layout_hit_test

*/

#include <html.h>
#include <libg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
html_layout_render(libg_context_t *ctx, const html_layout_t *layout,
    int32_t view_x, int32_t view_y, int32_t view_w, int32_t view_h,
    int32_t scroll_y)
{
	const html_layout_line_t	*line;
	int32_t				sx, sy, sh;

	(void)view_w;
	if (ctx == NULL || layout == NULL) {
		return;
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
			int32_t underline_y = sy + (int32_t)(line->scale * 7) + 1;
			if (underline_y >= view_y && underline_y < view_y + view_h) {
				libgLine(ctx, sx, underline_y, sx + line->width, underline_y, line->color);
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

	for (link = layout->links; link != NULL; link = link->next) {
		if (doc_x >= link->rect.x - 4 && doc_x <= link->rect.x + link->rect.width + 4 &&
		    doc_y >= link->rect.y - 2 && doc_y <= link->rect.y + link->rect.height + 4) {
			return (link->href);
		}
	}
	return (NULL);
}
