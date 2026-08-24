/* !DEFINES!

$define %type html_tag as enum of HTML tags
$define %type html_node as DOM tree node
$define %type html_doc as parsed HTML document
$define %type html_layout as formatted document layout
$define %func html_parse as function with args const char *, size_t
$define %func html_doc_free as procedure with args html_doc *
$define %func html_node_find as function with args const html_node *, html_tag
$define %func html_node_text as function with args const html_node *
$define %func html_layout_create as function with args const html_doc *, int32_t
$define %func html_layout_create_ex as function with args const html_doc *, int32_t, int32_t, html_image_size_fn, void *
$define %type html_image_size_fn as host callback answering intrinsic image size
$define %func html_layout_free as procedure with args html_layout *
$define %type html_box_kind as enum of non-text box kinds
$define %func html_layout_render as procedure with args libg_context *, const html_layout *, int32_t, int32_t, int32_t, int32_t, int32_t, html_image_draw_fn, void *
$define %func html_layout_hit_test as function with args const html_layout *, int32_t, int32_t
$define %type html_ctrl_kind as enum of form control kinds
$define %type html_ctrl_box as one laid-out form control
$define %func html_layout_ctrl_at as function with args const html_layout *, int32_t, int32_t
$define %func html_node_set_attr as function with args html_node *, const char *, const char *
$define %func html_node_form as function with args const html_node *
$define %func html_form_submit_url as function with args const html_node *, const html_node *, char *, size_t
$define %func html_doc_append_css as function with args html_doc *, const char *, size_t
$define %func html_doc_stylesheet_link as function with args const html_doc *, int, char *, size_t

*/

/* !SPACE!

$space %export html_tag_t, html_attr_t, html_node_t, html_doc_t
$space %export html_link_box_t, html_layout_line_t, html_layout_t
$space %export html_box_kind_t, html_layout_box_t, html_image_draw_fn
$space %export html_image_size_fn
$space %export html_ctrl_kind_t, html_ctrl_box_t
$space %export html_parse, html_doc_free, html_node_get_attr
$space %export html_node_find, html_node_text, html_node_set_attr
$space %export html_layout_create, html_layout_create_ex, html_layout_free
$space %export html_layout_render, html_layout_hit_test
$space %export html_layout_ctrl_at, html_node_form, html_form_submit_url
$space %export html_doc_append_css, html_doc_stylesheet_link

*/

#ifndef _HTML_H
#define _HTML_H

#include <libg.h>
#include <stddef.h>
#include <stdint.h>

typedef enum html_tag {
	HTML_TAG_UNKNOWN = 0,
	HTML_TAG_TEXT,
	HTML_TAG_HTML,
	HTML_TAG_HEAD,
	HTML_TAG_TITLE,
	HTML_TAG_BODY,
	HTML_TAG_H1,
	HTML_TAG_H2,
	HTML_TAG_H3,
	HTML_TAG_H4,
	HTML_TAG_H5,
	HTML_TAG_H6,
	HTML_TAG_P,
	HTML_TAG_A,
	HTML_TAG_DIV,
	HTML_TAG_SPAN,
	HTML_TAG_UL,
	HTML_TAG_OL,
	HTML_TAG_LI,
	HTML_TAG_BR,
	HTML_TAG_HR,
	HTML_TAG_PRE,
	HTML_TAG_CODE,
	HTML_TAG_B,
	HTML_TAG_STRONG,
	HTML_TAG_I,
	HTML_TAG_EM,
	HTML_TAG_U,
	HTML_TAG_IMG,
	HTML_TAG_TABLE,
	HTML_TAG_TR,
	HTML_TAG_TD,
	HTML_TAG_TH,
	HTML_TAG_FORM,
	HTML_TAG_INPUT,
	HTML_TAG_BUTTON,
	HTML_TAG_BLOCKQUOTE,
	HTML_TAG_STYLE,
	HTML_TAG_SCRIPT,
	/*
	 * HTML only.  STYLE/SCRIPT/NOSCRIPT/TEMPLATE exist as tags so the
	 * parser can skip their bodies -- LibHtml deliberately implements no
	 * CSS and no scripting, so their contents are never interpreted.
	 */
	HTML_TAG_NOSCRIPT,
	HTML_TAG_TEMPLATE,
	HTML_TAG_TEXTAREA,
	HTML_TAG_META,
	HTML_TAG_LINK,
	HTML_TAG_BASE,
	HTML_TAG_ARTICLE,
	HTML_TAG_SECTION,
	HTML_TAG_NAV,
	HTML_TAG_ASIDE,
	HTML_TAG_HEADER,
	HTML_TAG_FOOTER,
	HTML_TAG_MAIN,
	HTML_TAG_FIGURE,
	HTML_TAG_FIGCAPTION,
	HTML_TAG_DL,
	HTML_TAG_DT,
	HTML_TAG_DD,
	HTML_TAG_THEAD,
	HTML_TAG_TBODY,
	HTML_TAG_TFOOT,
	HTML_TAG_CAPTION,
	HTML_TAG_COLGROUP,
	HTML_TAG_COL,
	HTML_TAG_SMALL,
	HTML_TAG_BIG,
	HTML_TAG_SUB,
	HTML_TAG_SUP,
	HTML_TAG_DEL,
	HTML_TAG_INS,
	HTML_TAG_MARK,
	HTML_TAG_ABBR,
	HTML_TAG_CITE,
	HTML_TAG_Q,
	HTML_TAG_KBD,
	HTML_TAG_SAMP,
	HTML_TAG_VAR,
	HTML_TAG_TT,
	HTML_TAG_LABEL,
	HTML_TAG_SELECT,
	HTML_TAG_OPTION,
	HTML_TAG_FIELDSET,
	HTML_TAG_LEGEND,
	HTML_TAG_WBR,
	HTML_TAG_CENTER,
	HTML_TAG_FONT,
	HTML_TAG_ADDRESS,
	HTML_TAG_TIME,
	HTML_TAG_PARAM,
	HTML_TAG_SOURCE,
	HTML_TAG_TRACK,
	HTML_TAG_AREA,
	HTML_TAG_EMBED,
	HTML_TAG_SVG
} html_tag_t;

typedef struct html_attr {
	char			*name;
	char			*value;
	struct html_attr	*next;
} html_attr_t;

typedef struct html_node {
	html_tag_t		tag;
	char			*tag_name;
	char			*text;
	html_attr_t		*attrs;
	struct html_node	*parent;
	struct html_node	*first_child;
	struct html_node	*last_child;
	struct html_node	*next_sibling;
	struct html_node	*prev_sibling;
} html_node_t;

typedef struct html_doc {
	html_node_t		*root;
	char			*title;
	char			*base_url;
	char			*stylesheet;
} html_doc_t;

typedef struct html_link_box {
	libg_rect_t		rect;
	char			*href;
	struct html_link_box	*next;
} html_link_box_t;

typedef struct html_layout_line {
	char			*text;
	int32_t			x;
	int32_t			y;
	int32_t			width;
	int32_t			height;
	uint32_t		color;
	uint32_t		scale;
	int			bold;
	int			underline;
	int			strike;
	char			*href;
	struct html_layout_line	*next;
} html_layout_line_t;

typedef enum html_box_kind {
	HTML_BOX_FILL = 0,
	HTML_BOX_STROKE,
	HTML_BOX_HLINE,
	HTML_BOX_IMAGE
} html_box_kind_t;

/*
 * Non-text geometry: rules, table cell borders, blockquote bars.  Kept in a
 * separate list so it can all be painted underneath the text runs in one
 * pass, without sorting.
 */
typedef struct html_layout_box {
	libg_rect_t		rect;
	uint32_t		color;
	html_box_kind_t		kind;
	char			*ref;
	struct html_layout_box	*next;
} html_layout_box_t;

/*
 * What a control does when it is clicked or typed into.  Derived once at
 * layout time from the tag plus type= attribute so the renderer and the event
 * path never re-parse it.
 */
typedef enum html_ctrl_kind {
	HTML_CTRL_TEXT = 0,	/* text/search/email/url/tel/number/password */
	HTML_CTRL_BUTTON,	/* button, input type=button/reset */
	HTML_CTRL_SUBMIT,	/* input type=submit, button type=submit */
	HTML_CTRL_CHECKBOX,
	HTML_CTRL_RADIO,
	HTML_CTRL_SELECT,
	HTML_CTRL_TEXTAREA
} html_ctrl_kind_t;

/*
 * One interactive control.  Its own list rather than a box kind, because it
 * carries the DOM node: the text a field shows is read from that node's value
 * attribute at paint time, so typing a character does not force a relayout.
 *
 * `node` points into the html_doc_t the layout was built from, and is only
 * valid while that document is alive - html_layout_free() does not own it.
 */
typedef struct html_ctrl_box {
	libg_rect_t		rect;
	html_node_t		*node;
	char			*label;		/* button face, NULL for fields */
	uint32_t		bg;		/* alpha 0 = do not fill */
	uint32_t		fg;
	uint32_t		border;		/* alpha 0 = no frame */
	int32_t			border_width;
	int32_t			pad_left;
	int32_t			pad_top;
	uint32_t		scale;
	html_ctrl_kind_t	kind;
	int			disabled;
	int			password;
	struct html_ctrl_box	*next;
} html_ctrl_box_t;

typedef struct html_layout {
	html_layout_line_t	*lines;
	html_link_box_t		*links;
	html_layout_box_t	*boxes;
	html_ctrl_box_t		*ctrls;
	/*
	 * Tail pointers exist because appending by walking from the head is
	 * O(n) per word: a page with a few thousand words made layout the
	 * slowest part of a load.
	 */
	html_layout_line_t	*lines_tail;
	html_link_box_t		*links_tail;
	html_layout_box_t	*boxes_tail;
	html_ctrl_box_t		*ctrls_tail;
	int32_t			content_width;
	int32_t			content_height;
	uint32_t		page_bg;
	int			has_page_bg;
	void			*css_sheet;
	/*
	 * Keyboard focus, owned here rather than by the embedder so
	 * html_layout_render() keeps its signature.  Points at a control in
	 * the list above, never at a freed node: navigation frees the layout
	 * and the focus with it.
	 */
	html_ctrl_box_t		*focus;
	size_t			caret;
	int			caret_on;
	int			images_estimated;
} html_layout_t;

html_doc_t	*html_parse(const char *source, size_t len);
void		html_doc_free(html_doc_t *doc);
const char	*html_node_get_attr(const html_node_t *node, const char *key);
int		html_node_set_attr(html_node_t *node, const char *key,
		    const char *value);
const html_node_t *html_node_find(const html_node_t *root, html_tag_t tag);
char		*html_node_text(const html_node_t *node);

/*
 * Appends `len` bytes of CSS to doc->stylesheet, growing it.  Used to fold an
 * external <link rel=stylesheet> into the sheet the next layout parses.
 * Returns 0 on success, -1 if the combined text would exceed the cap.
 */
int		html_doc_append_css(html_doc_t *doc, const char *text,
		    size_t len);

/*
 * Copies the href of the `index`th <link rel=stylesheet> into `out`.  Returns
 * 0 on success, -1 when there is no such link, which is how a caller
 * enumerates: index 0, 1, 2, ... until it fails.
 */
int		html_doc_stylesheet_link(const html_doc_t *doc, int index,
		    char *out, size_t max_out);

/* Nearest <form> ancestor, or NULL for a control outside any form. */
const html_node_t *html_node_form(const html_node_t *node);

/*
 * Builds the URL a form submission navigates to: the form's action with a
 * query string assembled from every named control under it, percent-encoded.
 * `submitter` may be NULL, or the clicked submit control, whose own name=value
 * pair is then included as browsers do.
 *
 * Only GET is buildable.  Returns 0 on success, -1 when the form declares
 * method=post (nothing here can send a body) or the result would not fit.
 */
int		html_form_submit_url(const html_node_t *form,
		    const html_node_t *submitter, char *out, size_t max_out);

typedef int (*html_image_size_fn)(void *userdata, const char *ref,
	    int32_t *out_w, int32_t *out_h);

/*
 * viewport_w is the wrap width; viewport_h is only used to answer @media
 * height queries.  `doc` is not const because a text field's value attribute
 * is mutated in place as the user types.
 */
html_layout_t	*html_layout_create(html_doc_t *doc, int32_t viewport_w,
		    int32_t viewport_h);
html_layout_t	*html_layout_create_ex(html_doc_t *doc, int32_t viewport_w,
		    int32_t viewport_h, html_image_size_fn size,
		    void *size_user);
void		html_layout_free(html_layout_t *layout);
typedef int (*html_image_draw_fn)(void *userdata, libg_context_t *ctx,
	    libg_rect_t rect, const char *ref);

void		html_layout_render(libg_context_t *ctx, const html_layout_t *layout,
		    int32_t view_x, int32_t view_y, int32_t view_w,
		    int32_t view_h, int32_t scroll_y,
		    html_image_draw_fn draw, void *draw_user);
const char	*html_layout_hit_test(const html_layout_t *layout, int32_t doc_x, int32_t doc_y);

/*
 * Control under a document-space point, or NULL.  Must be consulted before
 * html_layout_hit_test(): a control inside an <a> would otherwise navigate
 * instead of taking focus.
 */
html_ctrl_box_t	*html_layout_ctrl_at(const html_layout_t *layout,
		    int32_t doc_x, int32_t doc_y);

#endif
