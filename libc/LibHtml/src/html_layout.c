/* !DEFINES!

$define %type html_layout_ctx as layout builder context
$define %func html_layout_create as function with args html_doc *, int32_t, int32_t
$define %func html_layout_create_ex as function with args html_doc *, int32_t, int32_t, html_image_size_fn, void *
$define %func html_layout_free as procedure with args html_layout *
$define %func html_layout_ctrl_at as function with args const html_layout *, int32_t, int32_t
$define %func html_node_form as function with args const html_node *
$define %func html_form_submit_url as function with args const html_node *, const html_node *, char *, size_t

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
$space %internal html_layout_svg_box, html_layout_img_box
$space %internal html_layout_len_attr
$space %internal css_iface_tag, css_iface_attr, css_iface_parent, html_css_iface
$space %internal css_iface_prev
$space %internal html_layout_apply_css, html_layout_apply_css_block
$space %internal html_layout_add_ctrl, html_ctrl_classify
$space %internal html_form_encode, html_form_field_included
$space %internal html_form_field_value
$space %export html_layout_create, html_layout_create_ex
$space %export html_layout_free, html_layout_ctrl_at
$space %export html_node_form, html_form_submit_url

*/

#include <ctype.h>
#include <css.h>
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

/*
 * Floor on the line advance, which is also what body text uses.  Equals
 * (HTML_GLYPH_HEIGHT + HTML_GLYPH_LEADING) * HTML_SCALE_BASE - keep it that way
 * if the base scale changes, or paragraphs and headings stop sharing a rhythm.
 */
#define HTML_LINE_H_BASE	18

#define HTML_INDENT_STEP	20
#define HTML_MARGIN_LEFT	16

/*
 * LibG's font is a 5x7 bitmap advanced 6px per glyph at scale 1, and LibG steps
 * its own wrapped text by (7 + 2) * scale.  Kept here as named constants: if
 * LibG ever changes glyph metrics, every width in this file is wrong and the
 * symptom is text overrunning the right margin, while a wrong leading shows as
 * consecutive lines drawn through each other.  These must track
 * LIBG_FONT_HEIGHT and the +2 in libg.c's text stepping.
 */
#define HTML_GLYPH_ADVANCE	6
#define HTML_GLYPH_HEIGHT	7
#define HTML_GLYPH_LEADING	2

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

/*
 * Default form control chrome, used only when the stylesheet says nothing.
 * GHOST is the fallback frame drawn when a sheet removes both border and
 * background - without it such a control is invisible and unclickable-looking.
 */
#define HTML_COLOR_CTRL_BORDER	0xFF8C8880
#define HTML_COLOR_CTRL_GHOST	0xFFD0CCC6
#define HTML_COLOR_BUTTON_FACE	0xFFE4E0DA
#define HTML_COLOR_FIELD_FACE	0xFFFFFFFF
#define HTML_COLOR_CTRL_HINT	0xFF8C8880
#define HTML_COLOR_CARET	0xFF202020

/*
 * Form control geometry.  Defaults only: CSS width/height/padding override
 * every one of them when present.
 */
#define HTML_CTRL_PAD_X		4
#define HTML_CTRL_PAD_Y		2
#define HTML_CTRL_MIN_W		16
#define HTML_CTRL_MAX_H		400
#define HTML_CTRL_MAX_BORDER	8
#define HTML_CTRL_TICK_W	13
#define HTML_CTRL_DEFAULT_COLS	20

/*
 * Form submission bounds.  FIELDS is a node-visit ceiling for the subtree walk,
 * not a field count limit per se - it stops a malformed tree with a sibling
 * cycle from building a query string until the output buffer fills.
 */
#define HTML_FORM_MAX_FIELDS	4096
#define HTML_FORM_FIELD_MAX	1024

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
	html_layout_t		*layout;
	const void		*css_sheet;
	html_image_size_fn	 image_size;
	void			*image_size_user;
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
	/*
	 * Horizontal space owed before the next inline item.  Whitespace cannot
	 * be applied where it is found, because a text node often ends with it
	 * and the next word belongs to a following element: "Click <a>here</a>"
	 * is two text nodes, and applying the space only when more text follows
	 * *in the same node* glued "Click" to "here".  Owed instead of applied
	 * so that a line break, which absorbs the space, can drop it.
	 */
	int		owed_space;
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

static html_ctrl_box_t *
html_layout_add_ctrl(html_layout_ctx_t *ctx, const html_node_t *node,
    int32_t x, int32_t y, int32_t w, int32_t h)
{
	html_ctrl_box_t	*c;

	if (ctx == NULL || node == NULL || w <= 0 || h <= 0) {
		return (NULL);
	}
	c = (html_ctrl_box_t *)malloc(sizeof(*c));
	if (c == NULL) {
		return (NULL);
	}
	memset(c, 0, sizeof(*c));
	c->rect.x = x;
	c->rect.y = y;
	c->rect.width = w;
	c->rect.height = h;
	/*
	 * The const is dropped deliberately.  Layout only reads the node, but a
	 * focused text field writes its value attribute through this pointer,
	 * and html_layout_create() takes a non-const doc precisely so that the
	 * cast here is honest rather than a lie about ownership.
	 */
	c->node = (html_node_t *)node;

	if (ctx->layout->ctrls_tail != NULL) {
		ctx->layout->ctrls_tail->next = c;
	} else {
		ctx->layout->ctrls = c;
	}
	ctx->layout->ctrls_tail = c;

	if (x + w > ctx->layout->content_width) {
		ctx->layout->content_width = x + w;
	}
	if (y + h > ctx->layout->content_height) {
		ctx->layout->content_height = y + h;
	}
	return (c);
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

/*
 * Line advance for a scale, taken from the font's own metrics rather than a
 * table of buckets.  LibG draws a glyph HTML_GLYPH_HEIGHT * scale tall and
 * steps its own multi-line text by (HTML_GLYPH_HEIGHT + 2) * scale, so that is
 * the smallest advance that cannot overlap.
 *
 * The old three-bucket table approximated this and drifted: it returned 28 for
 * every scale >= 4, which is exactly the glyph height at scale 4 (leading zero,
 * glyphs touching) and 7px short of it at scale 5.  A CSS font-size of 32-36px
 * maps to scale 5, so any page styling its heading in that range - Google's own
 * error pages among them - drew the second line of a wrapped heading through
 * the first.
 *
 * Body text keeps HTML_LINE_H_BASE: at scale 1 the 5x7 cell is far shorter than
 * the line, and that extra leading is what makes a paragraph readable instead
 * of cramped.  From scale 3 up the font metric is larger anyway and wins.
 */
static int32_t
html_layout_line_h(uint32_t scale)
{
	int32_t	advance;

	if (scale == 0) {
		scale = 1;
	}
	advance = (HTML_GLYPH_HEIGHT + HTML_GLYPH_LEADING) * (int32_t)scale;
	if (advance < HTML_LINE_H_BASE) {
		advance = HTML_LINE_H_BASE;
	}
	return (advance);
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
	ctx->owed_space = 0;	/* the break itself separates the words */
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
	/* Preformatted runs carry their own spacing; an owed one would shift
	 * the first column and break the alignment that <pre> exists for. */
	ctx->owed_space = 0;

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
	int32_t		advance, th, space_w, space_h, tw, sp;
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
			 * otherwise indent the line by one glyph.  Recorded
			 * rather than applied, so that a space ending this text
			 * node still separates it from the next element's text.
			 */
			if (!ctx->at_line_start) {
				ctx->owed_space = 1;
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

			/*
			 * The owed space counts against the line width, and the
			 * wrap discards it - hence measure, test, then apply,
			 * in that order.  new_line() clears the flag.
			 */
			sp = (ctx->owed_space != 0 && ctx->at_line_start == 0) ?
			    space_w : 0;
			if (!ctx->at_line_start &&
			    ctx->cursor_x + sp + tw > ctx->max_width) {
				html_layout_new_line(ctx, th);
				sp = 0;
			}
			ctx->cursor_x += sp;
			ctx->owed_space = 0;
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
 * <center>, align=center and text-align:center are resolved after the fact: the
 * block is laid out left-aligned, then every line it produced is moved so its
 * row's midpoint sits at the midpoint of [left, right).  Rows are keyed by y,
 * which is why the row table is bounded - a pathological block just stays
 * left-aligned instead of costing O(n^2).
 *
 * The shift is computed from the row's current extent, not from its right edge,
 * so running this twice over the same rows is a no-op.  That matters because
 * text-align inherits: a centred wrapper marks every nested block centred too,
 * and the old "shift right by half the slack" form applied at each level, which
 * walked the content towards the right edge one nesting level at a time.
 */
static void
html_layout_center_range(html_layout_line_t *first,
    html_link_box_t *first_link, html_ctrl_box_t *first_ctrl, int32_t left,
    int32_t right)
{
	int32_t			row_y[HTML_CENTER_MAX_ROWS];
	int32_t			row_min[HTML_CENTER_MAX_ROWS];
	int32_t			row_shift[HTML_CENTER_MAX_ROWS];
	html_layout_line_t	*line;
	html_link_box_t		*link;
	html_ctrl_box_t		*ctrl;
	int32_t			end;
	int			rows, i, seen;

	if (right - left <= 0) {
		return;
	}
	if (first == NULL && first_ctrl == NULL) {
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
			row_min[rows] = line->x;
			row_shift[rows] = end;
			rows++;
		} else {
			if (end > row_shift[i]) {
				row_shift[i] = end;
			}
			if (line->x < row_min[i]) {
				row_min[i] = line->x;
			}
		}
	}

	/*
	 * Controls contribute their own rows.  A centred block containing only
	 * a button produces no text line at all, so without this pass its row
	 * is unknown and the button stays hard left while its caption centres.
	 */
	for (ctrl = first_ctrl; ctrl != NULL; ctrl = ctrl->next) {
		if (++seen > HTML_CENTER_MAX_LINES) {
			return;
		}
		end = ctrl->rect.x + ctrl->rect.width;
		for (i = 0; i < rows; i++) {
			if (row_y[i] == ctrl->rect.y) {
				break;
			}
		}
		if (i == rows) {
			if (rows == HTML_CENTER_MAX_ROWS) {
				return;
			}
			row_y[rows] = ctrl->rect.y;
			row_min[rows] = ctrl->rect.x;
			row_shift[rows] = end;
			rows++;
		} else {
			if (end > row_shift[i]) {
				row_shift[i] = end;
			}
			if (ctrl->rect.x < row_min[i]) {
				row_min[i] = ctrl->rect.x;
			}
		}
	}

	/*
	 * row_min/row_shift hold the row's extent; convert to the delta that
	 * puts its midpoint on the container's.  Clamped at 0 so a row wider
	 * than the container stays put rather than sliding off to the left,
	 * where its start would be unreachable - there is no horizontal scroll.
	 */
	for (i = 0; i < rows; i++) {
		int32_t	delta = ((left + right) - (row_min[i] +
			    row_shift[i])) / 2;

		row_shift[i] = (delta > 0) ? delta : 0;
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
	for (ctrl = first_ctrl; ctrl != NULL; ctrl = ctrl->next) {
		for (i = 0; i < rows; i++) {
			if (row_y[i] == ctrl->rect.y) {
				ctrl->rect.x += row_shift[i];
				break;
			}
		}
	}
}

/* Maps tag + type= onto what the control does when clicked. */
static html_ctrl_kind_t
html_ctrl_classify(const html_node_t *node, const char *type)
{
	if (node->tag == HTML_TAG_TEXTAREA) {
		return (HTML_CTRL_TEXTAREA);
	}
	if (node->tag == HTML_TAG_SELECT) {
		return (HTML_CTRL_SELECT);
	}
	if (node->tag == HTML_TAG_BUTTON) {
		/*
		 * A <button> with no type= submits.  Getting this backwards is
		 * why a search box with a magnifier button does nothing.
		 */
		if (type == NULL || strcasecmp(type, "submit") == 0) {
			return (HTML_CTRL_SUBMIT);
		}
		return (HTML_CTRL_BUTTON);
	}
	if (type == NULL) {
		return (HTML_CTRL_TEXT);	/* <input> defaults to text */
	}
	if (strcasecmp(type, "submit") == 0 || strcasecmp(type, "image") == 0) {
		return (HTML_CTRL_SUBMIT);
	}
	if (strcasecmp(type, "button") == 0 || strcasecmp(type, "reset") == 0) {
		return (HTML_CTRL_BUTTON);
	}
	if (strcasecmp(type, "checkbox") == 0) {
		return (HTML_CTRL_CHECKBOX);
	}
	if (strcasecmp(type, "radio") == 0) {
		return (HTML_CTRL_RADIO);
	}
	return (HTML_CTRL_TEXT);
}

/*
 * Emits one interactive control.
 *
 * Unlike every other box here, a control's geometry comes from CSS when CSS
 * supplied it: a search field styled `width: 100%; padding: 12px` has to end up
 * that size or the page reads as a row of tiny empty strokes.  When CSS says
 * nothing, the fallback is the label's own width, which is what a plain
 * unstyled form wants.
 *
 * `cc` may be NULL (no stylesheet matched).
 */
static void
html_layout_control(html_layout_ctx_t *ctx, const html_node_t *node,
    const html_style_state_t *style, const css_computed_t *cc)
{
	html_style_state_t	inner;
	html_ctrl_box_t		*ctrl;
	html_ctrl_kind_t	kind;
	const char		*label, *type;
	char			*owned;
	int32_t			bw, avail, box_w, box_h, text_w, line_h;
	int32_t			pl, pr, pt, pb;
	size_t			nchars;

	type = html_node_get_attr(node, "type");
	kind = html_ctrl_classify(node, type);

	/*
	 * type=hidden carries form data but has no box.  It is still collected
	 * by html_form_submit_url(), which walks the DOM, not this list.
	 */
	if (type != NULL && strcasecmp(type, "hidden") == 0) {
		return;
	}

	owned = NULL;
	label = NULL;
	if (kind == HTML_CTRL_BUTTON || kind == HTML_CTRL_SUBMIT) {
		label = html_node_get_attr(node, "value");
		if (label == NULL || label[0] == '\0') {
			label = html_node_get_attr(node, "alt");
		}
		if ((label == NULL || label[0] == '\0') &&
		    node->tag != HTML_TAG_INPUT) {
			owned = html_node_text(node);
			label = owned;
		}
		if (label == NULL || label[0] == '\0') {
			/*
			 * aria-label is the last resort and it matters: an
			 * icon-only submit button has no text anywhere else,
			 * and an empty face is unclickable-looking.
			 */
			label = html_node_get_attr(node, "aria-label");
		}
		if (label == NULL || label[0] == '\0') {
			label = (kind == HTML_CTRL_SUBMIT) ? "Go" : "...";
			if (owned != NULL) {
				free(owned);
				owned = NULL;
			}
		}
	}

	inner = *style;
	inner.underline = 0;
	inner.strike = 0;
	inner.marked = 0;
	inner.href = NULL;	/* a control inside <a> is not link text */
	if (cc != NULL && (cc->set & CSS_PROP_COLOR) != 0) {
		inner.color = cc->color;
	}

	line_h = html_layout_line_h(inner.scale);
	avail = ctx->max_width - ctx->margin_left;
	if (avail < HTML_CTRL_MIN_W) {
		avail = HTML_CTRL_MIN_W;
	}

	pl = HTML_CTRL_PAD_X;
	pr = HTML_CTRL_PAD_X;
	pt = HTML_CTRL_PAD_Y;
	pb = HTML_CTRL_PAD_Y;
	bw = 1;
	if (cc != NULL) {
		if ((cc->set & CSS_PROP_PADDING) != 0) {
			pl = cc->pad_left;
			pr = cc->pad_right;
			pt = cc->pad_top;
			pb = cc->pad_bottom;
		}
		if ((cc->set & CSS_PROP_BORDER) != 0) {
			bw = cc->border_width;
		}
	}
	if (bw > HTML_CTRL_MAX_BORDER) {
		bw = HTML_CTRL_MAX_BORDER;
	}

	/* Content width: CSS first, then the label, then a default field. */
	if (kind == HTML_CTRL_CHECKBOX || kind == HTML_CTRL_RADIO) {
		text_w = HTML_CTRL_TICK_W;
		pl = 0;
		pr = 0;
		pt = 0;
		pb = 0;
	} else if (label != NULL) {
		text_w = (int32_t)strlen(label) * (int32_t)inner.scale *
		    HTML_GLYPH_ADVANCE;
	} else {
		const char	*sz = html_node_get_attr(node, "size");

		nchars = HTML_CTRL_DEFAULT_COLS;
		if (sz != NULL) {
			int	v = atoi(sz);

			if (v > 0 && v < 200) {
				nchars = (size_t)v;
			}
		}
		text_w = (int32_t)nchars * (int32_t)inner.scale *
		    HTML_GLYPH_ADVANCE;
	}

	box_w = text_w + pl + pr + 2 * bw;
	if (cc != NULL && (cc->set & CSS_PROP_WIDTH) != 0) {
		int32_t	w = cc->width;

		if (cc->width_pct != 0) {
			w = avail * cc->width / 100;
		}
		/*
		 * CSS width is the content box here, matching the default
		 * box-sizing.  box-sizing: border-box is not tracked, so a
		 * bordered field comes out 2*bw wider than the author asked -
		 * visible only as a few pixels, and preferable to clipping the
		 * text by subtracting padding we may not have parsed.
		 */
		if (w > 0) {
			box_w = w + pl + pr + 2 * bw;
		}
	}
	if (box_w > avail) {
		box_w = avail;
	}
	if (box_w < HTML_CTRL_MIN_W) {
		box_w = HTML_CTRL_MIN_W;
	}

	box_h = line_h - 2 + pt + pb;
	if (cc != NULL && (cc->set & CSS_PROP_HEIGHT) != 0 &&
	    cc->height_pct == 0 && cc->height > 0) {
		/* Percentage heights need a containing height nothing here
		 * tracks, so only absolute ones are honoured. */
		box_h = cc->height + pt + pb;
	}
	if (box_h < line_h - 2) {
		box_h = line_h - 2;
	}
	if (box_h > HTML_CTRL_MAX_H) {
		box_h = HTML_CTRL_MAX_H;
	}
	if (kind == HTML_CTRL_CHECKBOX || kind == HTML_CTRL_RADIO) {
		box_h = HTML_CTRL_TICK_W;
		box_w = HTML_CTRL_TICK_W;
	}

	/*
	 * Same order as the word path: the owed space counts against the wrap
	 * test and is discarded by it.  Without this a label and its field are
	 * glued together, since the label's text node ends with the space.
	 */
	{
		int32_t	sp = (ctx->owed_space != 0 && ctx->at_line_start == 0) ?
			    (int32_t)inner.scale * HTML_GLYPH_ADVANCE : 0;

		if (ctx->at_line_start == 0 &&
		    ctx->cursor_x + sp + box_w > ctx->max_width) {
			html_layout_new_line(ctx, line_h);
			sp = 0;
		}
		ctx->cursor_x += sp;
		ctx->owed_space = 0;
	}
	html_layout_flush_space(ctx);

	ctrl = html_layout_add_ctrl(ctx, node, ctx->cursor_x, ctx->cursor_y,
	    box_w, box_h);
	if (ctrl == NULL) {
		if (owned != NULL) {
			free(owned);
		}
		return;
	}

	ctrl->kind = kind;
	ctrl->scale = inner.scale;
	ctrl->fg = inner.color;
	ctrl->pad_left = pl + bw;
	ctrl->pad_top = pt + bw;
	ctrl->border_width = bw;
	ctrl->disabled = (html_node_get_attr(node, "disabled") != NULL);
	ctrl->password = (type != NULL && strcasecmp(type, "password") == 0);
	if (label != NULL) {
		ctrl->label = strdup(label);
	}

	/*
	 * Default chrome when the sheet is silent.  A field needs a frame to be
	 * findable; a button needs a face, or it is indistinguishable from the
	 * text beside it.  Both are overridden the moment CSS says anything.
	 */
	ctrl->border = (bw > 0) ? HTML_COLOR_CTRL_BORDER : 0;
	ctrl->bg = (kind == HTML_CTRL_BUTTON || kind == HTML_CTRL_SUBMIT) ?
	    HTML_COLOR_BUTTON_FACE : HTML_COLOR_FIELD_FACE;

	if (cc != NULL) {
		if ((cc->set & CSS_PROP_BGCOLOR) != 0) {
			ctrl->bg = cc->bgcolor;
		}
		if ((cc->set & CSS_PROP_BORDER_COLOR) != 0) {
			ctrl->border = cc->border_color;
		}
		if ((cc->set & CSS_PROP_BORDER) != 0 && bw == 0) {
			ctrl->border = 0;
		}
	}

	/*
	 * Last-resort visibility.  A sheet that removes both the border and the
	 * fill leaves a control that is only findable by guessing - common on
	 * pages that draw their own chrome with a background image this
	 * renderer cannot fetch.  One faint frame is added back.
	 */
	if ((ctrl->bg >> 24) == 0 && (ctrl->border >> 24) == 0) {
		ctrl->border = HTML_COLOR_CTRL_GHOST;
		ctrl->border_width = 1;
	}

	ctx->cursor_x += box_w + HTML_GLYPH_ADVANCE;
	ctx->at_line_start = 0;
	if (box_h + 2 > ctx->line_h) {
		ctx->line_h = box_h + 2;
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
	int	save_at_start, save_owed;

	save_max = ctx->max_width;
	save_x = ctx->cursor_x;
	save_y = ctx->cursor_y;
	save_line_h = ctx->line_h;
	save_margin = ctx->margin_left;
	save_pending = ctx->pending_space;
	save_at_start = ctx->at_line_start;
	save_owed = ctx->owed_space;

	ctx->margin_left = x + HTML_TABLE_CELL_PAD;
	ctx->max_width = x + w - HTML_TABLE_CELL_PAD;
	ctx->cursor_x = ctx->margin_left;
	ctx->cursor_y = y + HTML_TABLE_CELL_PAD;
	ctx->line_h = 0;
	ctx->pending_space = 0;
	ctx->at_line_start = 1;
	ctx->owed_space = 0;

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
	ctx->owed_space = save_owed;

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
			ctx->owed_space = 0;
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
	ctx->owed_space = 0;
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
	ctx->owed_space = 0;
	html_layout_block_space(ctx, 6);
}
#define HTML_IMG_DEF_W		64
#define HTML_IMG_DEF_H		64
#define HTML_IMG_GAP		2

static void
html_layout_img_box(html_layout_ctx_t *ctx, const html_node_t *node,
    const html_style_state_t *style)
{
	html_layout_box_t	*box;
	const char		*src;
	const char		*alt;
	int32_t			 nat_w, nat_h;
	int32_t			 avail;
	int32_t			 w, h;
	int32_t			 sp;
	int			 have_nat;

	src = html_node_get_attr(node, "src");
	if (src == NULL || src[0] == '\0') {
		alt = html_node_get_attr(node, "alt");
		if (alt != NULL && alt[0] != '\0') {
			html_style_state_t	alt_style;

			alt_style = *style;
			alt_style.color = HTML_COLOR_MUTED;
			html_layout_format_text(ctx, alt, &alt_style);
		}
		return;
	}

	w = html_layout_len_attr(html_node_get_attr(node, "width"));
	h = html_layout_len_attr(html_node_get_attr(node, "height"));

	nat_w = 0;
	nat_h = 0;
	have_nat = 0;
	if (ctx->image_size != NULL && (w == 0 || h == 0)) {
		have_nat = (ctx->image_size(ctx->image_size_user, src,
		    &nat_w, &nat_h) == 0 && nat_w > 0 && nat_h > 0);
	}

	if (!have_nat && (w == 0 || h == 0)) {
		ctx->layout->images_estimated++;
	}

	if (w == 0 && h == 0) {
		if (have_nat) {
			w = nat_w;
			h = nat_h;
		} else {
			w = HTML_IMG_DEF_W;
			h = HTML_IMG_DEF_H;
		}
	} else if (w == 0) {
		w = have_nat ? (int32_t)(((int64_t)h * nat_w) / nat_h) : h;
	} else if (h == 0) {
		h = have_nat ? (int32_t)(((int64_t)w * nat_h) / nat_w) : w;
	}
	if (w <= 0) {
		w = HTML_IMG_DEF_W;
	}
	if (h <= 0) {
		h = HTML_IMG_DEF_H;
	}

	avail = ctx->max_width - style->indent_x;
	if (avail < HTML_IMG_GAP + 1) {
		avail = HTML_IMG_GAP + 1;
	}
	if (w > avail) {
		h = (int32_t)(((int64_t)h * avail) / w);
		w = avail;
		if (h <= 0) {
			h = 1;
		}
	}
	if (w > HTML_SVG_MAX_DIM) {
		w = HTML_SVG_MAX_DIM;
	}
	if (h > HTML_SVG_MAX_DIM) {
		h = HTML_SVG_MAX_DIM;
	}

	sp = (ctx->owed_space != 0 && ctx->at_line_start == 0) ?
	    (int32_t)style->scale * HTML_GLYPH_ADVANCE : 0;
	if (ctx->at_line_start == 0 && ctx->cursor_x + sp + w >
	    ctx->max_width) {
		html_layout_new_line(ctx, ctx->line_h);
		sp = 0;
	}
	ctx->cursor_x += sp;
	ctx->owed_space = 0;
	html_layout_flush_space(ctx);

	box = html_layout_add_box(ctx, ctx->cursor_x, ctx->cursor_y, w, h,
	    HTML_COLOR_MUTED, HTML_BOX_IMAGE);
	if (box != NULL) {
		box->ref = strdup(src);
	}

	if (style->href != NULL) {
		html_layout_add_link(ctx, style->href, ctx->cursor_x,
		    ctx->cursor_y, w, h);
	}

	ctx->cursor_x += w + HTML_GLYPH_ADVANCE;
	ctx->at_line_start = 0;
	if (h + HTML_IMG_GAP > ctx->line_h) {
		ctx->line_h = h + HTML_IMG_GAP;
	}
}

static const char *
css_iface_tag(const void *n)
{
	const html_node_t	*node = n;

	return ((node != NULL && node->tag_name != NULL) ?
	    node->tag_name : "");
}

static const char *
css_iface_attr(const void *n, const char *key)
{
	return (html_node_get_attr((const html_node_t *)n, key));
}

static const void *
css_iface_parent(const void *n)
{
	return (((const html_node_t *)n)->parent);
}

/*
 * Previous *element* sibling.  Text nodes are skipped here rather than in the
 * matcher: CSS has no concept of them, so "h1 + p" must match across the
 * whitespace between the two tags, which the parser keeps as a text node.
 */
static const void *
css_iface_prev(const void *n)
{
	const html_node_t	*p = ((const html_node_t *)n)->prev_sibling;

	while (p != NULL && p->tag == HTML_TAG_TEXT) {
		p = p->prev_sibling;
	}
	return (p);
}

static const css_node_iface_t html_css_iface = {
	css_iface_tag,
	css_iface_attr,
	css_iface_parent,
	css_iface_prev
};

static void
html_layout_apply_css(html_style_state_t *style, const css_computed_t *cc)
{
	if ((cc->set & CSS_PROP_COLOR) != 0) {
		style->color = cc->color;
	}
	if ((cc->set & CSS_PROP_SCALE) != 0 && cc->font_scale >= 1 &&
	    cc->font_scale <= 6) {
		style->scale = (uint32_t)cc->font_scale;
	}
	if ((cc->set & CSS_PROP_BOLD) != 0) {
		style->bold = cc->bold;
	}
	if ((cc->set & CSS_PROP_UNDERLINE) != 0) {
		style->underline = cc->underline;
	}
	if ((cc->set & CSS_PROP_STRIKE) != 0) {
		style->strike = cc->strike;
	}
}

static void
html_layout_apply_css_block(html_layout_ctx_t *ctx, const html_node_t *node,
    const css_computed_t *cc, int32_t *space_before,
    int32_t *space_after, int *centered)
{
	if ((cc->set & CSS_PROP_BGCOLOR) != 0 &&
	    ctx->layout->has_page_bg == 0 &&
	    (node->tag == HTML_TAG_BODY || node->tag == HTML_TAG_HTML)) {
		ctx->layout->page_bg = cc->bgcolor | 0xff000000u;
		ctx->layout->has_page_bg = 1;
	}
	if ((cc->set & CSS_PROP_ALIGN_CENTER) != 0 && cc->align_center != 0) {
		*centered = 1;
	}
	if ((cc->set & CSS_PROP_MARGIN_TOP) != 0 &&
	    cc->margin_top > *space_before) {
		*space_before = cc->margin_top;
	}
	if ((cc->set & CSS_PROP_MARGIN_BOTTOM) != 0 &&
	    cc->margin_bottom > *space_after) {
		*space_after = cc->margin_bottom;
	}
}

static void
html_layout_node(html_layout_ctx_t *ctx, const html_node_t *node,
    html_style_state_t cur_style)
{
	html_style_state_t	style;
	html_layout_line_t	*mark_line;
	html_link_box_t		*mark_link;
	html_ctrl_box_t		*mark_ctrl;
	html_layout_box_t	*bg_box;
	const char		*attr;
	css_computed_t		cc;
	int32_t			space_before, space_after, quote_top;
	int32_t			box_w, box_h, saved_max_width;
	uint32_t		css_bg_color;
	int			have_css;
	int			css_bg;
	int			is_block, is_table, centered, pushed_list;

	if (node == NULL || ctx->depth >= HTML_LAYOUT_MAX_DEPTH) {
		return;
	}

	if (node->tag == HTML_TAG_TEXT) {
		html_layout_format_text(ctx, node->text, &cur_style);
		return;
	}

	have_css = 0;
	css_bg = 0;
	css_bg_color = 0;
	if (ctx->css_sheet != NULL &&
	    css_compute((const css_sheet_t *)ctx->css_sheet,
	    &html_css_iface, node, &cc) == 0) {
		/*
		 * The value matters, not just the bit: the bit is set by any
		 * `display` declaration at all.  Testing it alone dropped every
		 * element carrying `display: block` or `display: flex`, which on
		 * a stylesheet-driven page is most of the document.
		 */
		if ((cc.set & CSS_PROP_DISPLAY_NONE) != 0 &&
		    cc.display_none != 0) {
			return;
		}
		have_css = 1;
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
	 * text for style/script/template; dropping the elements here keeps
	 * stray attributes and <title> out of the page body.
	 *
	 * NOSCRIPT is not in this list: nothing here runs scripts, so its
	 * children are live content and must be walked like any other element.
	 */
	case HTML_TAG_HEAD:
	case HTML_TAG_TITLE:
	case HTML_TAG_STYLE:
	case HTML_TAG_SCRIPT:
	case HTML_TAG_TEMPLATE:
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
		ctx->owed_space = 0;
		html_layout_block_space(ctx, 6);
		return;

	case HTML_TAG_IMG:
	
		html_layout_img_box(ctx, node, &style);
		return;

	case HTML_TAG_SVG:
		html_layout_svg_box(ctx, node, &style);
		return;

	case HTML_TAG_INPUT:
	case HTML_TAG_BUTTON:
	case HTML_TAG_TEXTAREA:
		html_layout_control(ctx, node, &style,
		    (have_css != 0) ? &cc : NULL);
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

	if (have_css != 0) {
		html_layout_apply_css(&style, &cc);
		html_layout_apply_css_block(ctx, node, &cc, &space_before,
		    &space_after, &centered);
	}

	attr = html_node_get_attr(node, "align");
	if (attr != NULL && strcasecmp(attr, "center") == 0) {
		centered = 1;
	}

	if (have_css != 0 && (cc.set & CSS_PROP_BGCOLOR) != 0 &&
	    node->tag != HTML_TAG_BODY && node->tag != HTML_TAG_HTML) {
		css_bg = 1;
		css_bg_color = cc.bgcolor | 0xff000000u;
	}

	box_w = 0;
	box_h = 0;
	if (have_css != 0) {
		if ((cc.set & CSS_PROP_WIDTH) != 0 && cc.width > 0) {
			box_w = cc.width_pct ?
			    (ctx->max_width - style.indent_x) * cc.width / 100 :
			    cc.width;
		}
		if ((cc.set & CSS_PROP_HEIGHT) != 0 && cc.height > 0 &&
		    cc.height_pct == 0) {
			box_h = cc.height;
		}
		if (box_w > HTML_SVG_MAX_DIM) {
			box_w = HTML_SVG_MAX_DIM;
		}
		if (box_h > HTML_SVG_MAX_DIM) {
			box_h = HTML_SVG_MAX_DIM;
		}
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


	if (centered || node->tag == HTML_TAG_BLOCKQUOTE || css_bg != 0 ||
	    box_h > 0) {
		html_layout_flush_space(ctx);
	}
	quote_top = ctx->cursor_y;
	mark_line = ctx->layout->lines_tail;
	mark_link = ctx->layout->links_tail;
	mark_ctrl = ctx->layout->ctrls_tail;

	bg_box = NULL;
	if (css_bg != 0) {
		int32_t	bw = (box_w > 0) ? box_w :
			    ctx->max_width - style.indent_x;

		if (bw > 0) {
			bg_box = html_layout_add_box(ctx, style.indent_x,
			    quote_top, bw, 0, css_bg_color, HTML_BOX_FILL);
		}
	}


	saved_max_width = ctx->max_width;
	if (box_w > 0 && style.indent_x + box_w < ctx->max_width) {
		ctx->max_width = style.indent_x + box_w;
	}

	ctx->depth++;
	html_layout_children(ctx, node, style);
	ctx->depth--;

	ctx->max_width = saved_max_width;

	if (is_block && !ctx->at_line_start) {
		html_layout_new_line(ctx, 0);
	}


	if (box_h > 0) {
		ctx->cursor_y = quote_top + box_h;
		ctx->at_line_start = 1;
		ctx->line_h = 0;
		if (ctx->cursor_y > ctx->layout->content_height) {
			ctx->layout->content_height = ctx->cursor_y;
		}
	}

	if (bg_box != NULL) {
		bg_box->rect.height = (box_h > 0) ? box_h :
		    ctx->cursor_y - quote_top;

		if (bg_box->rect.y + bg_box->rect.height >
		    ctx->layout->content_height) {
			ctx->layout->content_height = bg_box->rect.y +
			    bg_box->rect.height;
		}
	}

	if (centered) {
		html_layout_center_range(
		    (mark_line != NULL) ? mark_line->next :
		    ctx->layout->lines,
		    (mark_link != NULL) ? mark_link->next :
		    ctx->layout->links,
		    (mark_ctrl != NULL) ? mark_ctrl->next :
		    ctx->layout->ctrls,
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
html_layout_create(html_doc_t *doc, int32_t viewport_w, int32_t viewport_h)
{
	return (html_layout_create_ex(doc, viewport_w, viewport_h, NULL,
	    NULL));
}

html_layout_t *
html_layout_create_ex(html_doc_t *doc, int32_t viewport_w, int32_t viewport_h,
    html_image_size_fn size, void *size_user)
{
	html_layout_t		*layout;
	html_layout_ctx_t	ctx;
	html_style_state_t	initial_style;
	const html_node_t	*body_node;
	int32_t			max_width;

	if (doc == NULL || doc->root == NULL) {
		return (NULL);
	}
	max_width = viewport_w;
	if (max_width < 100) {
		max_width = 600;
	}
	if (viewport_h < 100) {
		viewport_h = 800;
	}

	layout = (html_layout_t *)malloc(sizeof(html_layout_t));
	if (layout == NULL) {
		return (NULL);
	}
	memset(layout, 0, sizeof(html_layout_t));

	memset(&ctx, 0, sizeof(ctx));
	ctx.layout = layout;
	ctx.image_size = size;
	ctx.image_size_user = size_user;
	ctx.max_width = max_width - HTML_MARGIN_LEFT / 2;
	ctx.cursor_x = HTML_MARGIN_LEFT;
	ctx.cursor_y = HTML_MARGIN_LEFT;
	ctx.margin_left = HTML_MARGIN_LEFT;
	ctx.at_line_start = 1;

	if (doc->stylesheet != NULL) {
		css_sheet_t	*sheet = NULL;

		/*
		 * The real viewport goes in so @media queries resolve against
		 * the window the page is actually being laid out for.  Passing
		 * the wrong width here is worse than having no media support:
		 * the narrow-screen rules would win on a wide window.
		 */
		if (css_parse_ex(doc->stylesheet, max_width, viewport_h,
		    &sheet) == 0 && sheet != NULL) {
			layout->css_sheet = sheet;
			ctx.css_sheet = sheet;
		}
	}

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
	html_ctrl_box_t		*ctrl, *next_ctrl;

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

	/*
	 * ctrl->node is not freed here: it points into the html_doc_t, which
	 * outlives the layout and is freed separately.  Freeing it would double
	 * free the whole DOM on the next navigation.
	 */
	ctrl = layout->ctrls;
	while (ctrl != NULL) {
		next_ctrl = ctrl->next;
		free(ctrl->label);
		free(ctrl);
		ctrl = next_ctrl;
	}
	layout->focus = NULL;

	if (layout->css_sheet != NULL) {
		css_free((css_sheet_t *)layout->css_sheet);
	}

	free(layout);
}

html_ctrl_box_t *
html_layout_ctrl_at(const html_layout_t *layout, int32_t doc_x, int32_t doc_y)
{
	html_ctrl_box_t	*c, *hit;

	if (layout == NULL) {
		return (NULL);
	}

	/*
	 * Last match wins, not first.  Controls are appended in document order
	 * and a later one is painted over an earlier one, so on the rare
	 * overlap the visible control is the one that must take the click.
	 */
	hit = NULL;
	for (c = layout->ctrls; c != NULL; c = c->next) {
		if (doc_x >= c->rect.x &&
		    doc_x < c->rect.x + c->rect.width &&
		    doc_y >= c->rect.y &&
		    doc_y < c->rect.y + c->rect.height) {
			hit = c;
		}
	}
	return (hit);
}

const html_node_t *
html_node_form(const html_node_t *node)
{
	const html_node_t	*a;
	int			guard;

	guard = HTML_LAYOUT_MAX_DEPTH * 2;
	for (a = node; a != NULL && guard-- > 0; a = a->parent) {
		if (a->tag == HTML_TAG_FORM) {
			return (a);
		}
	}
	return (NULL);
}

/*
 * Percent-encodes one field name or value into `out`.
 *
 * Everything outside the RFC 3986 unreserved set is escaped, and space becomes
 * '+' as application/x-www-form-urlencoded requires.  Encoding conservatively
 * matters more than encoding minimally: a raw '&' or '=' in a value silently
 * turns into an extra parameter, and the server sees a different query than
 * the user typed.
 *
 * Returns bytes written, or (size_t)-1 if the result would not fit.
 */
static size_t
html_form_encode(char *out, size_t max_out, const char *src)
{
	static const char	hex[] = "0123456789ABCDEF";
	size_t			n;

	n = 0;
	for (; *src != '\0'; src++) {
		unsigned char	c = (unsigned char)*src;

		if (isalnum(c) != 0 || c == '-' || c == '_' || c == '.' ||
		    c == '~') {
			if (n + 1 >= max_out) {
				return ((size_t)-1);
			}
			out[n++] = (char)c;
		} else if (c == ' ') {
			if (n + 1 >= max_out) {
				return ((size_t)-1);
			}
			out[n++] = '+';
		} else {
			if (n + 3 >= max_out) {
				return ((size_t)-1);
			}
			out[n++] = '%';
			out[n++] = hex[(c >> 4) & 0xf];
			out[n++] = hex[c & 0xf];
		}
	}
	out[n] = '\0';
	return (n);
}

/*
 * True when a control contributes a name=value pair to the submission.
 *
 * The rules that matter here: unnamed and disabled controls are skipped, an
 * unchecked checkbox or radio contributes nothing, and a submit button
 * contributes only if it is the one that was clicked - otherwise every button
 * on the form would be sent at once.
 */
static int
html_form_field_included(const html_node_t *node, const html_node_t *submitter)
{
	const char	*type;
	html_ctrl_kind_t kind;

	if (html_node_get_attr(node, "name") == NULL) {
		return (0);
	}
	if (html_node_get_attr(node, "disabled") != NULL) {
		return (0);
	}

	type = html_node_get_attr(node, "type");
	kind = html_ctrl_classify(node, type);

	switch (kind) {
	case HTML_CTRL_CHECKBOX:
	case HTML_CTRL_RADIO:
		return (html_node_get_attr(node, "checked") != NULL);
	case HTML_CTRL_SUBMIT:
	case HTML_CTRL_BUTTON:
		return (node == submitter);
	default:
		return (1);
	}
}

/*
 * The value a control submits.  A checkbox with no value= sends "on", which is
 * the HTML default and what server-side code checks for.
 */
static const char *
html_form_field_value(const html_node_t *node)
{
	const char		*v;
	html_ctrl_kind_t	kind;

	v = html_node_get_attr(node, "value");
	kind = html_ctrl_classify(node, html_node_get_attr(node, "type"));

	if (kind == HTML_CTRL_CHECKBOX || kind == HTML_CTRL_RADIO) {
		return ((v != NULL && v[0] != '\0') ? v : "on");
	}
	if (kind == HTML_CTRL_SELECT) {
		/*
		 * No dropdown UI exists, so the selected option cannot have
		 * been changed: the one marked selected in the markup is what
		 * is sent, falling back to the first option as a browser does.
		 */
		const html_node_t	*opt, *first = NULL;

		for (opt = node->first_child; opt != NULL;
		    opt = opt->next_sibling) {
			if (opt->tag != HTML_TAG_OPTION) {
				continue;
			}
			if (first == NULL) {
				first = opt;
			}
			if (html_node_get_attr(opt, "selected") != NULL) {
				first = opt;
				break;
			}
		}
		if (first != NULL) {
			const char	*ov = html_node_get_attr(first, "value");

			if (ov != NULL) {
				return (ov);
			}
			/*
			 * An <option> without value= submits its text.  Leaked
			 * on purpose is not an option here, so fall through to
			 * the empty string rather than strdup something the
			 * caller cannot free.
			 */
			if (first->first_child != NULL &&
			    first->first_child->text != NULL) {
				return (first->first_child->text);
			}
		}
		return ("");
	}
	return ((v != NULL) ? v : "");
}

int
html_form_submit_url(const html_node_t *form, const html_node_t *submitter,
    char *out, size_t max_out)
{
	const html_node_t	*node;
	const char		*action, *method;
	size_t			used, n;
	int			guard, nfields;
	char			sep;

	if (form == NULL || out == NULL || max_out < 2) {
		return (-1);
	}

	method = html_node_get_attr(form, "method");
	if (method != NULL && strcasecmp(method, "get") != 0) {
		/*
		 * POST needs a request body, and browser_http_request() only
		 * emits GET.  Refusing here is what lets the caller say so
		 * instead of silently sending the wrong request.
		 */
		return (-1);
	}

	action = html_node_get_attr(form, "action");
	if (action == NULL) {
		action = "";
	}

	/*
	 * A query already on the action is dropped, as HTML requires: the new
	 * query string replaces it.  Keeping it would double parameters on
	 * every resubmit.
	 */
	used = 0;
	for (; action[used] != '\0' && action[used] != '?' &&
	    action[used] != '#'; used++) {
		if (used + 1 >= max_out) {
			return (-1);
		}
		out[used] = action[used];
	}
	out[used] = '\0';

	sep = '?';
	nfields = 0;
	guard = HTML_FORM_MAX_FIELDS;

	/*
	 * Walks the form's subtree by pointer rather than recursing, and stops
	 * at the form's own boundary so a malformed document that never closes
	 * the form does not sweep in the rest of the page.
	 */
	node = form->first_child;
	while (node != NULL && guard-- > 0) {
		int	is_ctrl = (node->tag == HTML_TAG_INPUT ||
			    node->tag == HTML_TAG_BUTTON ||
			    node->tag == HTML_TAG_TEXTAREA ||
			    node->tag == HTML_TAG_SELECT);

		if (is_ctrl != 0 &&
		    html_form_field_included(node, submitter) != 0) {
			char	enc[HTML_FORM_FIELD_MAX];

			if (used + 1 >= max_out) {
				return (-1);
			}
			out[used++] = sep;
			sep = '&';

			n = html_form_encode(enc, sizeof(enc),
			    html_node_get_attr(node, "name"));
			if (n == (size_t)-1 || used + n + 2 >= max_out) {
				return (-1);
			}
			memcpy(out + used, enc, n);
			used += n;
			out[used++] = '=';

			n = html_form_encode(enc, sizeof(enc),
			    html_form_field_value(node));
			if (n == (size_t)-1 || used + n + 1 >= max_out) {
				return (-1);
			}
			memcpy(out + used, enc, n);
			used += n;
			out[used] = '\0';
			nfields++;
		}

		if (node->first_child != NULL && node->tag != HTML_TAG_SELECT) {
			node = node->first_child;
			continue;
		}
		while (node != NULL && node != form &&
		    node->next_sibling == NULL) {
			node = node->parent;
		}
		if (node == NULL || node == form) {
			break;
		}
		node = node->next_sibling;
	}

	/*
	 * A form with no fields still submits: "?" is what a browser sends, and
	 * dropping it would re-request the current page instead.
	 */
	if (nfields == 0 && used + 1 < max_out) {
		out[used++] = '?';
		out[used] = '\0';
	}
	return (0);
}
