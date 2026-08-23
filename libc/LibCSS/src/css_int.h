/* !DEFINES!

$define %type css_compound as one simple selector (tag, class, id)
$define %type css_selector as compound chain with descendant combinators
$define %type css_decl as one property declaration
$define %type css_rule as selector list with declarations
$define %type css_sheet as parsed stylesheet

*/

/* !SPACE!

%internal css_compound_t, css_selector_t, css_decl_t, css_rule_t
$space %internal CSS_MAX_RULES, CSS_MAX_SELECTORS, CSS_MAX_COMPOUNDS
$space %internal CSS_MAX_DECLS, CSS_MAX_DEPTH, CSS_MATCH_STEPS_MAX
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
#define CSS_MAX_DECLS		64
#define CSS_MAX_DEPTH		128
#define CSS_TEXT_MAX		(256u * 1024u)
#define CSS_MATCH_STEPS_MAX	10000

typedef struct css_compound {
	char	*tag;
	char	*cls[CSS_MAX_CLASSES];
	int	ncls;
	char	*id;
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
