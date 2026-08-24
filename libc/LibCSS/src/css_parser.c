/* !DEFINES!

$define %func css_parse as function with args const char *, css_sheet **
$define %func css_parse_ex as function with args const char *, int32_t,
	int32_t, css_sheet **
$define %func css_free as procedure with args css_sheet *

*/

/* !SPACE!

$space %internal css_skip_ws, css_skip_comment, css_read_ident
$space %internal css_compound_parse, css_selector_parse
$space %internal css_decls_parse, css_rule_add, css_xdups
$space %internal css_pseudo_static, css_attr_parse, css_read_value
$space %internal css_media_query_match, css_media_match, css_parse_range
$space %internal css_skip_balanced, css_media_px, css_media_feature

*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "css_int.h"

static char *
css_xdups(const char *src, size_t len)
{
	char	*out;

	out = (char *)malloc(len + 1);
	if (out == NULL) {
		return (NULL);
	}
	memcpy(out, src, len);
	out[len] = '\0';
	return (out);
}

static const char *
css_skip_ws(const char *p, const char *end)
{
	while (p < end && isspace((unsigned char)*p)) {
		p++;
	}
	return (p);
}

static const char *
css_skip_comment(const char *p, const char *end)
{
	p += 2;
	while (p + 1 < end && !(p[0] == '*' && p[1] == '/')) {
		p++;
	}
	return ((p + 1 < end) ? p + 2 : end);
}

static const char *
css_skip_blank(const char *p, const char *end)
{
	for (;;) {
		p = css_skip_ws(p, end);
		if (end - p >= 2 && p[0] == '/' && p[1] == '*') {
			p = css_skip_comment(p, end);
			continue;
		}
		return (p);
	}
}

static int
css_is_ident_char(char c)
{
	return (isalnum((unsigned char)c) != 0 || c == '-' || c == '_' ||
	    (unsigned char)c >= 0x80);
}

static char *
css_read_ident(const char **pp, const char *end)
{
	const char	*p;
	size_t		nl;

	p = *pp;
	nl = 0;
	if (p < end && css_is_ident_char(p[nl]) == 0) {
		return (NULL);
	}
	while (p + nl < end && css_is_ident_char(p[nl]) != 0) {
		nl++;
	}
	*pp = p + nl;
	return (css_xdups(p, nl));
}

/*
 * Skips a balanced (...) run, starting at the opening paren.  Needed for
 * functional pseudo-classes: :not(.a, .b) must be consumed as one unit or the
 * comma inside it would be read as the end of the selector.
 */
static const char *
css_skip_balanced(const char *p, const char *end)
{
	int	depth;

	if (p >= end || *p != '(') {
		return (p);
	}
	depth = 0;
	while (p < end) {
		if (*p == '(') {
			depth++;
		} else if (*p == ')') {
			depth--;
			if (depth == 0) {
				return (p + 1);
			}
		}
		p++;
	}
	return (end);
}

/*
 * Classifies one pseudo-class or pseudo-element by whether a static, one-node
 * matcher can decide it.
 *
 * Returns 1 when the pseudo imposes no constraint we cannot honour (possibly
 * setting *force_html or *first_child), and 0 when it can never be decided
 * here - the caller then throws the whole selector away.  Dropping the
 * selector rather than the rule is the point: "a{...}" and "a:hover{...}" are
 * separate rules, and in "a,a:hover{...}" the plain "a" half must survive.
 */
static int
css_pseudo_static(const char *name, int *force_html, int *first_child)
{
	static const char *const ok[] = {
		"link", "any-link", "enabled", "read-write", NULL
	};
	int	i;

	if (strcasecmp(name, "root") == 0) {
		*force_html = 1;
		return (1);
	}
	if (strcasecmp(name, "first-child") == 0) {
		*first_child = 1;
		return (1);
	}
	for (i = 0; ok[i] != NULL; i++) {
		if (strcasecmp(name, ok[i]) == 0) {
			return (1);
		}
	}
	/*
	 * Everything else is a dynamic state (:hover, :focus, :checked), needs
	 * sibling counting (:nth-child, :last-child), or generates content
	 * (::before).  None of those can be resolved from a static tree walk.
	 */
	return (0);
}

/*
 * Reads an attribute selector's value: bare ident, 'single' or "double"
 * quoted.  A quoted value may contain characters an ident may not, which is
 * why this does not reuse css_read_ident.
 */
static char *
css_read_value(const char **pp, const char *end)
{
	const char	*p, *start;
	char		quote;

	p = *pp;
	if (p >= end) {
		return (NULL);
	}
	if (*p == '"' || *p == '\'') {
		quote = *p;
		p++;
		start = p;
		while (p < end && *p != quote) {
			p++;
		}
		if (p >= end) {
			return (NULL);	/* unterminated */
		}
		*pp = p + 1;
		return (css_xdups(start, (size_t)(p - start)));
	}
	start = p;
	while (p < end && css_is_ident_char(*p) != 0) {
		p++;
	}
	if (p == start) {
		return (NULL);
	}
	*pp = p;
	return (css_xdups(start, (size_t)(p - start)));
}

/* Parses "[name]", "[name=v]", "[name~=v]" and the ^= $= *= |= variants. */
static int
css_attr_parse(const char **pp, const char *end, css_attr_sel_t *out)
{
	const char	*p;

	memset(out, 0, sizeof(*out));
	p = *pp + 1;			/* past '[' */
	p = css_skip_blank(p, end);

	out->name = css_read_ident(&p, end);
	if (out->name == NULL) {
		return (-1);
	}
	p = css_skip_blank(p, end);
	if (p >= end) {
		goto fail;
	}

	if (*p == ']') {
		out->op = CSS_ATTR_PRESENT;
		*pp = p + 1;
		return (0);
	}

	switch (*p) {
	case '=':
		out->op = CSS_ATTR_EQ;
		break;
	case '~':
		out->op = CSS_ATTR_INCLUDES;
		p++;
		break;
	case '|':
		out->op = CSS_ATTR_DASH;
		p++;
		break;
	case '^':
		out->op = CSS_ATTR_PREFIX;
		p++;
		break;
	case '$':
		out->op = CSS_ATTR_SUFFIX;
		p++;
		break;
	case '*':
		out->op = CSS_ATTR_SUBSTR;
		p++;
		break;
	default:
		goto fail;
	}
	if (p >= end || *p != '=') {
		goto fail;
	}
	p++;
	p = css_skip_blank(p, end);

	out->value = css_read_value(&p, end);
	if (out->value == NULL) {
		goto fail;
	}
	p = css_skip_blank(p, end);
	/* An "i"/"s" case-sensitivity flag is accepted and ignored. */
	if (p < end && (*p == 'i' || *p == 'I' || *p == 's' || *p == 'S')) {
		p++;
		p = css_skip_blank(p, end);
	}
	if (p >= end || *p != ']') {
		goto fail;
	}
	*pp = p + 1;
	return (0);

fail:
	free(out->name);
	free(out->value);
	memset(out, 0, sizeof(*out));
	return (-1);
}

static void
css_compound_reset(css_compound_t *cp)
{
	int	k;

	free(cp->tag);
	free(cp->id);
	for (k = 0; k < cp->ncls; k++) {
		free(cp->cls[k]);
	}
	for (k = 0; k < cp->nattrs; k++) {
		free(cp->attrs[k].name);
		free(cp->attrs[k].value);
	}
	memset(cp, 0, sizeof(*cp));
}

/*
 * One compound selector: an optional tag followed by any run of #id, .class,
 * [attr], :pseudo and *.  Stops at whitespace, a combinator, a comma or '{'.
 * A construct this matcher cannot honour fails the whole compound, which fails
 * only its own selector - sibling selectors in the same comma list survive.
 */
static int
css_compound_parse(const char **pp, const char *end, css_compound_t *out,
    int *spec_ids, int *spec_classes, int *spec_types)
{
	const char	*p;
	char		*name;
	int		force_html, first_child, seen;

	memset(out, 0, sizeof(*out));
	p = css_skip_blank(*pp, end);
	seen = 0;

	while (p < end && *p != ' ' && *p != '\t' && *p != '\n' &&
	    *p != '\r' && *p != ',' && *p != '{' && *p != '>' && *p != '+' &&
	    *p != '~') {
		if (*p == '.' || *p == '#') {
			char	sigil = *p;

			p++;
			name = css_read_ident(&p, end);
			if (name == NULL) {
				goto fail;
			}
			seen = 1;
			if (sigil == '#') {
				/*
				 * A repeated #id on one compound (Google ships
				 * "#gb#gb" to win a specificity fight) can only
				 * match when both are the same id, so keeping
				 * the first and still counting specificity is
				 * exact rather than approximate.
				 */
				if (out->id != NULL) {
					free(name);
				} else {
					out->id = name;
				}
				(*spec_ids)++;
			} else if (out->ncls < CSS_MAX_CLASSES) {
				out->cls[out->ncls++] = name;
				(*spec_classes)++;
			} else {
				/* Past the ceiling: cannot honour, so drop. */
				free(name);
				goto fail;
			}
			continue;
		}
		if (*p == '[') {
			if (out->nattrs >= CSS_MAX_ATTRS ||
			    css_attr_parse(&p, end,
			    &out->attrs[out->nattrs]) != 0) {
				goto fail;
			}
			out->nattrs++;
			(*spec_classes)++;	/* attr ranks as a class */
			seen = 1;
			continue;
		}
		if (*p == ':') {
			p++;
			if (p < end && *p == ':') {
				goto fail;	/* ::before generates content */
			}
			name = css_read_ident(&p, end);
			if (name == NULL) {
				goto fail;
			}
			if (p < end && *p == '(') {
				/* :not(), :nth-child(), :is(): not decidable. */
				free(name);
				goto fail;
			}
			force_html = 0;
			first_child = 0;
			if (css_pseudo_static(name, &force_html,
			    &first_child) == 0) {
				free(name);
				goto fail;
			}
			free(name);
			(*spec_classes)++;
			if (force_html != 0 && out->tag == NULL) {
				out->tag = css_xdups("html", 4);
				if (out->tag == NULL) {
					goto fail;
				}
			}
			if (first_child != 0) {
				out->first_child = 1;
			}
			seen = 1;
			continue;
		}
		if (*p == '*') {
			/* Universal: constrains nothing, adds no specificity. */
			p++;
			seen = 1;
			continue;
		}
		if (out->tag == NULL && css_is_ident_char(*p) != 0) {
			out->tag = css_read_ident(&p, end);
			if (out->tag == NULL) {
				goto fail;
			}
			(*spec_types)++;
			seen = 1;
			continue;
		}
		goto fail;
	}

	if (seen == 0) {
		goto fail;
	}
	*pp = p;
	return (0);

fail:
	css_compound_reset(out);
	return (-1);
}

/*
 * A full selector: compounds joined by combinators.  The combinator is stored
 * on the compound to its right because the matcher walks right to left, so
 * that is the side that needs to know how to step.
 */
static int
css_selector_parse(const char **pp, const char *end, css_selector_t *out)
{
	const char		*p;
	css_combinator_t	next_comb;
	int			i;

	memset(out, 0, sizeof(*out));
	p = *pp;
	next_comb = CSS_COMB_DESC;

	for (;;) {
		p = css_skip_blank(p, end);
		if (p >= end || *p == ',' || *p == '{') {
			break;
		}

		if (*p == '>' || *p == '+' || *p == '~') {
			if (out->nparts == 0) {
				goto fail;	/* leading combinator */
			}
			next_comb = (*p == '>') ? CSS_COMB_CHILD :
			    (*p == '+') ? CSS_COMB_ADJ : CSS_COMB_SIB;
			p++;
			continue;
		}

		if (out->nparts >= CSS_MAX_COMPOUNDS ||
		    css_compound_parse(&p, end, &out->parts[out->nparts],
		    &out->spec_ids, &out->spec_classes,
		    &out->spec_types) != 0) {
			goto fail;
		}
		out->parts[out->nparts].comb = next_comb;
		out->nparts++;
		next_comb = CSS_COMB_DESC;
	}

	if (out->nparts == 0) {
		goto fail;
	}
	*pp = p;
	return (0);

fail:
	for (i = 0; i < out->nparts; i++) {
		css_compound_reset(&out->parts[i]);
	}
	out->nparts = 0;
	return (-1);
}

static void
css_decls_free(css_decl_t *decls, int ndecls)
{
	int	i;

	for (i = 0; i < ndecls; i++) {
		free(decls[i].prop);
		free(decls[i].value);
	}
	free(decls);
}

static int
css_decls_parse(const char *body, const char *body_end, css_decl_t **out,
    int *out_n)
{
	css_decl_t	*decls;
	const char	*p;
	int		n, cap;

	decls = NULL;
	n = 0;
	cap = 8;
	decls = (css_decl_t *)malloc((size_t)cap * sizeof(css_decl_t));
	if (decls == NULL) {
		return (-1);
	}

	p = body;
	while (p < body_end) {
		const char	*seg_end, *q, *colon;
		char		*prop, *value;
		size_t		plen, vlen;

		p = css_skip_blank(p, body_end);
		if (p >= body_end) {
			break;
		}
		seg_end = memchr(p, ';', (size_t)(body_end - p));
		if (seg_end == NULL) {
			seg_end = body_end;
		}

		colon = memchr(p, ':', (size_t)(seg_end - p));
		if (colon != NULL) {
			const char	*v;

			q = css_skip_blank(p, colon);
			plen = (size_t)(colon - q);
			while (plen > 0 && isspace((unsigned char)q[plen - 1])) {
				plen--;
			}
			v = colon + 1;
			vlen = (size_t)(seg_end - v);
			while (vlen > 0 &&
			    isspace((unsigned char)v[vlen - 1])) {
				vlen--;
			}
			if (plen > 0 && vlen > 0) {
				prop = css_xdups(q, plen);
				value = css_xdups(v, vlen);
				if (prop != NULL && value != NULL) {
					if (n == cap) {
						css_decl_t	*nd;
						int	ncap = cap * 2;

						nd = (css_decl_t *)realloc(
						    decls, (size_t)ncap *
						    sizeof(css_decl_t));
						if (nd != NULL) {
							decls = nd;
							cap = ncap;
						}
					}
					if (n < cap) {
						decls[n].prop = prop;
						decls[n].value = value;
						n++;
					} else {
						free(prop);
						free(value);
					}
				} else {
					free(prop);
					free(value);
				}
			}
		}
		p = seg_end + ((seg_end < body_end) ? 1 : 0);
	}

	*out = decls;
	*out_n = n;
	return (0);
}

static void
css_selector_free(css_selector_t *sel)
{
	int	i;

	/*
	 * Goes through css_compound_reset rather than freeing fields inline:
	 * a compound owns attribute name/value strings too, and an inline copy
	 * of this loop is exactly how those got leaked before.
	 */
	for (i = 0; i < sel->nparts; i++) {
		css_compound_reset(&sel->parts[i]);
	}
	sel->nparts = 0;
}

static void
css_rule_free(css_rule_t *rule)
{
	int	i;

	for (i = 0; i < rule->nsels; i++) {
		css_selector_free(&rule->sels[i]);
	}
	free(rule->sels);
	css_decls_free(rule->decls, rule->ndecls);
	memset(rule, 0, sizeof(*rule));
}

static int
css_rule_add(css_sheet_t *sheet, css_rule_t *rule)
{
	if (sheet->nrules >= CSS_MAX_RULES) {
		return (-1);
	}
	if (sheet->nrules + 1 > sheet->rules_cap) {
		css_rule_t	*nrules;
		int		ncap = (sheet->rules_cap == 0) ? 32 :
				    sheet->rules_cap * 2;

		nrules = (css_rule_t *)realloc(sheet->rules,
		    (size_t)ncap * sizeof(css_rule_t));
		if (nrules == NULL) {
			return (-1);
		}
		sheet->rules = nrules;
		sheet->rules_cap = ncap;
	}
	sheet->rules[sheet->nrules++] = *rule;
	memset(rule, 0, sizeof(*rule));
	return (0);
}

void
css_free(css_sheet_t *sheet)
{
	int	i;

	if (sheet == NULL) {
		return;
	}
	for (i = 0; i < sheet->nrules; i++) {
		css_rule_free(&sheet->rules[i]);
	}
	free(sheet->rules);
	free(sheet);
}

/*
 * Length parser for media features only.
 *
 * Deliberately separate from css_value_px() in css_match.c: that one clamps to
 * the widest box this renderer will draw, and clamping here would make
 * (min-width: 1024px) match a 640px viewport.  Media lengths must stay exact.
 *
 * Returns 0 and writes *out on success, -1 for a unit this engine cannot
 * resolve to a pixel count.
 */
static int
css_media_px(const char *str, int32_t *out)
{
	const char	*p;
	double		val, frac;
	int		digits;

	p = str;
	val = 0.0;
	digits = 0;
	while (isdigit((unsigned char)*p) != 0) {
		val = val * 10.0 + (double)(*p - '0');
		p++;
		digits++;
	}
	if (*p == '.') {
		p++;
		frac = 0.1;
		while (isdigit((unsigned char)*p) != 0) {
			val += (double)(*p - '0') * frac;
			frac *= 0.1;
			p++;
			digits++;
		}
	}
	if (digits == 0) {
		return (-1);
	}

	if (*p == '\0' || strcasecmp(p, "px") == 0) {
		/* Unitless 0 is legal in a media query; other bare numbers
		 * are not, but treating them as px is harmless here. */
	} else if (strcasecmp(p, "pt") == 0) {
		val = val * 96.0 / 72.0;
	} else if (strcasecmp(p, "em") == 0 || strcasecmp(p, "rem") == 0) {
		/* CSS_BASE_EM_PX: nominal font box, not the 5x7 cell. */
		val = val * (double)CSS_BASE_EM_PX;
	} else {
		return (-1);	/* dppx, dpi, vw, ch, ratios */
	}

	if (val < 0.0) {
		return (-1);
	}
	if (val > 1000000.0) {
		val = 1000000.0;
	}
	*out = (int32_t)(val + 0.5);
	return (0);
}

/*
 * One "(feature: value)" media condition, parens already stripped.
 *
 * Fails closed: a feature this engine cannot measure returns 0 rather than 1.
 * The alternative would apply a high-DPI or print stylesheet on a 1x display,
 * which is visibly worse than ignoring the block.
 */
static int
css_media_feature(const char *p, const char *end, int32_t vw, int32_t vh)
{
	char		name[40], value[40];
	const char	*colon;
	size_t		n;
	int32_t		px;

	p = css_skip_blank(p, end);
	colon = memchr(p, ':', (size_t)(end - p));

	if (colon == NULL) {
		/*
		 * Bare feature, true when the property is non-zero: (width) is
		 * true on any real viewport, (color) on any colour display.
		 */
		n = 0;
		while (p + n < end && css_is_ident_char(p[n]) != 0 &&
		    n < sizeof(name) - 1) {
			name[n] = p[n];
			n++;
		}
		name[n] = '\0';
		return (strcasecmp(name, "width") == 0 ||
		    strcasecmp(name, "height") == 0 ||
		    strcasecmp(name, "color") == 0);
	}

	n = 0;
	while (p < colon && n < sizeof(name) - 1) {
		if (css_is_ident_char(*p) != 0) {
			name[n++] = *p;
		}
		p++;
	}
	name[n] = '\0';

	p = css_skip_blank(colon + 1, end);
	n = 0;
	while (p < end && n < sizeof(value) - 1 && !isspace((unsigned char)*p)) {
		value[n++] = *p++;
	}
	value[n] = '\0';
	if (name[0] == '\0' || value[0] == '\0') {
		return (0);
	}

	if (strcasecmp(name, "orientation") == 0) {
		if (strcasecmp(value, "landscape") == 0) {
			return (vw >= vh);
		}
		if (strcasecmp(value, "portrait") == 0) {
			return (vw < vh);
		}
		return (0);
	}

	if (css_media_px(value, &px) != 0) {
		/*
		 * Not a pixel length: device-pixel-ratio, dppx resolutions,
		 * aspect ratios.  All of them describe hardware this renderer
		 * does not have, so none of them match.
		 */
		return (0);
	}

	if (strcasecmp(name, "min-width") == 0 ||
	    strcasecmp(name, "min-device-width") == 0) {
		return (vw >= px);
	}
	if (strcasecmp(name, "max-width") == 0 ||
	    strcasecmp(name, "max-device-width") == 0) {
		return (vw <= px);
	}
	if (strcasecmp(name, "min-height") == 0 ||
	    strcasecmp(name, "min-device-height") == 0) {
		return (vh >= px);
	}
	if (strcasecmp(name, "max-height") == 0 ||
	    strcasecmp(name, "max-device-height") == 0) {
		return (vh <= px);
	}
	if (strcasecmp(name, "width") == 0) {
		return (vw == px);
	}
	if (strcasecmp(name, "height") == 0) {
		return (vh == px);
	}
	return (0);
}

/* One query from a comma list: "not? only? type? (and (cond))*". */
static int
css_media_query_match(const char *p, const char *end, int32_t vw, int32_t vh)
{
	char	word[32];
	size_t	n;
	int	negate, result, saw_token;

	negate = 0;
	result = 1;
	saw_token = 0;

	for (;;) {
		p = css_skip_blank(p, end);
		if (p >= end) {
			break;
		}
		saw_token = 1;

		if (*p == '(') {
			const char	*close = css_skip_balanced(p, end);

			if (close <= p + 1) {
				return (0);
			}
			if (css_media_feature(p + 1, close - 1, vw, vh) == 0) {
				result = 0;
			}
			p = close;
			continue;
		}

		n = 0;
		while (p < end && css_is_ident_char(*p) != 0 &&
		    n < sizeof(word) - 1) {
			word[n++] = *p++;
		}
		word[n] = '\0';
		if (n == 0) {
			return (0);	/* junk in the prelude */
		}

		if (strcasecmp(word, "not") == 0) {
			negate = 1;
			continue;
		}
		if (strcasecmp(word, "only") == 0 ||
		    strcasecmp(word, "and") == 0) {
			/* "and" is implicit here; conditions already AND. */
			continue;
		}
		if (strcasecmp(word, "screen") == 0 ||
		    strcasecmp(word, "all") == 0) {
			continue;
		}
		/* print, speech, tv, projection: not this device. */
		result = 0;
	}

	if (saw_token == 0) {
		return (1);
	}
	return (negate ? !result : result);
}

/* The whole prelude: a comma list, true when any one query is true. */
static int
css_media_match(const char *p, const char *end, int32_t vw, int32_t vh)
{
	const char	*comma;

	p = css_skip_blank(p, end);
	if (p >= end) {
		return (1);	/* "@media { }" applies everywhere */
	}
	while (p < end) {
		comma = memchr(p, ',', (size_t)(end - p));
		if (comma == NULL) {
			comma = end;
		}
		if (css_media_query_match(p, comma, vw, vh) != 0) {
			return (1);
		}
		p = comma + 1;
	}
	return (0);
}

static int	css_parse_range(css_sheet_t *sheet, const char *text,
		    const char *end, int32_t vw, int32_t vh, int *order,
		    int depth_left);

/*
 * Parses one rule range into `sheet`.  Recurses once per nested @media, which
 * is what depth_left bounds: a stylesheet nesting @media into itself forever
 * would otherwise recurse until the stack ran out.
 */
static int
css_parse_range(css_sheet_t *sheet, const char *text, const char *end,
    int32_t vw, int32_t vh, int *order, int depth_left)
{
	const char	*p;
	int		i;

	p = text;

	while (p < end) {
		const char	*sel_start, *sel_end, *body_start, *body_end;
		css_rule_t	rule;
		int		depth;

		p = css_skip_blank(p, end);
		if (p >= end) {
			break;
		}

		if (*p == '@') {
			const char	*at_name, *prelude;
			int		is_media;

			p++;
			at_name = p;
			while (p < end && css_is_ident_char(*p) != 0) {
				p++;
			}
			is_media = ((size_t)(p - at_name) == 5 &&
			    strncasecmp(at_name, "media", 5) == 0);

			prelude = p;
			while (p < end && *p != ';' && *p != '{' && *p != '}') {
				p++;
			}
			if (p >= end || *p != '{') {
				/* @import/@charset: statement, no block. */
				if (p < end) {
					p++;
				}
				continue;
			}

			body_start = p + 1;
			depth = 1;
			body_end = body_start;
			while (body_end < end && depth > 0) {
				if (*body_end == '{') {
					depth++;
				} else if (*body_end == '}') {
					depth--;
					if (depth == 0) {
						break;
					}
				}
				body_end++;
			}

			/*
			 * Only @media is entered.  @supports, @keyframes,
			 * @font-face and @page either gate on features this
			 * engine does not implement or carry declarations that
			 * are not rules, so their bodies stay skipped.
			 */
			if (is_media != 0 && depth_left > 0 &&
			    css_media_match(prelude, p, vw, vh) != 0) {
				(void)css_parse_range(sheet, body_start,
				    body_end, vw, vh, order, depth_left - 1);
			}
			p = (body_end < end) ? body_end + 1 : end;
			continue;
		}

		sel_start = p;
		while (p < end && *p != '{' && *p != '}') {
			p++;
		}
		if (p >= end || *p == '}') {
			if (p < end) {
				p++;
			}
			continue;
		}
		sel_end = p;
		body_start = p + 1;

		depth = 1;
		body_end = body_start;
		while (body_end < end && depth > 0) {
			if (*body_end == '{') {
				depth++;
			} else if (*body_end == '}') {
				depth--;
				if (depth == 0) {
					break;
				}
			}
			body_end++;
		}

		memset(&rule, 0, sizeof(rule));
		{
			css_selector_t	parsed[CSS_MAX_SELECTORS];
			int		nparsed = 0;
			const char	*sp = sel_start;

			while (sp < sel_end && nparsed < CSS_MAX_SELECTORS) {
				if (css_selector_parse(&sp, sel_end,
				    &parsed[nparsed]) != 0) {
					css_selector_free(
					    &parsed[nparsed]);
					while (sp < sel_end && *sp != ',') {
						sp++;
					}
					if (sp < sel_end) {
						sp++;
					}
					continue;
				}
				nparsed++;
				if (sp < sel_end && *sp == ',') {
					sp++;
				}
			}

			if (nparsed > 0) {
				rule.sels = (css_selector_t *)malloc(
				    (size_t)nparsed *
				    sizeof(css_selector_t));
				if (rule.sels != NULL) {
					memcpy(rule.sels, parsed,
					    (size_t)nparsed *
					    sizeof(css_selector_t));
					rule.nsels = nparsed;
				} else {
					for (i = 0; i < nparsed; i++) {
						css_selector_free(
						    &parsed[i]);
					}
				}
			}
		}

		if (rule.nsels > 0) {
			const char	*btrim = body_end;

			(void)btrim;
			if (css_decls_parse(body_start, body_end,
			    &rule.decls, &rule.ndecls) == 0 &&
			    rule.ndecls > 0) {
				/*
				 * Order is threaded through the recursion, not
				 * restarted per @media block: a rule inside
				 * @media must still lose to an equally specific
				 * rule that comes after it in source order.
				 */
				rule.order = (*order)++;
				if (css_rule_add(sheet, &rule) != 0) {
					css_rule_free(&rule);
				}
			} else {
				css_rule_free(&rule);
			}
		} else {
			css_rule_free(&rule);
		}

		p = (body_end < end && depth == 0) ? body_end + 1 : end;
	}

	return (0);
}

int
css_parse_ex(const char *text, int32_t viewport_w, int32_t viewport_h,
    css_sheet_t **out)
{
	css_sheet_t	*sheet;
	int		order;

	if (out == NULL) {
		return (-1);
	}
	*out = NULL;
	if (text == NULL) {
		return (-1);
	}

	sheet = (css_sheet_t *)malloc(sizeof(css_sheet_t));
	if (sheet == NULL) {
		return (-1);
	}
	memset(sheet, 0, sizeof(*sheet));

	order = 0;
	if (css_parse_range(sheet, text, text + strlen(text), viewport_w,
	    viewport_h, &order, CSS_MAX_AT_DEPTH) != 0) {
		css_free(sheet);
		return (-1);
	}

	*out = sheet;
	return (0);
}

int
css_parse(const char *text, css_sheet_t **out)
{
	/*
	 * No viewport supplied, so answer media queries as if the window were
	 * the default browser size.  Callers that know their real width should
	 * use css_parse_ex(): guessing here makes (max-width: 600px) blocks
	 * apply on a window that is actually 1024px wide.
	 */
	return (css_parse_ex(text, CSS_DEFAULT_VIEWPORT_W,
	    CSS_DEFAULT_VIEWPORT_H, out));
}
