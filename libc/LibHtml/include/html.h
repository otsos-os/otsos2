/* !DEFINES!

$define %type html_tag as enum of HTML tags
$define %type html_node as DOM tree node
$define %type html_doc as parsed HTML document
$define %type html_layout as formatted document layout
$define %func html_parse as function with args const char *, size_t
$define %func html_doc_free as procedure with args html_doc *
$define %func html_layout_create as function with args const html_doc *, int32_t
$define %func html_layout_free as procedure with args html_layout *
$define %func html_layout_render as procedure with args libg_context *, const html_layout *, int32_t, int32_t, int32_t, int32_t, int32_t
$define %func html_layout_hit_test as function with args const html_layout *, int32_t, int32_t

*/

/* !SPACE!

$space %export html_tag_t, html_attr_t, html_node_t, html_doc_t
$space %export html_link_box_t, html_layout_line_t, html_layout_t
$space %export html_parse, html_doc_free, html_node_get_attr
$space %export html_layout_create, html_layout_free
$space %export html_layout_render, html_layout_hit_test

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
	HTML_TAG_SCRIPT
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
	char			*href;
	struct html_layout_line	*next;
} html_layout_line_t;

typedef struct html_layout {
	html_layout_line_t	*lines;
	html_link_box_t		*links;
	int32_t			content_width;
	int32_t			content_height;
} html_layout_t;

html_doc_t	*html_parse(const char *source, size_t len);
void		html_doc_free(html_doc_t *doc);
const char	*html_node_get_attr(const html_node_t *node, const char *key);

html_layout_t	*html_layout_create(const html_doc_t *doc, int32_t max_width);
void		html_layout_free(html_layout_t *layout);
void		html_layout_render(libg_context_t *ctx, const html_layout_t *layout,
		    int32_t view_x, int32_t view_y, int32_t view_w, int32_t view_h,
		    int32_t scroll_y);
const char	*html_layout_hit_test(const html_layout_t *layout, int32_t doc_x, int32_t doc_y);

#endif
