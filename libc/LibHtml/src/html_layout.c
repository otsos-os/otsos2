/* !DEFINES!

$define %type html_layout_ctx as layout builder context
$define %func html_layout_create as function with args const html_doc *, int32_t
$define %func html_layout_free as procedure with args html_layout *

*/

/* !SPACE!

$space %internal html_layout_add_line, html_layout_add_link, html_layout_node
$space %export html_layout_create, html_layout_free

*/

#include <ctype.h>
#include <html.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct html_style_state {
	uint32_t	scale;
	uint32_t	color;
	int		bold;
	int		underline;
	const char	*href;
	int32_t		indent_x;
	int		is_pre;
} html_style_state_t;

typedef struct html_layout_ctx {
	html_layout_t		*layout;
	int32_t			max_width;
	int32_t			cursor_x;
	int32_t			cursor_y;
	int32_t			line_h;
	int32_t			margin_left;
	int			at_line_start;
} html_layout_ctx_t;

static void
html_layout_add_line(html_layout_ctx_t *ctx, const char *text, int32_t x,
    int32_t y, int32_t w, int32_t h, const html_style_state_t *style)
{
	html_layout_line_t *line, **tail;

	if (ctx == NULL || text == NULL || text[0] == '\0') {
		return;
	}

	line = (html_layout_line_t *)malloc(sizeof(html_layout_line_t));
	if (line == NULL) {
		return;
	}
	memset(line, 0, sizeof(html_layout_line_t));
	line->text = strdup(text);
	line->x = x;
	line->y = y;
	line->width = w;
	line->height = h;
	line->scale = style->scale;
	line->color = style->color;
	line->bold = style->bold;
	line->underline = style->underline;
	if (style->href != NULL) {
		line->href = strdup(style->href);
	}

	tail = &ctx->layout->lines;
	while (*tail != NULL) {
		tail = &(*tail)->next;
	}
	*tail = line;

	if (style->href != NULL) {
		html_link_box_t *link, **ltail;
		link = (html_link_box_t *)malloc(sizeof(html_link_box_t));
		if (link != NULL) {
			link->rect.x = x;
			link->rect.y = y;
			link->rect.width = w;
			link->rect.height = h;
			link->href = strdup(style->href);
			link->next = NULL;

			ltail = &ctx->layout->links;
			while (*ltail != NULL) {
				ltail = &(*ltail)->next;
			}
			*ltail = link;
		}
	}

	if (x + w > ctx->layout->content_width) {
		ctx->layout->content_width = x + w;
	}
	if (y + h > ctx->layout->content_height) {
		ctx->layout->content_height = y + h;
	}
}

static void
html_layout_new_line(html_layout_ctx_t *ctx, int32_t custom_h)
{
	int32_t h = (custom_h > 0) ? custom_h : ctx->line_h;
	if (h <= 0) {
		h = 18;
	}
	ctx->cursor_y += h;
	ctx->cursor_x = ctx->margin_left;
	ctx->at_line_start = 1;
	ctx->line_h = 0;
	if (ctx->cursor_y > ctx->layout->content_height) {
		ctx->layout->content_height = ctx->cursor_y;
	}
}

static void
html_layout_format_text(html_layout_ctx_t *ctx, const char *text,
    const html_style_state_t *style)
{
	const char	*p, *start;
	char		word[512];
	int32_t		tw, th, space_w, space_h;
	int32_t		char_advance;

	if (ctx == NULL || text == NULL || text[0] == '\0') {
		return;
	}

	char_advance = (int32_t)(style->scale * 6);
	libgMeasureText(" ", style->scale, &space_w, &space_h);
	if (style->scale >= 4) {
		th = 28;
	} else if (style->scale == 3) {
		th = 22;
	} else {
		th = 18;
	}

	p = text;
	while (*p != '\0') {
		/* Skip leading space if at line start */
		if (ctx->at_line_start && !style->is_pre) {
			while (*p != '\0' && isspace((unsigned char)*p)) {
				p++;
			}
			if (*p == '\0') break;
		}

		if (style->is_pre) {
			/* Preformatted line */
			start = p;
			while (*p != '\0' && *p != '\n') {
				p++;
			}
			size_t len = p - start;
			if (len >= sizeof(word)) len = sizeof(word) - 1;
			memcpy(word, start, len);
			word[len] = '\0';
			tw = (int32_t)len * char_advance;
			html_layout_add_line(ctx, word, ctx->cursor_x, ctx->cursor_y, tw, th, style);
			if (*p == '\n') {
				p++;
				html_layout_new_line(ctx, th);
			} else {
				ctx->cursor_x += tw;
			}
			continue;
		}

		/* Normal text word parsing */
		if (isspace((unsigned char)*p)) {
			if (!ctx->at_line_start) {
				ctx->cursor_x += space_w;
			}
			while (*p != '\0' && isspace((unsigned char)*p)) {
				p++;
			}
			continue;
		}

		start = p;
		while (*p != '\0' && !isspace((unsigned char)*p)) {
			p++;
		}
		size_t wlen = p - start;
		if (wlen >= sizeof(word)) wlen = sizeof(word) - 1;
		memcpy(word, start, wlen);
		word[wlen] = '\0';

		tw = (int32_t)wlen * char_advance;

		/* Word wrap check */
		if (!ctx->at_line_start && ctx->cursor_x + tw > ctx->max_width) {
			html_layout_new_line(ctx, th);
		}

		html_layout_add_line(ctx, word, ctx->cursor_x, ctx->cursor_y, tw, th, style);
		ctx->cursor_x += tw;
		ctx->at_line_start = 0;
		if (th > ctx->line_h) {
			ctx->line_h = th;
		}
	}
}

static void
html_layout_node(html_layout_ctx_t *ctx, const html_node_t *node,
    html_style_state_t cur_style)
{
	html_style_state_t next_style = cur_style;
	const html_node_t *child;
	int is_block = 0;
	int is_header = 0;
	const char *href_val = NULL;

	if (node == NULL) {
		return;
	}

	if (node->tag == HTML_TAG_TEXT) {
		html_layout_format_text(ctx, node->text, &cur_style);
		return;
	}

	switch (node->tag) {
	case HTML_TAG_H1:
		is_block = 1;
		is_header = 1;
		next_style.scale = 4;
		next_style.bold = 1;
		break;
	case HTML_TAG_H2:
		is_block = 1;
		is_header = 1;
		next_style.scale = 3;
		next_style.bold = 1;
		break;
	case HTML_TAG_H3:
	case HTML_TAG_H4:
	case HTML_TAG_H5:
	case HTML_TAG_H6:
		is_block = 1;
		is_header = 1;
		next_style.scale = 2;
		next_style.bold = 1;
		break;
	case HTML_TAG_P:
		is_block = 1;
		break;
	case HTML_TAG_DIV:
	case HTML_TAG_TABLE:
	case HTML_TAG_TR:
	case HTML_TAG_BLOCKQUOTE:
		is_block = 1;
		break;
	case HTML_TAG_UL:
	case HTML_TAG_OL:
		is_block = 1;
		next_style.indent_x += 20;
		break;
	case HTML_TAG_LI:
		is_block = 1;
		break;
	case HTML_TAG_A:
		href_val = html_node_get_attr(node, "href");
		if (href_val != NULL) {
			next_style.href = href_val;
		}
		next_style.color = 0xFF0033CC; /* Blue */
		next_style.underline = 1;
		break;
	case HTML_TAG_B:
	case HTML_TAG_STRONG:
	case HTML_TAG_TH:
		next_style.bold = 1;
		break;
	case HTML_TAG_I:
	case HTML_TAG_EM:
	case HTML_TAG_U:
		next_style.underline = 1;
		break;
	case HTML_TAG_PRE:
	case HTML_TAG_CODE:
		is_block = (node->tag == HTML_TAG_PRE);
		next_style.is_pre = 1;
		break;
	case HTML_TAG_BR:
		html_layout_new_line(ctx, 0);
		return;
	case HTML_TAG_HR:
		if (!ctx->at_line_start) {
			html_layout_new_line(ctx, 0);
		}
		ctx->cursor_y += 6;
		html_layout_add_line(ctx, "--------------------------------------------------------------------------------",
		    ctx->margin_left, ctx->cursor_y, ctx->max_width - ctx->margin_left, 10, &cur_style);
		ctx->cursor_y += 8;
		html_layout_new_line(ctx, 0);
		return;
	default:
		break;
	}

	if (is_block) {
		if (!ctx->at_line_start) {
			html_layout_new_line(ctx, 0);
		}
		if (is_header) {
			ctx->cursor_y += (next_style.scale == 4) ? 14 : 10;
		} else if (node->tag == HTML_TAG_P) {
			ctx->cursor_y += 6;
		}
		ctx->margin_left = next_style.indent_x;
		ctx->cursor_x = ctx->margin_left;

		if (node->tag == HTML_TAG_LI) {
			/* Draw bullet marker */
			html_style_state_t bullet_style = next_style;
			bullet_style.scale = 2;
			bullet_style.bold = 1;
			html_layout_add_line(ctx, "* ", ctx->cursor_x - 12, ctx->cursor_y, 12, 18, &bullet_style);
		}
	}

	/* Process child nodes */
	for (child = node->first_child; child != NULL; child = child->next_sibling) {
		html_layout_node(ctx, child, next_style);
	}

	if (is_block) {
		if (!ctx->at_line_start) {
			html_layout_new_line(ctx, 0);
		}
		if (is_header) {
			ctx->cursor_y += (next_style.scale == 4) ? 12 : 8;
		} else if (node->tag == HTML_TAG_P || node->tag == HTML_TAG_UL || node->tag == HTML_TAG_OL) {
			ctx->cursor_y += 8;
		}
	}
}

html_layout_t *
html_layout_create(const html_doc_t *doc, int32_t max_width)
{
	html_layout_t		*layout;
	html_layout_ctx_t	ctx;
	html_style_state_t	initial_style;
	const html_node_t	*body_node;

	if (doc == NULL || doc->root == NULL) {
		return (NULL);
	}
	if (max_width < 100) {
		max_width = 600;
	}

	layout = (html_layout_t *)malloc(sizeof(html_layout_t));
	if (layout == NULL) {
		return (NULL);
	}
	memset(layout, 0, sizeof(html_layout_t));

	memset(&ctx, 0, sizeof(ctx));
	ctx.layout = layout;
	ctx.max_width = max_width - 24;
	ctx.cursor_x = 16;
	ctx.cursor_y = 16;
	ctx.margin_left = 16;
	ctx.at_line_start = 1;

	memset(&initial_style, 0, sizeof(initial_style));
	initial_style.scale = 2;
	initial_style.color = 0xFF111111; /* Dark text */
	initial_style.indent_x = 16;

	/* Find body node if exists */
	body_node = doc->root;
	for (const html_node_t *n = doc->root->first_child; n != NULL; n = n->next_sibling) {
		if (n->tag == HTML_TAG_BODY) {
			body_node = n;
			break;
		}
	}

	html_layout_node(&ctx, body_node, initial_style);

	layout->content_height = ctx.cursor_y + 32;
	return (layout);
}

void
html_layout_free(html_layout_t *layout)
{
	html_layout_line_t	*line, *next_line;
	html_link_box_t		*link, *next_link;

	if (layout == NULL) {
		return;
	}

	line = layout->lines;
	while (line != NULL) {
		next_line = line->next;
		if (line->text) free(line->text);
		if (line->href) free(line->href);
		free(line);
		line = next_line;
	}

	link = layout->links;
	while (link != NULL) {
		next_link = link->next;
		if (link->href) free(link->href);
		free(link);
		link = next_link;
	}

	free(layout);
}
