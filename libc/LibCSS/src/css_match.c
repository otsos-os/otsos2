/* !DEFINES!

$define %func css_compute as function with args const css_sheet *, const css_node_iface *, const void *, css_computed *
$define %func css_apply_declarations as function with args css_computed *, const char *, int32_t

*/

/* !SPACE!

$space %internal css_compound_match, css_chain_match, css_chain_step
$space %internal css_selector_match, css_rule_candidates
$space %internal css_candidate_t, css_value_color, css_value_scale
$space %internal css_class_tokens_match

*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "css_int.h"

static double
css_num(const char **pp)
{
	const char	*p = *pp;
	double		v = 0.0;
	int		neg = 0, digits = 0;

	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (*p == '-') {
		neg = 1;
		p++;
	} else if (*p == '+') {
		p++;
	}
	while (isdigit((unsigned char)*p)) {
		v = v * 10.0 + (*p - '0');
		digits++;
		p++;
	}
	if (*p == '.') {
		double	frac = 0.1;

		p++;
		while (isdigit((unsigned char)*p)) {
			v += (*p - '0') * frac;
			frac *= 0.1;
			digits++;
			p++;
		}
	}
	if (digits == 0) {
		return (0.0);
	}
	*pp = p;
	return (neg ? -v : v);
}

typedef struct css_color_name {
	const char	*name;
	uint32_t	rgb;
} css_color_name_t;

static const css_color_name_t css_colors[] = {
	{ "black",	0x000000 },
	{ "white",	0xffffff },
	{ "red",	0xff0000 },
	{ "green",	0x008000 },
	{ "lime",	0x00ff00 },
	{ "blue",	0x0000ff },
	{ "yellow",	0xffff00 },
	{ "magenta",	0xff00ff },
	{ "fuchsia",	0xff00ff },
	{ "cyan",	0x00ffff },
	{ "aqua",	0x00ffff },
	{ "silver",	0xc0c0c0 },
	{ "gray",	0x808080 },
	{ "grey",	0x808080 },
	{ "maroon",	0x800000 },
	{ "olive",	0x808000 },
	{ "purple",	0x800080 },
	{ "teal",	0x008080 },
	{ "navy",	0x000080 },
	{ "orange",	0xffa500 },
	{ "pink",	0xffc0cb },
	{ "brown",	0xa52a2a },
	{ NULL,		0 }
};

static const char *
css_ci_str(const char *hay, const char *needle)
{
	size_t	nl = strlen(needle);

	for (; *hay != '\0'; hay++) {
		if (strncasecmp(hay, needle, nl) == 0) {
			return (hay);
		}
	}
	return (NULL);
}

int
css_color_parse(const char *str, uint32_t *out_rgba)
{
	const char	*p;
	int		i, v, n;
	uint32_t	wide, comp[3];

	p = str;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (strncasecmp(p, "none", 4) == 0 || *p == '\0') {
		return (-1);
	}

	if (*p == '#') {
		p++;
		n = 0;
		while (isxdigit((unsigned char)p[n]) && n < 6) {
			n++;
		}
		if (n != 6 && n != 3) {
			return (-1);
		}
		wide = 0;
		for (i = 0; i < n; i++) {
			v = p[i];
			if (v >= '0' && v <= '9') {
				v -= '0';
			} else if (v >= 'a' && v <= 'f') {
				v -= 'a' - 10;
			} else {
				v -= 'A' - 10;
			}
			wide = (wide << 4) | (uint32_t)(unsigned)v;
		}
		if (n == 3) {
			uint32_t	r = (wide >> 8) & 0xf;
			uint32_t	g = (wide >> 4) & 0xf;
			uint32_t	b = wide & 0xf;

			wide = ((r * 0x11) << 16) | ((g * 0x11) << 8) |
			    (b * 0x11);
		}
		*out_rgba = 0xff000000u | wide;
		return (0);
	}

	if (strncasecmp(p, "rgb(", 4) == 0 || strncasecmp(p, "rgba(", 5) == 0) {
		const char	*q;

		q = strchr(p, '(');
		q++;
		for (i = 0; i < 3; i++) {
			double	val;

			val = css_num(&q);
			while (*q == ' ') {
				q++;
			}
			if (*q == '%') {
				val *= 2.55;
				q++;
			}
			if (val < 0.0) {
				val = 0.0;
			}
			if (val > 255.0) {
				val = 255.0;
			}
			comp[i] = (uint32_t)val;
			while (*q == ' ') {
				q++;
			}
			if (*q == ',') {
				q++;
			}
		}
		*out_rgba = 0xff000000u | (comp[0] << 16) |
		    (comp[1] << 8) | comp[2];
		return (0);
	}

	for (i = 0; css_colors[i].name != NULL; i++) {
		if (strcasecmp(p, css_colors[i].name) == 0) {
			*out_rgba = 0xff000000u | css_colors[i].rgb;
			return (0);
		}
	}
	return (-1);
}

static int
css_scale_keyword(const char *str, int32_t *out)
{
	static const struct { const char *name; int32_t sc; } kw[] = {
		{ "xx-small",	1 }, { "x-small", 1 }, { "small", 1 },
		{ "medium",	2 }, { "large", 3 },
		{ "x-large",	4 }, { "xx-large", 5 },
	};
	int	i;

	for (i = 0; i < 7; i++) {
		if (strcasecmp(str, kw[i].name) == 0) {
			*out = kw[i].sc;
			return (0);
		}
	}
	return (-1);
}

static int
css_value_scale(const char *str, int32_t base_scale, int32_t *out)
{
	const char	*p = str;
	double		px;

	if (css_scale_keyword(str, out) == 0) {
		return (0);
	}
	if (strcasecmp(p, "larger") == 0) {
		*out = base_scale + 1;
		goto clamp;
	}
	if (strcasecmp(p, "smaller") == 0) {
		*out = base_scale - 1;
		goto clamp;
	}

	px = css_num(&p);
	if (px <= 0.0) {
		return (-1);
	}
	while (*p == ' ') {
		p++;
	}
	if (strcasecmp(p, "pt") == 0) {
		px = px * 96.0 / 72.0;
	} else if (*p == '%') {
		px = (double)base_scale * 6.0 * px / 100.0;
	} else if (*p != '\0' && strcasecmp(p, "px") != 0) {
		return (-1);
	}
	*out = (int32_t)((px * 2.0 + 7.0) / 14.0);
clamp:
	if (*out < 1) {
		*out = 1;
	}
	if (*out > 6) {
		*out = 6;
	}
	return (0);
}

int
css_apply_declarations(css_computed_t *out, const char *decl_text,
    int32_t base_scale)
{
	const char	*p = decl_text;
	const char	*seg_end;
	char		prop[32], value[128];

	if (out == NULL || decl_text == NULL) {
		return (-1);
	}

	while (*p != '\0') {
		const char	*colon;
		size_t		plen = 0, vlen = 0;

		while (*p == ' ' || *p == ';' || *p == '\t' ||
		    *p == '\n' || *p == '\r') {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		seg_end = strchr(p, ';');
		if (seg_end == NULL) {
			seg_end = p + strlen(p);
		}
		colon = memchr(p, ':', (size_t)(seg_end - p));
		if (colon != NULL) {
			const char	*q = p;

			while (q < colon && plen < sizeof(prop) - 1 &&
			    *q != ' ') {
				prop[plen++] = *q++;
			}
			prop[plen] = '\0';
			q = colon + 1;
			while (q < seg_end && (*q == ' ' || *q == '\t')) {
				q++;
			}
			while (q < seg_end && vlen < sizeof(value) - 1 &&
			    *q != '\n' && *q != '\r') {
				value[vlen++] = *q++;
			}
			value[vlen] = '\0';

			if (plen != 0 && vlen != 0) {
				uint32_t	color;

				if (strcasecmp(prop, "color") == 0 &&
				    css_color_parse(value, &color) == 0) {
					out->set |= CSS_PROP_COLOR;
					out->color = color;
				} else if (strcasecmp(prop,
				    "font-size") == 0 &&
				    css_value_scale(value, base_scale,
				    &out->font_scale) == 0) {
					out->set |= CSS_PROP_SCALE;
				} else if (strcasecmp(prop,
				    "font-weight") == 0) {
					if (strcasecmp(value, "bold") == 0 ||
					    strcasecmp(value,
					    "bolder") == 0 ||
					    (value[0] >= '6' &&
					    isdigit((unsigned char)value[0])))
					{
						out->set |= CSS_PROP_BOLD;
						out->bold = 1;
					} else if (strcasecmp(value,
					    "normal") == 0 ||
					    strcasecmp(value,
					    "lighter") == 0) {
						out->set |= CSS_PROP_BOLD;
						out->bold = 0;
					}
				} else if (strcasecmp(prop,
				    "text-decoration") == 0) {
					if (css_ci_str(value,
					    "underline") != NULL) {
						out->set |= CSS_PROP_UNDERLINE;
						out->underline = 1;
					}
					if (css_ci_str(value,
					    "line-through") != NULL) {
						out->set |= CSS_PROP_STRIKE;
						out->strike = 1;
					}
					if (strcasecmp(value, "none") == 0) {
						out->set |=
						    CSS_PROP_UNDERLINE |
						    CSS_PROP_STRIKE;
						out->underline = 0;
						out->strike = 0;
					}
				} else if (strcasecmp(prop, "display") == 0) {
					out->set |= CSS_PROP_DISPLAY_NONE;
					out->display_none =
					    (strcasecmp(value, "none") == 0);
				}
			}
		}
		p = (*seg_end != '\0') ? seg_end + 1 : seg_end;
	}
	return (0);
}

static int
css_class_tokens_match(const css_compound_t *cp, const char *elem_classes)
{
	int	k;
	const char	*c;

	for (k = 0; k < cp->ncls; k++) {
		const char	*e = elem_classes;
		size_t		nl = strlen(cp->cls[k]);
		int	found = 0;

		while (*e != '\0') {
			while (*e == ' ' || *e == '\t' || *e == '\n' ||
			    *e == '\r') {
				e++;
			}
			c = e;
			while (*e != '\0' && *e != ' ' && *e != '\t' &&
			    *e != '\n' && *e != '\r') {
				e++;
			}
			if ((size_t)(e - c) == nl &&
			    strncasecmp(c, cp->cls[k], nl) == 0) {
				found = 1;
				break;
			}
		}
		if (found == 0) {
			return (0);
		}
	}
	return (1);
}

static int
css_compound_match(const css_compound_t *cp, const css_node_iface_t *ifc,
    const void *node)
{
	const char	*v;

	if (cp->tag != NULL) {
		const char	*t = ifc->tag(node);

		if (t == NULL || strcasecmp(t, cp->tag) != 0) {
			return (0);
		}
	}
	if (cp->id != NULL) {
		v = ifc->get_attr(node, "id");
		if (v == NULL || strcmp(v, cp->id) != 0) {
			return (0);
		}
	}
	if (cp->ncls > 0) {
		v = ifc->get_attr(node, "class");
		if (v == NULL || css_class_tokens_match(cp, v) == 0) {
			return (0);
		}
	}
	return (1);
}

static int
css_chain_step(const css_selector_t *sel, int last, const css_node_iface_t *ifc,
    const void *node, int *steps)
{
	if (*steps <= 0) {
		return (0);
	}
	(*steps)--;

	if (css_compound_match(&sel->parts[last], ifc, node) == 0) {
		return (0);
	}
	if (last == 0) {
		return (1);
	}
	{
		const void	*a;

		for (a = ifc->parent(node); a != NULL;
		    a = ifc->parent(a)) {
			if (css_chain_step(sel, last - 1, ifc, a,
			    steps) != 0) {
				return (1);
			}
		}
	}
	return (0);
}

static int
css_selector_match(const css_selector_t *sel, const css_node_iface_t *ifc,
    const void *node, int *steps)
{
	return (css_chain_step(sel, sel->nparts - 1, ifc, node, steps) != 0);
}

typedef struct css_candidate {
	int	spec_ids;
	int	spec_classes;
	int	spec_types;
	int	order;
	int	rule;
} css_candidate_t;

static int
css_candidate_cmp(const void *pa, const void *pb)
{
	const css_candidate_t	*a = pa;
	const css_candidate_t	*b = pb;

	if (a->spec_ids != b->spec_ids) {
		return (a->spec_ids - b->spec_ids);
	}
	if (a->spec_classes != b->spec_classes) {
		return (a->spec_classes - b->spec_classes);
	}
	if (a->spec_types != b->spec_types) {
		return (a->spec_types - b->spec_types);
	}
	return (a->order - b->order);
}

static int
css_rule_candidates(const css_sheet_t *sheet, const css_node_iface_t *ifc,
    const void *node, css_candidate_t *cands, int max_cands)
{
	int	i, j, n = 0;

	for (i = 0; i < sheet->nrules; i++) {
		const css_rule_t	*rule = &sheet->rules[i];
		css_selector_t		*best = NULL;
		int			steps = CSS_MATCH_STEPS_MAX;

		for (j = 0; j < rule->nsels; j++) {
			css_selector_t	*s = &rule->sels[j];

			steps = CSS_MATCH_STEPS_MAX;
			if (css_selector_match(s, ifc, node, &steps) == 0) {
				continue;
			}
			if (best == NULL ||
			    s->spec_ids > best->spec_ids ||
			    (s->spec_ids == best->spec_ids &&
			    (s->spec_classes > best->spec_classes ||
			    (s->spec_classes == best->spec_classes &&
			    s->spec_types > best->spec_types)))) {
				best = s;
			}
		}
		if (best != NULL && n < max_cands) {
			cands[n].spec_ids = best->spec_ids;
			cands[n].spec_classes = best->spec_classes;
			cands[n].spec_types = best->spec_types;
			cands[n].order = rule->order;
			cands[n].rule = i;
			n++;
		}
	}
	return (n);
}

static void
css_apply_rule(const css_rule_t *rule, css_computed_t *cc, int32_t base_scale)
{
	int	i;
	char	buf[256];
	size_t	used = 0;

	for (i = 0; i < rule->ndecls; i++) {
		int		pl = snprintf(buf + used, sizeof(buf) - used,
				    "%s:%s;", rule->decls[i].prop,
				    rule->decls[i].value);

		if (pl < 0 || (size_t)pl >= sizeof(buf) - used) {
			break;
		}
		used += (size_t)pl;
	}
	css_apply_declarations(cc, buf, base_scale);
}

int
css_compute(const css_sheet_t *sheet, const css_node_iface_t *iface,
    const void *node, css_computed_t *out)
{
	const void		*chain[CSS_MAX_DEPTH];
	const void		*a;
	css_candidate_t		cands[CSS_MAX_RULES > 256 ? 256 : 64];
	css_computed_t		inherited;
	int			depth, i, top;

	if (sheet == NULL || iface == NULL || node == NULL || out == NULL) {
		return (-1);
	}

	memset(out, 0, sizeof(*out));

	depth = 0;
	a = node;
	while (a != NULL && depth < CSS_MAX_DEPTH) {
		chain[depth++] = a;
		a = iface->parent(a);
	}
	top = depth - 1;

	memset(&inherited, 0, sizeof(inherited));
	for (i = top; i >= 0; i--) {
		css_computed_t	cur = inherited;
		int		nc, k;
		int32_t		base_scale;

		base_scale = 2;
		if ((cur.set & CSS_PROP_SCALE) != 0) {
			base_scale = cur.font_scale;
		}

		nc = css_rule_candidates(sheet, iface, chain[i], cands,
		    (int)(sizeof(cands) / sizeof(cands[0])));
		qsort(cands, (size_t)nc, sizeof(cands[0]), css_candidate_cmp);
		for (k = 0; k < nc; k++) {
			css_apply_rule(&sheet->rules[cands[k].rule], &cur,
			    base_scale);
		}

		{
			const char	*st = iface->get_attr(chain[i],
					    "style");

			if (st != NULL) {
				css_apply_declarations(&cur, st,
				    base_scale);
			}
		}

		out->set |= cur.set;
		if ((cur.set & CSS_PROP_COLOR) != 0) {
			out->color = cur.color;
		}
		if ((cur.set & CSS_PROP_SCALE) != 0) {
			out->font_scale = cur.font_scale;
		}
		if ((cur.set & CSS_PROP_BOLD) != 0) {
			out->bold = cur.bold;
		}
		if ((cur.set & CSS_PROP_UNDERLINE) != 0) {
			out->underline = cur.underline;
		}
		if ((cur.set & CSS_PROP_STRIKE) != 0) {
			out->strike = cur.strike;
		}
		if ((cur.set & CSS_PROP_DISPLAY_NONE) != 0) {
			out->set |= CSS_PROP_DISPLAY_NONE;
			out->display_none = cur.display_none;
		}

		memset(&inherited, 0, sizeof(inherited));
		if ((cur.set & CSS_PROP_COLOR) != 0) {
			inherited.set |= CSS_PROP_COLOR;
			inherited.color = cur.color;
		}
		if ((cur.set & CSS_PROP_SCALE) != 0) {
			inherited.set |= CSS_PROP_SCALE;
			inherited.font_scale = cur.font_scale;
		}
	}
	return (0);
}
