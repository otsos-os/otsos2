/* !DEFINES!

$define %type css_compound as one simple selector (tag, class, id, attrs)
$define %type css_attr_sel as one attribute selector with match operator
$define %type css_combinator as how a compound binds to the one before it
$define %type css_selector as compound chain with combinators
$define %type css_decl as one property declaration
$define %type css_rule as selector list with declarations
$define %type css_sheet as parsed stylesheet

*/

/* !SPACE!

%internal css_compound_t, css_selector_t, css_decl_t, css_rule_t
$space %internal css_attr_sel_t, css_combinator_t, css_attr_op_t
$space %internal CSS_MAX_RULES, CSS_MAX_SELECTORS, CSS_MAX_COMPOUNDS
$space %internal CSS_MAX_DECLS, CSS_MAX_DEPTH, CSS_MATCH_STEPS_MAX
$space %internal CSS_MAX_ATTRS, CSS_MAX_AT_DEPTH, CSS_BASE_EM_PX
$space %internal CSS_DEFAULT_VIEWPORT_W, CSS_DEFAULT_VIEWPORT_H
$space %internal CSS_MARGIN_MAX_PX, CSS_PAD_MAX_PX, CSS_BORDER_MAX_PX
$space %internal CSS_SIZE_MAX_PX
$space %internal css_color_parse

*/

/*
 * Copyright (c) 2026, otsos team
 */

#ifndef _CSS_INT_H
#define _CSS_INT_H

#include <css.h>

#define CSS_MAX_RULES		2048
#define CSS_MAX_SELECTORS	16
#define CSS_MAX_COMPOUNDS	16
#define CSS_MAX_CLASSES		4
#define CSS_MAX_ATTRS		4
#define CSS_MAX_DECLS		64
#define CSS_MAX_DEPTH		128
#define CSS_TEXT_MAX		(256u * 1024u)
#define CSS_MATCH_STEPS_MAX	10000

#define CSS_MAX_AT_DEPTH	4

#define CSS_BASE_EM_PX		16
#define CSS_DEFAULT_VIEWPORT_W	1024
#define CSS_DEFAULT_VIEWPORT_H	768
#define CSS_MARGIN_MAX_PX	500
#define CSS_PAD_MAX_PX		200
#define CSS_BORDER_MAX_PX	32
#define CSS_SIZE_MAX_PX		8192

typedef enum css_attr_op {
	CSS_ATTR_PRESENT = 0,	/* [x]      */
	CSS_ATTR_EQ,		/* [x=v]    */
	CSS_ATTR_INCLUDES,	/* [x~=v]   space-separated word */
	CSS_ATTR_DASH,		/* [x|=v]   v or v- prefix */
	CSS_ATTR_PREFIX,	/* [x^=v]   */
	CSS_ATTR_SUFFIX,	/* [x$=v]   */
	CSS_ATTR_SUBSTR		/* [x*=v]   */
} css_attr_op_t;

typedef struct css_attr_sel {
	char		*name;
	char		*value;
	css_attr_op_t	op;
} css_attr_sel_t;

/*
 * How a compound binds to the compound to its left.  DESC is whitespace, the
 * default; the other three are the explicit combinators.  Stored on the right
 * compound because that is the direction the matcher walks (rightmost first).
 */
typedef enum css_combinator {
	CSS_COMB_DESC = 0,	/* "a b"  */
	CSS_COMB_CHILD,		/* "a > b" */
	CSS_COMB_ADJ,		/* "a + b" */
	CSS_COMB_SIB		/* "a ~ b" */
} css_combinator_t;

typedef struct css_compound {
	char			*tag;
	char			*cls[CSS_MAX_CLASSES];
	char			*id;
	css_attr_sel_t		attrs[CSS_MAX_ATTRS];
	int			ncls;
	int			nattrs;
	/* :first-child, answerable from the prev-sibling callback alone. */
	int			first_child;
	css_combinator_t	comb;
} css_compound_t;

typedef struct css_selector {
	css_compound_t	parts[CSS_MAX_COMPOUNDS];
	int		nparts;
	int		spec_ids;
	int		spec_classes;
	int		spec_types;
} css_selector_t;

typedef struct css_decl {
	char	*prop;
	char	*value;
} css_decl_t;

typedef struct css_rule {
	css_selector_t	*sels;
	int		nsels;
	css_decl_t	*decls;
	int		ndecls;
	int		order;
} css_rule_t;

struct css_sheet {
	css_rule_t	*rules;
	int		nrules;
	int		rules_cap;
};

int	css_color_parse(const char *str, uint32_t *out_rgba);

#endif
