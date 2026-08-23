/* !DEFINES!

$define %type html_layout_ctx as layout builder context
$define %func html_layout_create as function with args const html_doc *, int32_t
$define %func html_layout_free as procedure with args html_layout *

*/

/* !SPACE!

$space %internal html_layout_add_line, html_layout_add_link, html_layout_node
$space %internal html_layout_add_box, html_layout_new_line, html_layout_line_h
$space %internal html_layout_format_text, html_layout_format_pre
$space %internal html_layout_block_space, html_layout_flush_space
$space %internal html_layout_table_cells, html_layout_list_marker
$space %internal html_layout_table, html_layout_table_row, html_layout_table_cols
$space %internal html_layout_children, html_layout_center_range
$space %internal html_layout_control, html_layout_span_attr
$space %export html_layout_create, html_layout_free

*/

#include <ctype.h>
#include <html.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Text scale is in font multiples, not points: the renderer draws a 5x7
 * bitmap font, so 1 is the smallest legible size and 4 is an <h1>.
 */
#define HTML_SCALE_BASE		2
#define HTML_SCALE_SMALL	1
#define HTML_SCALE_H1		4
#define HTML_SCALE_H2		3
#define HTML_SCALE_H3		2

#define HTML_LINE_H_BASE	18
#define HTML_LINE_H_H2		22
#define HTML_LINE_H_H1		28

#define HTML_INDENT_STEP	20
#define HTML_MARGIN_LEFT	16

/*
 * LibG's font is a 5x7 bitmap advanced 6px per glyph at scale 1.  Kept here
 * as named constants: if LibG ever changes glyph metrics, every width in this
 * file is wrong and the symptom is text overrunning the right margin.
 */
#define HTML_GLYPH_ADVANCE	6
#define HTML_GLYPH_HEIGHT	7

/* Matches the parser's ceiling; the walk below is recursive. */
#define HTML_LAYOUT_MAX_DEPTH	128

/* Table geometry.  Columns are divided evenly - no content measuring pass. */
#define HTML_TABLE_MAX_COLS	32
#define HTML_TABLE_CELL_PAD	6
#define HTML_TABLE_MIN_COL	48

/*
 * <center> is laid out left-aligned then shifted, so it needs a bound on how
 * many lines it will revisit.  Past this the block stays left-aligned.
 */
#define HTML_CENTER_MAX_LINES	512
#define HTML_CENTER_MAX_ROWS	64

/* Sub/sup baseline shift, in pixels at the current scale's line box. */
#define HTML_SUBSUP_SHIFT	3

#define HTML_LIST_MAX_DEPTH	16

#define HTML_COLOR_TEXT		0xFF111111
#define HTML_COLOR_LINK		0xFF0033CC
#define HTML_COLOR_MUTED	0xFF666666
#define HTML_COLOR_RULE		0xFFB0ACA6
#define HTML_COLOR_MARK		0xFFB08000
#define HTML_COLOR_CELL_BORDER	0xFFC8C4BE
#define HTML_COLOR_QUOTE_BAR	0xFF9E9A94

typedef struct html_style_state {
	const char	*href;
	int32_t		indent_x;
	int32_t		voffset;
	uint32_t	scale;
	uint32_t	color;
	uint32_t	mark_color;
	int		bold;
	int		underline;
	int		strike;
	int		marked;
	int		is_pre;
} html_style_state_t;

typedef struct html_layout_ctx {
	html_layout_t	*layout;
	/* Ordered-list state, one entry per nesting level. */
	int32_t		list_index[HTML_LIST_MAX_DEPTH];
	int		list_ordered[HTML_LIST_MAX_DEPTH];
	int32_t		max_width;
	int32_t		cursor_x;
	int32_t		cursor_y;
	int32_t		line_h;
	int32_t		margin_left;
	/*
	 * Vertical gap owed before the next line is emitted.  Held instead of
	 * applied so that </p><p> collapses to one gap rather than two, and so
	 * a trailing gap never extends the document past its last line.
	 */
	int32_t		pending_space;
	int		at_line_start;
	int		depth;
	int		list_depth;
} html_layout_ctx_t;

static void	html_layout_node(html_layout_ctx_t *ctx, const html_node_t *node,
		    html_style_state_t cur_style);

/*
 * Every append below goes through the layout's tail pointer.  Walking from
 * the head instead is O(n) per word, which on a page of a few thousand words
 * made layout cost more than the network fetch.
 */
static html_layout_box_t *
html_layout_add_box(html_layout_ctx_t *ctx, int32_t x, int32_t y, int32_t w,
    int32_t h, uint32_t color, html_box_kind_t kind)
{
	html_layout_box_t	*box;

	if (ctx == NULL || w <= 0 || h < 0) {
		return (NULL);
	}

	box = (html_layout_box_t *)malloc(sizeof(*box));
	if (box == NULL) {
		return (NULL);
	}
	memset(box, 0, sizeof(*box));
	box->rect.x = x;
	box->rect.y = y;
	box->rect.width = w;
	box->rect.height = h;
	box->color = color;
	box->kind = kind;

	if (ctx->layout->boxes_tail != NULL) {
		ctx->layout->boxes_tail->next = box;
	} else {
		ctx->layout->boxes = box;
	}
	ctx->layout->boxes_tail = box;

	if (x + w > ctx->layout->content_width) {
		ctx->layout->content_width = x + w;
	}
	if (y + h > ctx->layout->content_height) {
		ctx->layout->content_height = y + h;
	}
	return (box);
}

static void
html_layout_add_link(html_layout_ctx_t *ctx, const char *href, int32_t x,
    int32_t y, int32_t w, int32_t h)
{
	html_link_box_t	*link;

	link = (html_link_box_t *)malloc(sizeof(*link));
	if (link == NULL) {
		return;
	}
	memset(link, 0, sizeof(*link));
	link->rect.x = x;
	link->rect.y = y;
	link->rect.width = w;
	link->rect.height = h;
	link->href = strdup(href);
	if (link->href == NULL) {
		free(link);
		return;
	}

	if (ctx->layout->links_tail != NULL) {
		ctx->layout->links_tail->next = link;
	} else {
		ctx->layout->links = link;
	}
	ctx->layout->links_tail = link;
}

static void
html_layout_add_line(html_layout_ctx_t *ctx, const char *text, int32_t x,
    int32_t y, int32_t w, int32_t h, const html_style_state_t *style)
{
	html_layout_line_t	*line;

	if (ctx == NULL || text == NULL || text[0] == '\0') {
		return;
	}

	line = (html_layout_line_t *)malloc(sizeof(*line));
	if (line == NULL) {
		return;
	}
	memset(line, 0, sizeof(*line));
	line->text = strdup(text);
	if (line->text == NULL) {
		free(line);
		return;
	}
	line->x = x;
	line->y = y + style->voffset;
	line->width = w;
	line->height = h;
	line->scale = style->scale;
	line->color = style->color;
	line->bold = style->bold;
	line->underline = style->underline;
	line->strike = style->strike;
	if (style->href != NULL) {
		line->href = strdup(style->href);
	}

	if (ctx->layout->lines_tail != NULL) {
		ctx->layout->lines_tail->next = line;
	} else {
		ctx->layout->lines = line;
	}
	ctx->layout->lines_tail = line;

	/*
	 * <mark> is a fill behind the glyphs.  Emitted before the caller's
	 * own boxes matter because the renderer paints the whole box list
	 * underneath every line, so ordering within the list is irrelevant.
	 */
	if (style->marked) {
		html_layout_add_box(ctx, x - 1, line->y - 1, w + 2, h - 2,
		    style->mark_color, HTML_BOX_FILL);
	}

	if (style->href != NULL) {
		html_layout_add_link(ctx, style->href, x, line->y, w, h);
	}

	if (x + w > ctx->layout->content_width) {
		ctx->layout->content_width = x + w;
	}
	if (line->y + h > ctx->layout->content_height) {
		ctx->layout->content_height = line->y + h;
	}
}

static int32_t
html_layout_line_h(uint32_t scale)
{
	if (scale >= HTML_SCALE_H1) {
		return (HTML_LINE_H_H1);
	}
	if (scale == HTML_SCALE_H2) {
		return (HTML_LINE_H_H2);
	}
	return (HTML_LINE_H_BASE);
}

/*
 * Pending vertical space is applied by the first thing that actually draws,
 * never at request time.  Two consequences that are the whole point: adjacent
 * block margins collapse instead of stacking, and a gap requested by the last
 * closing tag never lengthens the document.
 */
static void
html_layout_flush_space(html_layout_ctx_t *ctx)
{
	if (ctx->pending_space <= 0) {
		return;
	}
	ctx->cursor_y += ctx->pending_space;
	ctx->pending_space = 0;
}

static void
html_layout_block_space(html_layout_ctx_t *ctx, int32_t amount)
{
	if (amount <= 0) {
		return;
	}
	if (amount > ctx->pending_space) {
		ctx->pending_space = amount;
	}
}

static void
html_layout_new_line(html_layout_ctx_t *ctx, int32_t custom_h)
{
	int32_t	h;

	h = (custom_h > 0) ? custom_h : ctx->line_h;
	if (h <= 0) {
		h = HTML_LINE_H_BASE;
	}
	ctx->cursor_y += h;
	ctx->cursor_x = ctx->margin_left;
	ctx->at_line_start = 1;
	ctx->line_h = 0;
	if (ctx->cursor_y > ctx->layout->content_height) {
		ctx->layout->content_height = ctx->cursor_y;
	}
}

/*
 * <pre> keeps runs verbatim: newlines break, tabs expand to the next multiple
 * of eight columns, and nothing wraps.  A long pre line widens
 * content_width instead, which is what the horizontal scrollbar is for.
 */
static void
html_layout_format_pre(html_layout_ctx_t *ctx, const char *text,
    const html_style_state_t *style)
{
	char		chunk[256];
	const char	*p;
	int32_t		advance, th, col, tw;
	size_t		n;

	advance = (int32_t)style->scale * HTML_GLYPH_ADVANCE;
	th = html_layout_line_h(style->scale);
	p = text;
	col = 0;
	n = 0;

	while (1) {
		if (*p == '\0' || *p == '\n' || *p == '\t' ||
		    n + 1 >= sizeof(chunk)) {
			if (n != 0) {
				chunk[n] = '\0';
				tw = (int32_t)n * advance;
				html_layout_add_line(ctx, chunk, ctx->cursor_x,
				    ctx->cursor_y, tw, th, style);
				ctx->cursor_x += tw;
				ctx->at_line_start = 0;
				if (th > ctx->line_h) {
					ctx->line_h = th;
				}
				n = 0;
			}
			if (*p == '\0') {
				break;
			}
			if (*p == '\n') {
				html_layout_new_line(ctx, th);
				col = 0;
				p++;
				continue;
			}
			if (*p == '\t') {
				/* Tab stops every 8 columns, like a terminal. */
				int32_t pad = 8 - (col % 8);
				ctx->cursor_x += pad * advance;
				col += pad;
				p++;
				continue;
			}
			continue;
		}
		if (*p == '\r') {
			p++;
			continue;
		}
		chunk[n++] = *p++;
		col++;
	}
}

static void
html_layout_format_text(html_layout_ctx_t *ctx, const char *text,
    const html_style_state_t *style)
{
	char		word[256];
	const char	*p, *start;
	int32_t		advance, th, space_w, space_h, tw;
	size_t		wlen, take;

	if (ctx == NULL || text == NULL || text[0] == '\0') {
		return;
	}

	if (style->is_pre) {
		html_layout_flush_space(ctx);
		html_layout_format_pre(ctx, text, style);
		return;
	}

	advance = (int32_t)style->scale * HTML_GLYPH_ADVANCE;
	libgMeasureText(" ", style->scale, &space_w, &space_h);
	if (space_w <= 0) {
		space_w = advance;
	}
	th = html_layout_line_h(style->scale);

	p = text;
	while (*p != '\0') {
		if (isspace((unsigned char)*p)) {
			while (*p != '\0' && isspace((unsigned char)*p)) {
				p++;
			}
			/*
			 * Inter-word space only, never leading: a run that
			 * starts with whitespace after a block break would
			 * otherwise indent the line by one glyph.
			 */
			if (!ctx->at_line_start && *p != '\0') {
				ctx->cursor_x += space_w;
			}
			continue;
		}

		start = p;
		while (*p != '\0' && !isspace((unsigned char)*p)) {
			p++;
		}
		wlen = (size_t)(p - start);

		/*
		 * A word longer than the buffer is emitted in slices rather
		 * than truncated, so a 4000-char URL still renders in full.
		 */
		while (wlen != 0) {
			take = wlen;
			if (take >= sizeof(word)) {
				take = sizeof(word) - 1;
			}
			memcpy(word, start, take);
			word[take] = '\0';
			tw = (int32_t)take * advance;

			if (!ctx->at_line_start &&
			    ctx->cursor_x + tw > ctx->max_width) {
				html_layout_new_line(ctx, th);
			}
			html_layout_flush_space(ctx);
			html_layout_add_line(ctx, word, ctx->cursor_x,
			    ctx->cursor_y, tw, th, style);
			ctx->cursor_x += tw;
			ctx->at_line_start = 0;
			if (th > ctx->line_h) {
				ctx->line_h = th;
			}
			start += take;
			wlen -= take;
		}
	}
}

/* Reads a small non-negative integer attribute, clamped.  0 means absent. */
static int32_t
html_layout_span_attr(const html_node_t *node, const char *key, int32_t max)
{
	const char	*val;
	int32_t		n;

	val = html_node_get_attr(node, key);
	if (val == NULL) {
		return (0);
	}
	n = (int32_t)atoi(val);
	if (n < 1) {
		return (0);
	}
	if (n > max) {
		n = max;
	}
	return (n);
}

static void
html_layout_list_marker(html_layout_ctx_t *ctx,
    const html_style_state_t *style)
{
	html_style_state_t	marker;
	char			buf[24];
	int32_t			tw, th;
	int			level, ordered;

	level = ctx->list_depth - 1;
	ordered = (level >= 0) ? ctx->list_ordered[level] : 0;

	marker = *style;
	marker.href = NULL;
	marker.underline = 0;
	marker.strike = 0;
	marker.marked = 0;
	marker.bold = ordered ? 0 : 1;

	if (ordered) {
		snprintf(buf, sizeof(buf), "%d.",
		    (int)ctx->list_index[level]);
	} else {
		/*
		 * The 5x7 font has no bullet glyph, so nesting is shown by
		 * cycling ASCII markers the way a plain-text browser does.
		 */
		static const char *const bullets[] = { "*", "-", "+" };
		size_t which = (level < 0) ? 0 :
		    (size_t)level % (sizeof(bullets) / sizeof(bullets[0]));
		snprintf(buf, sizeof(buf), "%s", bullets[which]);
	}

	th = html_layout_line_h(marker.scale);
	tw = (int32_t)strlen(buf) * (int32_t)marker.scale * HTML_GLYPH_ADVANCE;

	html_layout_flush_space(ctx);
	/*
	 * Hangs in the indent to the left of the item text.  Clamped at 0 so a
	 * top-level <li> with no enclosing <ul> cannot draw off-canvas.
	 */
	int32_t mx = ctx->cursor_x - tw - 4;
	if (mx < 0) {
		mx = 0;
	}
	html_layout_add_line(ctx, buf, mx, ctx->cursor_y, tw, th, &marker);
	if (th > ctx->line_h) {
		ctx->line_h = th;
	}
	ctx->at_line_start = 0;
}

/*
 * <center> and align=center are resolved after the fact: the block is laid
 * out left-aligned, then every line it produced is shifted right by half its
 * row's slack.  Rows are keyed by y, which is why the row table is bounded -
 * a pathological block just stays left-aligned instead of costing O(n^2).
 */
static void
html_layout_center_range(html_layout_line_t *first,
    html_link_box_t *first_link, int32_t left, int32_t right)
{
	int32_t			row_y[HTML_CENTER_MAX_ROWS];
	int32_t			row_shift[HTML_CENTER_MAX_ROWS];
	html_layout_line_t	*line;
	html_link_box_t		*link;
	int32_t			end, avail;
	int			rows, i, seen;

	if (first == NULL || right - left <= 0) {
		return;
	}

	rows = 0;
	seen = 0;
	for (line = first; line != NULL; line = line->next) {
		if (++seen > HTML_CENTER_MAX_LINES) {
			return;
		}
		end = line->x + line->width;
		for (i = 0; i < rows; i++) {
			if (row_y[i] == line->y) {
				break;
			}
		}
		if (i == rows) {
			if (rows == HTML_CENTER_MAX_ROWS) {
				return;
			}
			row_y[rows] = line->y;
			row_shift[rows] = end;
			rows++;
		} else if (end > row_shift[i]) {
			row_shift[i] = end;
		}
	}

	/* row_shift holds the row's right edge; convert it to a delta. */
	for (i = 0; i < rows; i++) {
		avail = right - row_shift[i];
		row_shift[i] = (avail > 0) ? avail / 2 : 0;
	}

	for (line = first; line != NULL; line = line->next) {
		for (i = 0; i < rows; i++) {
			if (row_y[i] == line->y) {
				line->x += row_shift[i];
				break;
			}
		}
	}
	for (link = first_link; link != NULL; link = link->next) {
		for (i = 0; i < rows; i++) {
			if (row_y[i] == link->rect.y) {
				link->rect.x += row_shift[i];
				break;
			}
		}
	}
}

/*
 * Form controls are drawn, not interactive: LibHtml is HTML only, so there is
 * no scripting or submission to hang them off.  A stroked box with the value
 * or placeholder keeps the page's shape readable.
 */
static void
html_layout_control(html_layout_ctx_t *ctx, const html_node_t *node,
    const html_style_state_t *style)
{
	html_style_state_t	inner;
	const char		*label, *type;
	char			*owned;
	int32_t			tw, th, pad;

	type = html_node_get_attr(node, "type");
	if (type != NULL && strcasecmp(type, "hidden") == 0) {
		return;
	}

	owned = NULL;
	label = html_node_get_attr(node, "value");
	if (label == NULL) {
		label = html_node_get_attr(node, "placeholder");
	}
	if (label == NULL && node->tag != HTML_TAG_INPUT) {
		owned = html_node_text(node);
		label = owned;
	}
	if (label == NULL || label[0] == '\0') {
		label = (node->tag == HTML_TAG_BUTTON) ? "[button]" : " ";
	}

	inner = *style;
	inner.underline = 0;
	inner.strike = 0;
	inner.marked = 0;

	th = html_layout_line_h(inner.scale);
	pad = 4;
	tw = (int32_t)strlen(label) * (int32_t)inner.scale * HTML_GLYPH_ADVANCE;
	if (tw > ctx->max_width - ctx->margin_left - 2 * pad) {
		tw = ctx->max_width - ctx->margin_left - 2 * pad;
	}
	if (tw < HTML_GLYPH_ADVANCE) {
		tw = HTML_GLYPH_ADVANCE * 4;
	}

	if (!ctx->at_line_start && ctx->cursor_x + tw + 2 * pad >
	    ctx->max_width) {
		html_layout_new_line(ctx, th);
	}
	html_layout_flush_space(ctx);

	html_layout_add_box(ctx, ctx->cursor_x, ctx->cursor_y, tw + 2 * pad,
	    th - 2, HTML_COLOR_CELL_BORDER, HTML_BOX_STROKE);
	html_layout_add_line(ctx, label, ctx->cursor_x + pad, ctx->cursor_y,
	    tw, th, &inner);
	ctx->cursor_x += tw + 2 * pad + HTML_GLYPH_ADVANCE;
	ctx->at_line_start = 0;
	if (th > ctx->line_h) {
		ctx->line_h = th;
	}
	if (owned != NULL) {
		free(owned);
	}
}

static void
html_layout_children(html_layout_ctx_t *ctx, const html_node_t *node,
    html_style_state_t style)
{
	const html_node_t	*child;

	for (child = node->first_child; child != NULL;
	    child = child->next_sibling) {
		html_layout_node(ctx, child, style);
	}
}

/*
 * Widest row wins the column count.  There is no content-measuring pass, so
 * columns are equal width; colspan is honoured only as a width multiplier.
 */
static int
html_layout_table_cols(const html_node_t *table)
{
	const html_node_t	*sec, *row, *cell;
	int			max, n, span;

	max = 0;
	for (sec = table->first_child; sec != NULL; sec = sec->next_sibling) {
		for (row = sec; row != NULL; row = NULL) {
			const html_node_t *rows;

			if (sec->tag == HTML_TAG_TR) {
				rows = sec;
			} else if (sec->tag == HTML_TAG_THEAD ||
			    sec->tag == HTML_TAG_TBODY ||
			    sec->tag == HTML_TAG_TFOOT) {
				rows = sec->first_child;
			} else {
				break;
			}
			for (; rows != NULL; rows = rows->next_sibling) {
				if (rows->tag != HTML_TAG_TR) {
					continue;
				}
				n = 0;
				for (cell = rows->first_child; cell != NULL;
				    cell = cell->next_sibling) {
					if (cell->tag != HTML_TAG_TD &&
					    cell->tag != HTML_TAG_TH) {
						continue;
					}
					span = html_layout_span_attr(cell,
					    "colspan", HTML_TABLE_MAX_COLS);
					n += (span > 0) ? span : 1;
				}
				if (n > max) {
					max = n;
				}
				if (sec->tag == HTML_TAG_TR) {
					break;
				}
			}
		}
		if (max >= HTML_TABLE_MAX_COLS) {
			max = HTML_TABLE_MAX_COLS;
			break;
		}
	}
	return (max);
}

/*
 * Lays out one cell inside a fixed rectangle and returns the y it ended at.
 * Saving and restoring the whole cursor block is what makes nested tables
 * work: the recursion sees a narrower canvas and nothing else.
 */
static int32_t
html_layout_table_cells(html_layout_ctx_t *ctx, const html_node_t *cell,
    html_style_state_t style, int32_t x, int32_t y, int32_t w)
{
	int32_t	save_max, save_x, save_y, save_line_h, save_margin;
	int32_t	save_pending, bottom;
	int	save_at_start;

	save_max = ctx->max_width;
	save_x = ctx->cursor_x;
	save_y = ctx->cursor_y;
	save_line_h = ctx->line_h;
	save_margin = ctx->margin_left;
	save_pending = ctx->pending_space;
	save_at_start = ctx->at_line_start;

	ctx->margin_left = x + HTML_TABLE_CELL_PAD;
	ctx->max_width = x + w - HTML_TABLE_CELL_PAD;
	ctx->cursor_x = ctx->margin_left;
	ctx->cursor_y = y + HTML_TABLE_CELL_PAD;
	ctx->line_h = 0;
	ctx->pending_space = 0;
	ctx->at_line_start = 1;

	style.indent_x = ctx->margin_left;
	if (cell->tag == HTML_TAG_TH) {
		style.bold = 1;
	}
	html_layout_children(ctx, cell, style);

	bottom = ctx->cursor_y;
	if (!ctx->at_line_start) {
		bottom += (ctx->line_h > 0) ? ctx->line_h : HTML_LINE_H_BASE;
	}
	bottom += HTML_TABLE_CELL_PAD;

	ctx->max_width = save_max;
	ctx->cursor_x = save_x;
	ctx->cursor_y = save_y;
	ctx->line_h = save_line_h;
	ctx->margin_left = save_margin;
	ctx->pending_space = save_pending;
	ctx->at_line_start = save_at_start;

	return (bottom);
}

static int32_t
html_layout_table_row(html_layout_ctx_t *ctx, const html_node_t *row,
    html_style_state_t style, int32_t left, int32_t col_w, int32_t top)
{
	const html_node_t	*cell;
	int32_t			x, w, bottom, cell_bottom;
	int			col, span;

	bottom = top;
	x = left;
	col = 0;
	for (cell = row->first_child; cell != NULL;
	    cell = cell->next_sibling) {
		if (cell->tag != HTML_TAG_TD && cell->tag != HTML_TAG_TH) {
			continue;
		}
		if (col >= HTML_TABLE_MAX_COLS) {
			break;
		}
		span = html_layout_span_attr(cell, "colspan",
		    HTML_TABLE_MAX_COLS - col);
		if (span < 1) {
			span = 1;
		}
		w = col_w * span;
		cell_bottom = html_layout_table_cells(ctx, cell, style, x, top,
		    w);
		if (cell_bottom > bottom) {
			bottom = cell_bottom;
		}
		x += w;
		col += span;
	}

	if (col == 0) {
		return (top);
	}
	if (bottom - top < HTML_LINE_H_BASE) {
		bottom = top + HTML_LINE_H_BASE;
	}

	/*
	 * Borders are a second pass because a cell's height is not known until
	 * every cell in the row has been laid out.  Recomputing x is cheaper
	 * than carrying an array of rects.
	 */
	x = left;
	col = 0;
	for (cell = row->first_child; cell != NULL;
	    cell = cell->next_sibling) {
		if (cell->tag != HTML_TAG_TD && cell->tag != HTML_TAG_TH) {
			continue;
		}
		if (col >= HTML_TABLE_MAX_COLS) {
			break;
		}
		span = html_layout_span_attr(cell, "colspan",
		    HTML_TABLE_MAX_COLS - col);
		if (span < 1) {
			span = 1;
		}
		w = col_w * span;
		html_layout_add_box(ctx, x, top, w, bottom - top,
		    HTML_COLOR_CELL_BORDER, HTML_BOX_STROKE);
		x += w;
		col += span;
	}
	return (bottom);
}

static void
html_layout_table(html_layout_ctx_t *ctx, const html_node_t *table,
    html_style_state_t style)
{
	const html_node_t	*sec, *row;
	int32_t			left, avail, col_w, y;
	int			cols;

	cols = html_layout_table_cols(table);
	if (cols <= 0) {
		/* No rows: treat it as a plain block so text is not lost. */
		html_layout_children(ctx, table, style);
		return;
	}

	if (!ctx->at_line_start) {
		html_layout_new_line(ctx, 0);
	}
	html_layout_flush_space(ctx);

	left = style.indent_x;
	avail = ctx->max_width - left;
	col_w = (cols > 0) ? avail / cols : avail;
	if (col_w < HTML_TABLE_MIN_COL) {
		/* Overflow to the right rather than crushing the columns. */
		col_w = HTML_TABLE_MIN_COL;
	}

	y = ctx->cursor_y;
	for (sec = table->first_child; sec != NULL; sec = sec->next_sibling) {
		if (sec->tag == HTML_TAG_CAPTION) {
			html_style_state_t cap = style;
			cap.bold = 1;
			ctx->cursor_y = y;
			ctx->cursor_x = left;
			ctx->margin_left = left;
			ctx->at_line_start = 1;
			html_layout_children(ctx, sec, cap);
			if (!ctx->at_line_start) {
				html_layout_new_line(ctx, 0);
			}
			y = ctx->cursor_y;
			continue;
		}
		if (sec->tag == HTML_TAG_TR) {
			y = html_layout_table_row(ctx, sec, style, left, col_w,
			    y);
			continue;
		}
		if (sec->tag != HTML_TAG_THEAD && sec->tag != HTML_TAG_TBODY &&
		    sec->tag != HTML_TAG_TFOOT) {
			continue;
		}
		for (row = sec->first_child; row != NULL;
		    row = row->next_sibling) {
			if (row->tag != HTML_TAG_TR) {
				continue;
			}
			y = html_layout_table_row(ctx, row, style, left, col_w,
			    y);
		}
	}

	ctx->cursor_y = y;
	ctx->cursor_x = ctx->margin_left;
	ctx->line_h = 0;
	ctx->at_line_start = 1;
	if (y > ctx->layout->content_height) {
		ctx->layout->content_height = y;
	}
}

#define HTML_SVG_DEF_W		300
#define HTML_SVG_DEF_H		150
#define HTML_SVG_MAX_DIM	4096

static int32_t
html_layout_len_attr(const char *s)
{
	int32_t	v;

	if (s == NULL) {
		return (0);
	}
	v = (int32_t)atoi(s);
	if (v <= 0) {
		return (0);
	}
	if (v > HTML_SVG_MAX_DIM) {
		v = HTML_SVG_MAX_DIM;
	}
	return (v);
}

static void
html_layout_svg_box(html_layout_ctx_t *ctx, const html_node_t *node,
    const html_style_state_t *style)
{
	html_layout_box_t	*box;
	const char		*src;
	int32_t			w, h;

	w = html_layout_len_attr(html_node_get_attr(node, "width"));
	h = html_layout_len_attr(html_node_get_attr(node, "height"));
	if (w == 0 || h == 0) {
		w = HTML_SVG_DEF_W;
		h = HTML_SVG_DEF_H;
	}

	if (!ctx->at_line_start) {
		html_layout_new_line(ctx, 0);
	}
	html_layout_block_space(ctx, 4);
	html_layout_flush_space(ctx);

	box = html_layout_add_box(ctx, style->indent_x, ctx->cursor_y, w, h,
	    HTML_COLOR_MUTED, HTML_BOX_IMAGE);
	if (box != NULL) {
		src = (node->text != NULL) ? node->text : "";
		box->ref = strdup(src);
	}

	ctx->cursor_y += h + 6;
	ctx->cursor_x = ctx->margin_left;
	ctx->at_line_start = 1;
	html_layout_block_space(ctx, 6);
}

static void
html_layout_node(html_layout_ctx_t *ctx, const html_node_t *node,
    html_style_state_t cur_style)
{
	html_style_state_t	style;
	html_layout_line_t	*mark_line;
	html_link_box_t		*mark_link;
	const char		*attr;
	int32_t			space_before, space_after, quote_top;
	int			is_block, is_table, centered, pushed_list;

	if (node == NULL || ctx->depth >= HTML_LAYOUT_MAX_DEPTH) {
		return;
	}

	if (node->tag == HTML_TAG_TEXT) {
		html_layout_format_text(ctx, node->text, &cur_style);
		return;
	}

	style = cur_style;
	is_block = 0;
	is_table = 0;
	centered = 0;
	pushed_list = 0;
	space_before = 0;
	space_after = 0;

	switch (node->tag) {
	/*
	 * Metadata and non-HTML payloads.  The parser already discards raw
	 * text for style/script/template/noscript; dropping the elements here
	 * keeps stray attributes and <title> out of the page body.
	 */
	case HTML_TAG_HEAD:
	case HTML_TAG_TITLE:
	case HTML_TAG_STYLE:
	case HTML_TAG_SCRIPT:
	case HTML_TAG_TEMPLATE:
	case HTML_TAG_NOSCRIPT:
	case HTML_TAG_META:
	case HTML_TAG_LINK:
	case HTML_TAG_BASE:
	case HTML_TAG_COLGROUP:
	case HTML_TAG_COL:
	case HTML_TAG_PARAM:
	case HTML_TAG_SOURCE:
	case HTML_TAG_TRACK:
	case HTML_TAG_AREA:
	case HTML_TAG_WBR:
		return;

	case HTML_TAG_BR:
		html_layout_flush_space(ctx);
		html_layout_new_line(ctx, html_layout_line_h(style.scale));
		return;

	case HTML_TAG_HR:
		if (!ctx->at_line_start) {
			html_layout_new_line(ctx, 0);
		}
		html_layout_block_space(ctx, 6);
		html_layout_flush_space(ctx);
		html_layout_add_box(ctx, style.indent_x, ctx->cursor_y + 4,
		    ctx->max_width - style.indent_x, 1, HTML_COLOR_RULE,
		    HTML_BOX_HLINE);
		ctx->cursor_y += 10;
		ctx->cursor_x = ctx->margin_left;
		ctx->at_line_start = 1;
		html_layout_block_space(ctx, 6);
		return;

	case HTML_TAG_IMG:
		/*
		 * No decoders here - LibHtml is markup only.  alt text keeps
		 * the surrounding sentence readable; src is the last resort so
		 * a broken image still shows as something.
		 */
		attr = html_node_get_attr(node, "alt");
		if (attr == NULL || attr[0] == '\0') {
			attr = "[image]";
		}
		style.color = HTML_COLOR_MUTED;
		html_layout_format_text(ctx, attr, &style);
		return;

	case HTML_TAG_SVG:
		html_layout_svg_box(ctx, node, &style);
		return;

	case HTML_TAG_INPUT:
	case HTML_TAG_BUTTON:
	case HTML_TAG_TEXTAREA:
		html_layout_control(ctx, node, &style);
		return;

	case HTML_TAG_H1:
		is_block = 1;
		style.scale = HTML_SCALE_H1;
		style.bold = 1;
		space_before = 14;
		space_after = 12;
		break;
	case HTML_TAG_H2:
		is_block = 1;
		style.scale = HTML_SCALE_H2;
		style.bold = 1;
		space_before = 12;
		space_after = 10;
		break;
	case HTML_TAG_H3:
	case HTML_TAG_H4:
	case HTML_TAG_H5:
	case HTML_TAG_H6:
		is_block = 1;
		style.scale = HTML_SCALE_H3;
		style.bold = 1;
		space_before = 10;
		space_after = 8;
		break;

	case HTML_TAG_P:
		is_block = 1;
		space_before = 8;
		space_after = 8;
		break;

	case HTML_TAG_HTML:
	case HTML_TAG_BODY:
	case HTML_TAG_DIV:
	case HTML_TAG_ARTICLE:
	case HTML_TAG_SECTION:
	case HTML_TAG_NAV:
	case HTML_TAG_ASIDE:
	case HTML_TAG_HEADER:
	case HTML_TAG_FOOTER:
	case HTML_TAG_MAIN:
	case HTML_TAG_FIGURE:
	case HTML_TAG_FORM:
	case HTML_TAG_FIELDSET:
	case HTML_TAG_SELECT:
		is_block = 1;
		break;

	case HTML_TAG_FIGCAPTION:
	case HTML_TAG_ADDRESS:
		is_block = 1;
		style.color = HTML_COLOR_MUTED;
		space_after = 6;
		break;

	case HTML_TAG_LEGEND:
	case HTML_TAG_DT:
		is_block = 1;
		style.bold = 1;
		break;

	case HTML_TAG_DL:
		is_block = 1;
		space_before = 6;
		space_after = 6;
		break;
	case HTML_TAG_DD:
		is_block = 1;
		style.indent_x += HTML_INDENT_STEP;
		break;

	case HTML_TAG_OPTION:
		/* One choice per line so a <select> reads as a list. */
		is_block = 1;
		style.color = HTML_COLOR_MUTED;
		style.indent_x += HTML_INDENT_STEP;
		break;

	case HTML_TAG_UL:
	case HTML_TAG_OL:
		is_block = 1;
		style.indent_x += HTML_INDENT_STEP;
		space_before = 6;
		space_after = 6;
		if (ctx->list_depth < HTML_LIST_MAX_DEPTH) {
			int lvl = ctx->list_depth;
			ctx->list_ordered[lvl] = (node->tag == HTML_TAG_OL);
			ctx->list_index[lvl] = 1;
			if (node->tag == HTML_TAG_OL) {
				int32_t start = html_layout_span_attr(node,
				    "start", 100000);
				if (start > 0) {
					ctx->list_index[lvl] = start;
				}
			}
			ctx->list_depth++;
			pushed_list = 1;
		}
		break;
	case HTML_TAG_LI:
		is_block = 1;
		break;

	case HTML_TAG_BLOCKQUOTE:
		is_block = 1;
		style.indent_x += HTML_INDENT_STEP * 2;
		space_before = 6;
		space_after = 6;
		break;

	case HTML_TAG_PRE:
		is_block = 1;
		style.is_pre = 1;
		space_before = 6;
		space_after = 6;
		break;

	case HTML_TAG_TABLE:
		is_table = 1;
		space_before = 6;
		space_after = 6;
		break;
	case HTML_TAG_TR:
	case HTML_TAG_THEAD:
	case HTML_TAG_TBODY:
	case HTML_TAG_TFOOT:
	case HTML_TAG_CAPTION:
		/*
		 * Reached only for table parts outside a <table> - the table
		 * walker consumes them otherwise.  Laid out as plain blocks so
		 * the text still appears.
		 */
		is_block = 1;
		break;
	case HTML_TAG_TD:
		is_block = 1;
		break;
	case HTML_TAG_TH:
		is_block = 1;
		style.bold = 1;
		break;

	case HTML_TAG_CENTER:
		is_block = 1;
		centered = 1;
		break;

	case HTML_TAG_A:
		attr = html_node_get_attr(node, "href");
		if (attr != NULL && attr[0] != '\0') {
			style.href = attr;
			style.color = HTML_COLOR_LINK;
			style.underline = 1;
		}
		break;

	case HTML_TAG_B:
	case HTML_TAG_STRONG:
		style.bold = 1;
		break;
	case HTML_TAG_I:
	case HTML_TAG_EM:
	case HTML_TAG_CITE:
	case HTML_TAG_VAR:
		/* No italic face in a 5x7 bitmap font; underline stands in. */
		style.underline = 1;
		break;
	case HTML_TAG_U:
	case HTML_TAG_INS:
		style.underline = 1;
		break;
	case HTML_TAG_DEL:
		style.strike = 1;
		break;
	case HTML_TAG_MARK:
		style.marked = 1;
		style.mark_color = HTML_COLOR_MARK;
		break;
	case HTML_TAG_SMALL:
		style.scale = HTML_SCALE_SMALL;
		break;
	case HTML_TAG_BIG:
		if (style.scale < HTML_SCALE_H1) {
			style.scale++;
		}
		break;
	case HTML_TAG_SUB:
		style.scale = HTML_SCALE_SMALL;
		style.voffset += HTML_SUBSUP_SHIFT;
		break;
	case HTML_TAG_SUP:
		style.scale = HTML_SCALE_SMALL;
		style.voffset -= HTML_SUBSUP_SHIFT;
		break;
	case HTML_TAG_CODE:
	case HTML_TAG_KBD:
	case HTML_TAG_SAMP:
	case HTML_TAG_TT:
		/*
		 * The font is already fixed-pitch, so inline code only needs a
		 * colour cue.  is_pre is deliberately not set: that would keep
		 * source newlines and break wrapping mid-sentence.
		 */
		style.color = HTML_COLOR_MUTED;
		break;

	default:
		/* SPAN, FONT, LABEL, ABBR, Q, TIME, EMBED, UNKNOWN: inline. */
		break;
	}

	attr = html_node_get_attr(node, "align");
	if (attr != NULL && strcasecmp(attr, "center") == 0) {
		centered = 1;
	}

	if (is_table) {
		html_layout_block_space(ctx, space_before);
		ctx->depth++;
		html_layout_table(ctx, node, style);
		ctx->depth--;
		html_layout_block_space(ctx, space_after);
		return;
	}

	if (is_block) {
		if (!ctx->at_line_start) {
			html_layout_new_line(ctx, 0);
		}
		html_layout_block_space(ctx, space_before);
		ctx->margin_left = style.indent_x;
		ctx->cursor_x = ctx->margin_left;
		if (node->tag == HTML_TAG_LI) {
			html_layout_list_marker(ctx, &style);
			if (ctx->list_depth > 0) {
				ctx->list_index[ctx->list_depth - 1]++;
			}
		}
	}

	/*
	 * Both deferred effects need the position after pending space is
	 * applied, so flush now: the quote bar would otherwise start above its
	 * first line, and centering keys on line y values.
	 */
	if (centered || node->tag == HTML_TAG_BLOCKQUOTE) {
		html_layout_flush_space(ctx);
	}
	quote_top = ctx->cursor_y;
	mark_line = ctx->layout->lines_tail;
	mark_link = ctx->layout->links_tail;

	ctx->depth++;
	html_layout_children(ctx, node, style);
	ctx->depth--;

	if (is_block && !ctx->at_line_start) {
		html_layout_new_line(ctx, 0);
	}

	if (centered) {
		html_layout_center_range(
		    (mark_line != NULL) ? mark_line->next :
		    ctx->layout->lines,
		    (mark_link != NULL) ? mark_link->next :
		    ctx->layout->links,
		    style.indent_x, ctx->max_width);
	}

	if (node->tag == HTML_TAG_BLOCKQUOTE && ctx->cursor_y > quote_top) {
		int32_t bar_x = style.indent_x - HTML_INDENT_STEP / 2;
		if (bar_x < 0) {
			bar_x = 0;
		}
		html_layout_add_box(ctx, bar_x, quote_top, 2,
		    ctx->cursor_y - quote_top, HTML_COLOR_QUOTE_BAR,
		    HTML_BOX_FILL);
	}

	if (pushed_list) {
		ctx->list_depth--;
	}
	if (is_block) {
		ctx->margin_left = cur_style.indent_x;
		ctx->cursor_x = ctx->margin_left;
		html_layout_block_space(ctx, space_after);
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
	ctx.max_width = max_width - HTML_MARGIN_LEFT / 2;
	ctx.cursor_x = HTML_MARGIN_LEFT;
	ctx.cursor_y = HTML_MARGIN_LEFT;
	ctx.margin_left = HTML_MARGIN_LEFT;
	ctx.at_line_start = 1;

	memset(&initial_style, 0, sizeof(initial_style));
	initial_style.scale = HTML_SCALE_BASE;
	initial_style.color = HTML_COLOR_TEXT;
	initial_style.mark_color = HTML_COLOR_MARK;
	initial_style.indent_x = HTML_MARGIN_LEFT;

	/*
	 * Lay out <body> when the tree has one; otherwise the root, which is
	 * what a fragment without <html>/<body> parses to.  Searching only the
	 * root's children is enough - the parser never nests body deeper.
	 */
	body_node = html_node_find(doc->root, HTML_TAG_BODY);
	if (body_node == NULL) {
		body_node = doc->root;
	}

	html_layout_node(&ctx, body_node, initial_style);

	if (!ctx.at_line_start) {
		ctx.cursor_y += (ctx.line_h > 0) ? ctx.line_h :
		    HTML_LINE_H_BASE;
	}
	if (ctx.cursor_y + HTML_MARGIN_LEFT > layout->content_height) {
		layout->content_height = ctx.cursor_y + HTML_MARGIN_LEFT;
	}
	if (layout->content_width < max_width) {
		layout->content_width = max_width;
	}
	return (layout);
}

void
html_layout_free(html_layout_t *layout)
{
	html_layout_line_t	*line, *next_line;
	html_link_box_t		*link, *next_link;
	html_layout_box_t	*box, *next_box;

	if (layout == NULL) {
		return;
	}

	line = layout->lines;
	while (line != NULL) {
		next_line = line->next;
		free(line->text);
		free(line->href);
		free(line);
		line = next_line;
	}

	link = layout->links;
	while (link != NULL) {
		next_link = link->next;
		free(link->href);
		free(link);
		link = next_link;
	}

	box = layout->boxes;
	while (box != NULL) {
		next_box = box->next;
		free(box->ref);
		free(box);
		box = next_box;
	}

	free(layout);
}
