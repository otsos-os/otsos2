/* !DEFINES!

$define %func css_compute as function with args const css_sheet *, const css_node_iface *, const void *, css_computed *
$define %func css_apply_declarations as function with args css_computed *, const char *, int32_t

*/

/* !SPACE!

$space %internal css_compound_match, css_chain_match, css_chain_step
$space %internal css_selector_match, css_rule_candidates
$space %internal css_candidate_t, css_value_color, css_value_scale
$space %internal css_class_tokens_match, css_attr_match
$space %internal css_value_px_cap, css_value_len, css_box_values
$space %internal css_box_expand, css_func_color, css_background_color
$space %internal css_apply_border, css_apply_prop, css_apply_prop_text

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

	/*
	 * Transparent is a real value, not a parse failure: "background:
	 * transparent" on a rule that beats an earlier "background: #ddd" has
	 * to clear it.  Alpha 0 is how that travels; every painter here treats
	 * alpha 0 as "do not fill".
	 */
	if (strcasecmp(p, "transparent") == 0) {
		*out_rgba = 0;
		return (0);
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
		uint32_t	alpha;

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
			if (*q == ',' || *q == '/') {
				q++;
			}
		}

		/*
		 * Alpha is read but not blended: there is no compositing here,
		 * so it collapses to draw/do-not-draw.  Half is the threshold
		 * because rgba(0,0,0,.87) is body text and rgba(0,0,0,.04) is a
		 * hairline nobody misses.
		 */
		alpha = 0xff000000u;
		while (*q == ' ') {
			q++;
		}
		if (isdigit((unsigned char)*q) != 0 || *q == '.') {
			if (css_num(&q) < 0.5) {
				alpha = 0;
			}
		}
		*out_rgba = alpha | (comp[0] << 16) | (comp[1] << 8) | comp[2];
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

/*
 * Absolute length in device pixels, clamped to `cap`.
 *
 * The cap is per-property, not global: a margin over a few hundred pixels is
 * almost certainly a unit this parser misread, while a width of 1200px on a
 * wide window is ordinary.  Sharing one cap made every box the same width.
 *
 * Percentages and viewport units are rejected here; css_value_len() resolves
 * them because it knows the containing width.
 */
static int
css_value_px_cap(const char *str, int32_t cap, int32_t *out)
{
	const char	*p = str;
	double		px;

	px = css_num(&p);
	/*
	 * css_num() leaves p where it found it when there are no digits, and
	 * that is the only way to tell "0px" from a keyword.  Without this test
	 * every non-numeric token parsed as a valid zero, so in
	 * "border: 2px solid #ccc" both "solid" and "#ccc" came back as 0px and
	 * overwrote the width - the border vanished and the colour was never
	 * even offered to the colour parser.
	 */
	if (p == str) {
		return (-1);
	}
	while (*p == ' ') {
		p++;
	}
	if (strcasecmp(p, "pt") == 0) {
		px = px * 96.0 / 72.0;
	} else if (strcasecmp(p, "em") == 0 || strcasecmp(p, "rem") == 0) {
		px = px * (double)CSS_BASE_EM_PX;
	} else if (*p == '%' || (*p != '\0' && strcasecmp(p, "px") != 0)) {
		return (-1);
	}
	/* No property routed here accepts a negative length. */
	if (px < 0.0) {
		px = 0.0;
	}
	if (px > (double)cap) {
		px = (double)cap;
	}
	*out = (int32_t)(px + 0.5);
	return (0);
}

static int
css_value_px(const char *str, int32_t *out)
{
	return (css_value_px_cap(str, CSS_MARGIN_MAX_PX, out));
}

/*
 * Length that may be a percentage.  On success *out holds pixels and *pct is
 * 0, or *out holds the percentage and *pct is 1 - the caller resolves it
 * against whatever box it is actually placing.
 */
static int
css_value_len(const char *str, int32_t cap, int32_t *out, int *pct)
{
	const char	*p = str;
	double		v;

	*pct = 0;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (strcasecmp(p, "auto") == 0 || strcasecmp(p, "inherit") == 0 ||
	    strcasecmp(p, "initial") == 0) {
		return (-1);	/* leave the property unset, use the default */
	}

	{
		const char	*num = p;

		v = css_num(&p);
		if (p == num) {
			return (-1);	/* no digits: not a length at all */
		}
	}
	while (*p == ' ') {
		p++;
	}
	if (*p == '%') {
		if (v < 0.0 || v > 100.0) {
			return (-1);
		}
		*out = (int32_t)(v + 0.5);
		*pct = 1;
		return (0);
	}
	return (css_value_px_cap(str, cap, out));
}

/*
 * Splits one space-separated run into at most `max` length tokens.  Shared by
 * the margin/padding 1-4 value shorthands, which differ only in where the
 * results land.  Returns how many parsed.
 */
static int
css_box_values(const char *value, int32_t cap, int32_t *vals, int max)
{
	char		tok[40];
	const char	*t = value;
	int		n = 0;

	while (n < max && *t != '\0') {
		const char	*e = t;
		size_t		len;

		while (*e != '\0' && *e != ' ' && *e != '\t') {
			e++;
		}
		len = (size_t)(e - t);
		if (len != 0 && len < sizeof(tok)) {
			memcpy(tok, t, len);
			tok[len] = '\0';
			if (css_value_px_cap(tok, cap, &vals[n]) == 0) {
				n++;
			}
		}
		while (*e == ' ' || *e == '\t') {
			e++;
		}
		t = e;
	}
	return (n);
}

/* CSS 1-4 value expansion: all / vert horz / top horz bottom / t r b l. */
static void
css_box_expand(const int32_t *v, int n, int32_t *top, int32_t *right,
    int32_t *bottom, int32_t *left)
{
	*top = v[0];
	*right = v[0];
	*bottom = v[0];
	*left = v[0];
	if (n >= 2) {
		*right = v[1];
		*left = v[1];
	}
	if (n >= 3) {
		*bottom = v[2];
	}
	if (n >= 4) {
		*left = v[3];
	}
}

/*
 * Pulls a colour out of a functional value, e.g. the first stop of
 * linear-gradient(to bottom, #f8f8f8, #e0e0e0).
 *
 * A gradient cannot be drawn here, but its first stop is a far better button
 * face than falling back to the page background, which is what ignoring the
 * whole declaration produces.
 */
static int
css_func_color(const char *value, uint32_t *out)
{
	char		tok[64];
	const char	*p, *t;
	int		depth;

	p = strchr(value, '(');
	if (p == NULL) {
		return (-1);
	}
	p++;
	depth = 1;

	while (*p != '\0' && depth > 0) {
		while (*p == ' ' || *p == '\t' || *p == ',') {
			p++;
		}
		t = p;
		while (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t') {
			if (*p == '(') {
				depth++;
			} else if (*p == ')') {
				depth--;
				if (depth == 0) {
					break;
				}
			}
			p++;
		}
		if ((size_t)(p - t) != 0 && (size_t)(p - t) < sizeof(tok)) {
			memcpy(tok, t, (size_t)(p - t));
			tok[p - t] = '\0';
			if (css_color_parse(tok, out) == 0) {
				return (0);
			}
		}
		if (*p == ')') {
			break;
		}
	}
	return (-1);
}

/*
 * The "background" shorthand.  Only the colour component is extracted; images,
 * positions and repeat modes have no representation downstream.
 *
 * The colour is looked for token by token rather than by handing the whole
 * value to css_color_parse, because "url(x.png) no-repeat #fff" has the colour
 * last and "#fff url(x.png)" has it first.
 */
static int
css_background_color(const char *value, uint32_t *out)
{
	char		tok[80];
	const char	*p = value;

	if (css_color_parse(value, out) == 0) {
		return (0);
	}

	while (*p != '\0') {
		const char	*t;
		int		depth = 0;
		size_t		len;

		while (*p == ' ' || *p == '\t' || *p == ',') {
			p++;
		}
		t = p;
		/* A function is one token: rgb(1, 2, 3) must not split. */
		while (*p != '\0' && (depth > 0 || (*p != ' ' && *p != '\t'))) {
			if (*p == '(') {
				depth++;
			} else if (*p == ')') {
				depth--;
			}
			p++;
		}
		len = (size_t)(p - t);
		if (len == 0) {
			continue;
		}
		if (len < sizeof(tok)) {
			memcpy(tok, t, len);
			tok[len] = '\0';
			if (strncasecmp(tok, "url(", 4) == 0) {
				continue;
			}
			if (css_color_parse(tok, out) == 0) {
				return (0);
			}
			if (css_func_color(tok, out) == 0) {
				return (0);
			}
		}
	}
	return (-1);
}

/* "1px solid #ccc" in any order; each component is optional. */
static void
css_apply_border(css_computed_t *out, const char *value)
{
	char		tok[64];
	const char	*p = value;
	uint32_t	color;
	int32_t		px;
	int		got_width, none;

	got_width = 0;
	none = 0;

	while (*p != '\0') {
		const char	*t;
		int		depth = 0;
		size_t		len;

		while (*p == ' ' || *p == '\t') {
			p++;
		}
		t = p;
		while (*p != '\0' && (depth > 0 || (*p != ' ' && *p != '\t'))) {
			if (*p == '(') {
				depth++;
			} else if (*p == ')') {
				depth--;
			}
			p++;
		}
		len = (size_t)(p - t);
		if (len == 0 || len >= sizeof(tok)) {
			continue;
		}
		memcpy(tok, t, len);
		tok[len] = '\0';

		if (strcasecmp(tok, "none") == 0 ||
		    strcasecmp(tok, "hidden") == 0) {
			none = 1;
			continue;
		}
		if (strcasecmp(tok, "solid") == 0 ||
		    strcasecmp(tok, "dotted") == 0 ||
		    strcasecmp(tok, "dashed") == 0 ||
		    strcasecmp(tok, "double") == 0 ||
		    strcasecmp(tok, "groove") == 0 ||
		    strcasecmp(tok, "ridge") == 0 ||
		    strcasecmp(tok, "inset") == 0 ||
		    strcasecmp(tok, "outset") == 0) {
			/*
			 * Style is recognised but discarded: every border is
			 * painted solid.  Recognising it matters anyway, or
			 * "solid" would fall through to the colour parser.
			 */
			if (got_width == 0) {
				out->set |= CSS_PROP_BORDER;
				out->border_width = 1;
				got_width = 1;
			}
			continue;
		}
		if (strcasecmp(tok, "thin") == 0) {
			px = 1;
		} else if (strcasecmp(tok, "medium") == 0) {
			px = 2;
		} else if (strcasecmp(tok, "thick") == 0) {
			px = 3;
		} else if (css_value_px_cap(tok, CSS_BORDER_MAX_PX, &px) != 0) {
			if (css_color_parse(tok, &color) == 0) {
				out->set |= CSS_PROP_BORDER_COLOR;
				out->border_color = color;
			}
			continue;
		}
		out->set |= CSS_PROP_BORDER;
		out->border_width = px;
		got_width = 1;
	}

	if (none != 0) {
		out->set |= CSS_PROP_BORDER;
		out->border_width = 0;
	}
}

/*
 * Applies one property.  Split out of the declaration loop so css_apply_rule()
 * can feed pre-split pairs straight in; concatenating them back into one string
 * meant a rule with many declarations lost its tail to the buffer size.
 */
static void
css_apply_prop(css_computed_t *out, const char *prop, const char *value,
    int32_t base_scale)
{
	uint32_t	color;
	int32_t		v[4], px;
	int		n, pct;

	if (strcasecmp(prop, "color") == 0) {
		if (css_color_parse(value, &color) == 0) {
			out->set |= CSS_PROP_COLOR;
			out->color = color;
		}
		return;
	}
	if (strcasecmp(prop, "background-color") == 0 ||
	    strcasecmp(prop, "background") == 0) {
		if (css_background_color(value, &color) == 0) {
			out->set |= CSS_PROP_BGCOLOR;
			out->bgcolor = color;
		} else if (strcasecmp(value, "none") == 0) {
			out->set |= CSS_PROP_BGCOLOR;
			out->bgcolor = 0;
		}
		return;
	}
	if (strcasecmp(prop, "text-align") == 0) {
		out->set |= CSS_PROP_ALIGN_CENTER;
		out->align_center = (strcasecmp(value, "center") == 0);
		return;
	}
	if (strcasecmp(prop, "margin-top") == 0) {
		if (css_value_px(value, &px) == 0) {
			out->set |= CSS_PROP_MARGIN_TOP;
			out->margin_top = px;
		}
		return;
	}
	if (strcasecmp(prop, "margin-bottom") == 0) {
		if (css_value_px(value, &px) == 0) {
			out->set |= CSS_PROP_MARGIN_BOTTOM;
			out->margin_bottom = px;
		}
		return;
	}
	if (strcasecmp(prop, "margin") == 0) {
		n = css_box_values(value, CSS_MARGIN_MAX_PX, v, 4);
		if (n >= 1) {
			int32_t	t, r, b, l;

			css_box_expand(v, n, &t, &r, &b, &l);
			out->set |= CSS_PROP_MARGIN_TOP |
			    CSS_PROP_MARGIN_BOTTOM;
			out->margin_top = t;
			out->margin_bottom = b;
		}
		return;
	}
	if (strcasecmp(prop, "padding") == 0) {
		n = css_box_values(value, CSS_PAD_MAX_PX, v, 4);
		if (n >= 1) {
			out->set |= CSS_PROP_PADDING;
			css_box_expand(v, n, &out->pad_top, &out->pad_right,
			    &out->pad_bottom, &out->pad_left);
		}
		return;
	}
	if (strncasecmp(prop, "padding-", 8) == 0) {
		if (css_value_px_cap(value, CSS_PAD_MAX_PX, &px) != 0) {
			return;
		}
		out->set |= CSS_PROP_PADDING;
		if (strcasecmp(prop + 8, "top") == 0) {
			out->pad_top = px;
		} else if (strcasecmp(prop + 8, "right") == 0) {
			out->pad_right = px;
		} else if (strcasecmp(prop + 8, "bottom") == 0) {
			out->pad_bottom = px;
		} else if (strcasecmp(prop + 8, "left") == 0) {
			out->pad_left = px;
		}
		return;
	}
	if (strcasecmp(prop, "width") == 0) {
		if (css_value_len(value, CSS_SIZE_MAX_PX, &px, &pct) == 0) {
			out->set |= CSS_PROP_WIDTH;
			out->width = px;
			out->width_pct = pct;
		}
		return;
	}
	if (strcasecmp(prop, "height") == 0) {
		if (css_value_len(value, CSS_SIZE_MAX_PX, &px, &pct) == 0) {
			out->set |= CSS_PROP_HEIGHT;
			out->height = px;
			out->height_pct = pct;
		}
		return;
	}
	if (strcasecmp(prop, "border") == 0) {
		css_apply_border(out, value);
		return;
	}
	if (strcasecmp(prop, "border-width") == 0) {
		n = css_box_values(value, CSS_BORDER_MAX_PX, v, 4);
		if (n >= 1) {
			out->set |= CSS_PROP_BORDER;
			out->border_width = v[0];
		}
		return;
	}
	if (strcasecmp(prop, "border-color") == 0) {
		if (css_color_parse(value, &color) == 0) {
			out->set |= CSS_PROP_BORDER_COLOR;
			out->border_color = color;
		}
		return;
	}
	if (strcasecmp(prop, "border-style") == 0) {
		/*
		 * Only the disappearing case is honoured.  A border already
		 * declared with a width stays; "border-style: none" removes it,
		 * which is how reset sheets strip the default control frame.
		 */
		if (strcasecmp(value, "none") == 0 ||
		    strcasecmp(value, "hidden") == 0) {
			out->set |= CSS_PROP_BORDER;
			out->border_width = 0;
		} else if ((out->set & CSS_PROP_BORDER) == 0) {
			out->set |= CSS_PROP_BORDER;
			out->border_width = 1;
		}
		return;
	}
	if (strncasecmp(prop, "border-", 7) == 0) {
		const char	*sub = prop + 7;

		/*
		 * Per-side borders collapse onto the one width this engine
		 * keeps.  Wrong for a table cell with only a bottom rule, but
		 * closer than dropping the declaration: the common author
		 * intent is a visible frame.
		 */
		if (strncasecmp(sub, "top", 3) == 0 ||
		    strncasecmp(sub, "right", 5) == 0 ||
		    strncasecmp(sub, "bottom", 6) == 0 ||
		    strncasecmp(sub, "left", 4) == 0) {
			const char	*dash = strchr(sub, '-');

			if (dash == NULL) {
				css_apply_border(out, value);
			} else if (strcasecmp(dash + 1, "color") == 0) {
				if (css_color_parse(value, &color) == 0) {
					out->set |= CSS_PROP_BORDER_COLOR;
					out->border_color = color;
				}
			} else if (strcasecmp(dash + 1, "width") == 0) {
				if (css_value_px_cap(value, CSS_BORDER_MAX_PX,
				    &px) == 0) {
					out->set |= CSS_PROP_BORDER;
					out->border_width = px;
				}
			}
		}
		return;
	}
	if (strcasecmp(prop, "font-size") == 0) {
		if (css_value_scale(value, base_scale, &out->font_scale) == 0) {
			out->set |= CSS_PROP_SCALE;
		}
		return;
	}
	if (strcasecmp(prop, "font-weight") == 0) {
		if (strcasecmp(value, "bold") == 0 ||
		    strcasecmp(value, "bolder") == 0 ||
		    (value[0] >= '6' && isdigit((unsigned char)value[0]))) {
			out->set |= CSS_PROP_BOLD;
			out->bold = 1;
		} else if (strcasecmp(value, "normal") == 0 ||
		    strcasecmp(value, "lighter") == 0) {
			out->set |= CSS_PROP_BOLD;
			out->bold = 0;
		}
		return;
	}
	if (strcasecmp(prop, "text-decoration") == 0 ||
	    strcasecmp(prop, "text-decoration-line") == 0) {
		if (css_ci_str(value, "underline") != NULL) {
			out->set |= CSS_PROP_UNDERLINE;
			out->underline = 1;
		}
		if (css_ci_str(value, "line-through") != NULL) {
			out->set |= CSS_PROP_STRIKE;
			out->strike = 1;
		}
		if (css_ci_str(value, "none") != NULL) {
			out->set |= CSS_PROP_UNDERLINE | CSS_PROP_STRIKE;
			out->underline = 0;
			out->strike = 0;
		}
		return;
	}
	if (strcasecmp(prop, "display") == 0) {
		out->set |= CSS_PROP_DISPLAY_NONE;
		out->display_none = (strcasecmp(value, "none") == 0);
		return;
	}
	if (strcasecmp(prop, "visibility") == 0) {
		/*
		 * visibility:hidden keeps the box in flow, which this layout
		 * cannot express, so it is folded onto display:none.  The
		 * difference is whitespace; the alternative is drawing menus
		 * that are meant to be invisible.
		 */
		out->set |= CSS_PROP_DISPLAY_NONE;
		out->display_none = (strcasecmp(value, "hidden") == 0);
		return;
	}
}

/*
 * Trims and strips a value in place, then applies it.
 *
 * !important is removed rather than honoured: this cascade has no importance
 * tier.  Leaving it attached made "1px solid red !important" fail colour
 * parsing and drop the whole border.
 */
static void
css_apply_prop_text(css_computed_t *out, const char *prop, char *value,
    int32_t base_scale)
{
	char	*bang;
	size_t	n;

	bang = strchr(value, '!');
	if (bang != NULL) {
		*bang = '\0';
	}
	n = strlen(value);
	while (n > 0 && (value[n - 1] == ' ' || value[n - 1] == '\t' ||
	    value[n - 1] == '\r' || value[n - 1] == '\n')) {
		n--;
	}
	value[n] = '\0';
	if (n == 0) {
		return;
	}
	css_apply_prop(out, prop, value, base_scale);
}

int
css_apply_declarations(css_computed_t *out, const char *decl_text,
    int32_t base_scale)
{
	const char	*p = decl_text;
	const char	*seg_end;
	char		prop[40], value[192];

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
			if (plen != 0) {
				css_apply_prop_text(out, prop, value,
				    base_scale);
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

/* One [attr op value] test against the node's actual attribute value. */
static int
css_attr_match(const css_attr_sel_t *as, const char *have)
{
	size_t	hl, vl;

	if (have == NULL) {
		return (0);
	}
	if (as->op == CSS_ATTR_PRESENT) {
		return (1);
	}
	if (as->value == NULL) {
		return (0);
	}

	hl = strlen(have);
	vl = strlen(as->value);
	/*
	 * An empty operand matches nothing for the substring operators.  CSS
	 * says so explicitly, and without the guard [class*=""] would match
	 * every element on the page.
	 */
	if (vl == 0) {
		return (as->op == CSS_ATTR_EQ && hl == 0);
	}

	switch (as->op) {
	case CSS_ATTR_EQ:
		return (strcasecmp(have, as->value) == 0);
	case CSS_ATTR_INCLUDES: {
		const char	*e = have;

		while (*e != '\0') {
			const char	*t;

			while (*e == ' ' || *e == '\t' || *e == '\n' ||
			    *e == '\r' || *e == '\f') {
				e++;
			}
			t = e;
			while (*e != '\0' && *e != ' ' && *e != '\t' &&
			    *e != '\n' && *e != '\r' && *e != '\f') {
				e++;
			}
			if ((size_t)(e - t) == vl &&
			    strncasecmp(t, as->value, vl) == 0) {
				return (1);
			}
		}
		return (0);
	}
	case CSS_ATTR_DASH:
		if (strncasecmp(have, as->value, vl) != 0) {
			return (0);
		}
		return (have[vl] == '\0' || have[vl] == '-');
	case CSS_ATTR_PREFIX:
		return (hl >= vl && strncasecmp(have, as->value, vl) == 0);
	case CSS_ATTR_SUFFIX:
		return (hl >= vl &&
		    strcasecmp(have + (hl - vl), as->value) == 0);
	case CSS_ATTR_SUBSTR:
		return (css_ci_str(have, as->value) != NULL);
	case CSS_ATTR_PRESENT:
	default:
		return (1);
	}
}

static int
css_compound_match(const css_compound_t *cp, const css_node_iface_t *ifc,
    const void *node)
{
	const char	*v;
	int		k;

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
	for (k = 0; k < cp->nattrs; k++) {
		v = ifc->get_attr(node, cp->attrs[k].name);
		if (css_attr_match(&cp->attrs[k], v) == 0) {
			return (0);
		}
	}
	if (cp->first_child != 0) {
		/*
		 * prev() is the previous *element* sibling, so no text-node
		 * skipping is needed here; the host iface already does it.
		 */
		if (ifc->prev == NULL || ifc->prev(node) != NULL) {
			return (0);
		}
	}
	return (1);
}

/*
 * Matches parts[0..last] against `node` and its ancestors/siblings, walking
 * right to left.
 *
 * The combinator stored on parts[last] describes how it binds to parts[last-1],
 * which is why it is read here and not one index down.  Descendant and general
 * sibling are the branching cases: both try every candidate and recurse, so the
 * shared *steps counter is what stops a pathological selector from walking a
 * deep tree exponentially.
 */
static int
css_chain_step(const css_selector_t *sel, int last, const css_node_iface_t *ifc,
    const void *node, int *steps)
{
	const void	*a;

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

	switch (sel->parts[last].comb) {
	case CSS_COMB_CHILD:
		a = ifc->parent(node);
		if (a == NULL) {
			return (0);
		}
		return (css_chain_step(sel, last - 1, ifc, a, steps));
	case CSS_COMB_ADJ:
		if (ifc->prev == NULL) {
			return (0);
		}
		a = ifc->prev(node);
		if (a == NULL) {
			return (0);
		}
		return (css_chain_step(sel, last - 1, ifc, a, steps));
	case CSS_COMB_SIB:
		if (ifc->prev == NULL) {
			return (0);
		}
		for (a = ifc->prev(node); a != NULL; a = ifc->prev(a)) {
			if (css_chain_step(sel, last - 1, ifc, a, steps) != 0) {
				return (1);
			}
			if (*steps <= 0) {
				return (0);
			}
		}
		return (0);
	case CSS_COMB_DESC:
	default:
		for (a = ifc->parent(node); a != NULL; a = ifc->parent(a)) {
			if (css_chain_step(sel, last - 1, ifc, a, steps) != 0) {
				return (1);
			}
			if (*steps <= 0) {
				return (0);
			}
		}
		return (0);
	}
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
	char	value[192];
	int	i;

	for (i = 0; i < rule->ndecls; i++) {
		/*
		 * Values arrive already split, but still go through the
		 * !important strip and trailing-space trim in
		 * css_apply_declarations, so reuse it one pair at a time
		 * instead of duplicating that cleanup here.
		 */
		size_t	n = strlen(rule->decls[i].value);

		if (n >= sizeof(value)) {
			continue;	/* data: URI or similar; nothing to gain */
		}
		memcpy(value, rule->decls[i].value, n + 1);
		css_apply_prop_text(cc, rule->decls[i].prop, value, base_scale);
	}
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

		/*
		 * Box metrics, margins and background belong to the node they
		 * were declared on.  Ancestors contribute only the inherited
		 * half of the cascade; i == 0 is the node itself.  Note that
		 * cur still carries the ancestor's values into the next
		 * iteration - that is deliberate for colour and font size, and
		 * harmless for the rest because only this OR reaches the
		 * caller.
		 */
		if (i != 0) {
			out->set |= cur.set & ~CSS_NOINHERIT_PROPS;
		} else {
			out->set |= cur.set;
			if ((cur.set & CSS_PROP_WIDTH) != 0) {
				out->width = cur.width;
				out->width_pct = cur.width_pct;
			}
			if ((cur.set & CSS_PROP_HEIGHT) != 0) {
				out->height = cur.height;
				out->height_pct = cur.height_pct;
			}
			if ((cur.set & CSS_PROP_PADDING) != 0) {
				out->pad_top = cur.pad_top;
				out->pad_right = cur.pad_right;
				out->pad_bottom = cur.pad_bottom;
				out->pad_left = cur.pad_left;
			}
			if ((cur.set & CSS_PROP_BORDER) != 0) {
				out->border_width = cur.border_width;
			}
			if ((cur.set & CSS_PROP_BORDER_COLOR) != 0) {
				out->border_color = cur.border_color;
			}
			if ((cur.set & CSS_PROP_BGCOLOR) != 0) {
				out->bgcolor = cur.bgcolor;
			}
			if ((cur.set & CSS_PROP_MARGIN_TOP) != 0) {
				out->margin_top = cur.margin_top;
			}
			if ((cur.set & CSS_PROP_MARGIN_BOTTOM) != 0) {
				out->margin_bottom = cur.margin_bottom;
			}
		}

		if ((cur.set & CSS_PROP_COLOR) != 0) {
			out->color = cur.color;
		}
		if ((cur.set & CSS_PROP_SCALE) != 0) {
			out->font_scale = cur.font_scale;
		}
		if ((cur.set & CSS_PROP_ALIGN_CENTER) != 0) {
			out->align_center = cur.align_center;
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
