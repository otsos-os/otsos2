/* !DEFINES!

$define %type svg_parser as internal parse state
$define %func svg_parse as function with args const char *, size_t, svg_doc **
$define %func svg_view_transform as function with args const svg_doc *, double, double, double *

*/

/* !SPACE!

$space %internal svg_elem_kind_t, svg_paint_t, svg_matrix_t, svg_frame_t
$space %internal svg_xdups, svg_skip_ws_commas, svg_decode_entities
$space %internal svg_color_parse, svg_opacity_parse, svg_paint_apply
$space %internal sb_scratch, sb_add_point, sb_subpath_begin, sb_commit
$space %internal svg_emit_cubic, svg_flatten_arc
$space %internal svg_parse_path_d
$space %internal svg_matrix_multiply, svg_matrix_map, svg_matrix_scale
$space %internal svg_parse_transform_attr, svg_length_parse
$space %internal svg_classify, svg_collect_attrs, svg_attrs_free
$space %internal svg_find_attr, svg_apply_paint_attrs
$space %internal svg_build_rect, svg_build_ellipse_geom

*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "svg_int.h"

#define SVG_ATTRS_MAX		64
#define SVG_NAME_MAX		32
#define PI			3.14159265358979324
#define SVG_CUBIC_SEG_MAX	128
#define SVG_ARC_SEG_MAX		64

typedef struct svg_matrix {
	double	a, b, c, d, e, f;
} svg_matrix_t;

typedef enum svg_elem_kind {
	SVG_ELEM_IGNORE = 0,
	SVG_ELEM_GROUP,
	SVG_ELEM_SKIP,
	SVG_ELEM_ROOT,
	SVG_ELEM_PATH,
	SVG_ELEM_RECT,
	SVG_ELEM_CIRCLE,
	SVG_ELEM_ELLIPSE,
	SVG_ELEM_LINE,
	SVG_ELEM_POLYLINE,
	SVG_ELEM_POLYGON
} svg_elem_kind_t;

typedef struct svg_paint {
	uint32_t	flags;
	uint32_t	fill;
	uint32_t	stroke;
	double		stroke_width;
	double		fill_op;
	double		stroke_op;
} svg_paint_t;

typedef struct svg_frame {
	char		name[SVG_NAME_MAX];
	svg_matrix_t	ctm;
	svg_paint_t	paint;
	int		skip;
} svg_frame_t;

typedef struct svg_parsed_attr {
	char	*name;
	char	*value;
} svg_parsed_attr_t;


typedef struct svg_builder {
	svg_shape_t	*shapes;
	int		nshapes;
	int		shapes_cap;
	double		*pts;
	int		npts;
	int		pts_cap;
	int		*subs;
	int		nsubs;
	int		subs_cap;
	int		truncated;
} svg_builder_t;

static void	svg_matrix_map(const svg_matrix_t *m, double x, double y,
		    double *out_x, double *out_y);

static void
frame_set_name(svg_frame_t *f, const char *name)
{
	size_t	nl;

	nl = strlen(name);
	if (nl >= sizeof(f->name)) {
		nl = sizeof(f->name) - 1;
	}
	memcpy(f->name, name, nl);
	f->name[nl] = '\0';
}

static const char *
svg_skip_ws_commas(const char *p)
{
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ||
	    *p == ',') {
		p++;
	}
	return (p);
}

static char *
svg_xdups(const char *src, size_t len)
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

static char *
svg_decode_entities(const char *src, size_t len)
{
	char		*out;
	size_t		i, j, k;
	unsigned long	code;
	int		is_hex;

	out = (char *)malloc(len + 1);
	if (out == NULL) {
		return (NULL);
	}
	i = 0;
	j = 0;
	while (i < len) {
		if (src[i] != '&') {
			out[j++] = src[i++];
			continue;
		}
		if (len - i >= 5 && strncmp(src + i, "&amp;", 5) == 0) {
			out[j++] = '&';
			i += 5;
		} else if (len - i >= 4 && strncmp(src + i, "&lt;", 4) == 0) {
			out[j++] = '<';
			i += 4;
		} else if (len - i >= 4 && strncmp(src + i, "&gt;", 4) == 0) {
			out[j++] = '>';
			i += 4;
		} else if (len - i >= 6 &&
		    strncmp(src + i, "&quot;", 6) == 0) {
			out[j++] = '"';
			i += 6;
		} else if (len - i >= 6 &&
		    strncmp(src + i, "&apos;", 6) == 0) {
			out[j++] = '\'';
			i += 6;
		} else if (len - i >= 3 && src[i + 1] == '#') {
			k = i + 2;
			is_hex = 0;
			if (k < len && (src[k] == 'x' || src[k] == 'X')) {
				is_hex = 1;
				k++;
			}
			code = 0;
			while (k < len && src[k] != ';' && code < 0x110000) {
				if (src[k] >= '0' && src[k] <= '9') {
					code = code * (is_hex ? 16u : 10u) +
					    (unsigned long)(src[k] - '0');
				} else if (is_hex && src[k] >= 'a' &&
				    src[k] <= 'f') {
					code = code * 16u +
					    (unsigned long)(src[k] - 'a' + 10);
				} else if (is_hex && src[k] >= 'A' &&
				    src[k] <= 'F') {
					code = code * 16u +
					    (unsigned long)(src[k] - 'A' + 10);
				} else {
					break;
				}
				k++;
			}
			if (k < len && src[k] == ';' && code > 0 &&
			    code < 0x80) {
				out[j++] = (char)code;
				i = k + 1;
			} else {
				out[j++] = src[i++];
			}
		} else {
			out[j++] = src[i++];
		}
	}
	out[j] = '\0';
	return (out);
}


typedef struct svg_color_name {
	const char	*name;
	uint32_t	rgb;
} svg_color_name_t;

static const svg_color_name_t svg_colors[] = {
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
	{ NULL,		0 }
};

static int
svg_color_parse(const char *str, uint32_t *out_rgb, int *out_none)
{
	const char	*p;
	int		i, v;
	uint32_t	wide, comp[3];

	*out_none = 0;
	p = svg_skip_ws_commas(str);
	if (*p == '\0') {
		return (-1);
	}
	if (strncasecmp(p, "none", 4) == 0) {
		*out_none = 1;
		return (0);
	}

	if (*p == '#') {
		p++;
		for (i = 0; i < 6; i++) {
			if (!isxdigit((unsigned char)p[i])) {
				break;
			}
		}
		if (i == 6) {
			wide = 0;
			for (i = 0; i < 6; i++) {
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
			*out_rgb = wide;
			return (0);
		}
		if (i == 3) {
			wide = 0;
			for (i = 0; i < 3; i++) {
				v = p[i];
				if (v >= '0' && v <= '9') {
					v -= '0';
				} else if (v >= 'a' && v <= 'f') {
					v -= 'a' - 10;
				} else {
					v -= 'A' - 10;
				}
				wide = (wide << 4) |
				    (uint32_t)(unsigned)(v | (v << 4));
			}
			*out_rgb = wide;
			return (0);
		}
		return (-1);
	}

	if (strncasecmp(p, "rgb(", 4) == 0) {
		double	val;
		char	*q;

		p += 4;
		for (i = 0; i < 3; i++) {
			val = svg_strtod(p, &q);
			if (q == p) {
				return (-1);
			}
			p = svg_skip_ws_commas(q);
			if (*p == '%') {
				val *= 2.55;
				p++;
				p = svg_skip_ws_commas(p);
			}
			if (val < 0.0) {
				val = 0.0;
			}
			if (val > 255.0) {
				val = 255.0;
			}
			comp[i] = (uint32_t)val;
			if (i < 2 && *p == ',') {
				p++;
				p = svg_skip_ws_commas(p);
			}
		}
		*out_rgb = (comp[0] << 16) | (comp[1] << 8) | comp[2];
		return (0);
	}

	for (i = 0; svg_colors[i].name != NULL; i++) {
		if (strcasecmp(p, svg_colors[i].name) == 0) {
			*out_rgb = svg_colors[i].rgb;
			return (0);
		}
	}
	return (-1);
}

static double
svg_opacity_parse(const char *str)
{
	double	v;
	char	*q;
	size_t	len;

	len = strlen(str);
	v = svg_strtod(str, &q);
	if (q != str && *q == '%' && q + 1 == str + len) {
		v /= 100.0;
	}
	if (v < 0.0) {
		v = 0.0;
	}
	if (v > 1.0) {
		v = 1.0;
	}
	return (v);
}

static int
svg_paint_apply(svg_paint_t *paint, const char *name, const char *value)
{
	uint32_t	rgb;
	int		none;

	if (name == NULL || value == NULL) {
		return (0);
	}
	if (strcasecmp(name, "fill") == 0) {
		if (svg_color_parse(value, &rgb, &none) == 0) {
			if (none != 0) {
				paint->flags &= ~(uint32_t)SVG_FILL;
			} else {
				paint->flags |= SVG_FILL;
				paint->fill = rgb;
			}
		}
		return (1);
	}
	if (strcasecmp(name, "stroke") == 0) {
		if (svg_color_parse(value, &rgb, &none) == 0) {
			if (none != 0) {
				paint->flags &= ~(uint32_t)SVG_STROKE;
			} else {
				paint->flags |= SVG_STROKE;
				paint->stroke = rgb;
			}
		}
		return (1);
	}
	if (strcasecmp(name, "stroke-width") == 0) {
		paint->stroke_width = svg_strtod(value, NULL);
		if (paint->stroke_width < 0.0) {
			paint->stroke_width = 0.0;
		}
		if (paint->stroke_width > SVG_STROKE_MAX) {
			paint->stroke_width = SVG_STROKE_MAX;
		}
		return (1);
	}
	if (strcasecmp(name, "fill-opacity") == 0) {
		paint->fill_op = svg_opacity_parse(value);
		return (1);
	}
	if (strcasecmp(name, "stroke-opacity") == 0) {
		paint->stroke_op = svg_opacity_parse(value);
		return (1);
	}
	if (strcasecmp(name, "opacity") == 0) {
		paint->fill_op *= svg_opacity_parse(value);
		paint->stroke_op *= svg_opacity_parse(value);
		return (1);
	}
	return (0);
}


static int
sb_scratch(svg_builder_t *bld)
{
	bld->pts_cap = 256;
	bld->npts = 0;
	bld->subs_cap = 16;
	bld->nsubs = 0;
	bld->pts = (double *)malloc((size_t)bld->pts_cap * 2 *
	    sizeof(double));
	bld->subs = (int *)malloc((size_t)bld->subs_cap * sizeof(int));
	return ((bld->pts == NULL || bld->subs == NULL) ? -1 : 0);
}

static void
svg_builder_init(svg_builder_t *bld)
{
	memset(bld, 0, sizeof(*bld));
	bld->shapes_cap = 64;
	bld->shapes = (svg_shape_t *)malloc(
	    (size_t)bld->shapes_cap * sizeof(svg_shape_t));
	if (bld->shapes == NULL || sb_scratch(bld) != 0) {
		bld->truncated = 1;
	}
}

static void
svg_builder_free(svg_builder_t *bld)
{
	int	i;

	free(bld->pts);
	free(bld->subs);
	for (i = 0; i < bld->nshapes; i++) {
		free(bld->shapes[i].pts);
		free(bld->shapes[i].subs);
	}
	free(bld->shapes);
	memset(bld, 0, sizeof(*bld));
}

static int
sb_add_point(svg_builder_t *bld, double x, double y)
{
	double	*np;

	if (bld->truncated != 0) {
		return (-1);
	}
	if (bld->npts >= SVG_MAX_POINTS) {
		bld->truncated = 1;
		return (-1);
	}
	if (bld->npts + 1 > bld->pts_cap) {
		while (bld->npts + 1 > bld->pts_cap) {
			bld->pts_cap *= 2;
		}
		np = (double *)realloc(bld->pts, (size_t)bld->pts_cap * 2 *
		    sizeof(double));
		if (np == NULL) {
			bld->truncated = 1;
			return (-1);
		}
		bld->pts = np;
	}
	bld->pts[bld->npts * 2] = x;
	bld->pts[bld->npts * 2 + 1] = y;
	bld->npts++;
	return (0);
}

static int
sb_subpath_begin(svg_builder_t *bld)
{
	int	*ns;

	if (bld->truncated != 0) {
		return (-1);
	}
	if (bld->nsubs >= SVG_MAX_SUBPATHS) {
		bld->truncated = 1;
		return (-1);
	}
	if (bld->nsubs + 1 > bld->subs_cap) {
		while (bld->nsubs + 1 > bld->subs_cap) {
			bld->subs_cap *= 2;
		}
		ns = (int *)realloc(bld->subs,
		    (size_t)bld->subs_cap * sizeof(int));
		if (ns == NULL) {
			bld->truncated = 1;
			return (-1);
		}
		bld->subs = ns;
	}
	bld->subs[bld->nsubs++] = bld->npts;
	return (0);
}

static void
sb_commit(svg_builder_t *bld, const svg_paint_t *paint, uint32_t extra_flags)
{
	svg_shape_t	*sp;

	if (bld->npts >= 2 && bld->nsubs >= 1 &&
	    bld->nshapes < SVG_MAX_SHAPES) {
		if (bld->nshapes + 1 > bld->shapes_cap) {
			bld->shapes_cap *= 2;
			sp = (svg_shape_t *)realloc(bld->shapes,
			    (size_t)bld->shapes_cap * sizeof(svg_shape_t));
			if (sp != NULL) {
				bld->shapes = sp;
				sp = NULL;
			}
		}
		if (bld->shapes != NULL &&
		    bld->nshapes < bld->shapes_cap) {
			sp = &bld->shapes[bld->nshapes++];
			sp->flags = paint->flags | extra_flags;
			sp->fill = paint->fill;
			sp->stroke = paint->stroke;
			sp->stroke_width = paint->stroke_width;
			sp->fill_opacity = paint->fill_op;
			sp->stroke_opacity = paint->stroke_op;
			sp->pts = bld->pts;
			sp->npts = bld->npts;
			sp->subs = bld->subs;
			sp->nsubs = bld->nsubs;
			bld->pts = NULL;
			bld->subs = NULL;
		}
	}
	free(bld->pts);
	free(bld->subs);
	bld->pts = NULL;
	bld->subs = NULL;
	bld->npts = 0;
	bld->nsubs = 0;
	bld->pts_cap = 0;
	bld->subs_cap = 0;
	if (sb_scratch(bld) != 0) {
		bld->truncated = 1;
	}
}
static void
svg_emit_cubic(svg_builder_t *bld,
    double x0, double y0, double x1, double y1,
    double x2, double y2, double x3, double y3)
{
	double	t, mt, ax, ay, bx, by, cx, cy, px, py;
	double	len;
	int	i, steps;

	len = svg_sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) +
	    svg_sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) +
	    svg_sqrt((x3 - x2) * (x3 - x2) + (y3 - y2) * (y3 - y2));
	steps = (int)(len / 4.0) + 1;
	if (steps < 1) {
		steps = 1;
	}
	if (steps > SVG_CUBIC_SEG_MAX) {
		steps = SVG_CUBIC_SEG_MAX;
	}

	for (i = 1; i <= steps; i++) {
		t = (double)i / (double)steps;
		mt = 1.0 - t;
		ax = mt * x0 + t * x1;
		ay = mt * y0 + t * y1;
		bx = mt * x1 + t * x2;
		by = mt * y1 + t * y2;
		cx = mt * x2 + t * x3;
		cy = mt * y2 + t * y3;
		ax = mt * ax + t * bx;
		ay = mt * ay + t * by;
		bx = mt * bx + t * cx;
		by = mt * by + t * cy;
		px = mt * ax + t * bx;
		py = mt * ay + t * by;
		sb_add_point(bld, px, py);
	}
}

static void
svg_flatten_arc(svg_builder_t *bld, const svg_matrix_t *ctm,
    double x1, double y1, double rx, double ry, double phi_deg,
    int large_arc, int sweep, double x2, double y2)
{
	double	phi, cos_phi, sin_phi;
	double	dx2, dy2, x1p, y1p;
	double	lambda, factor, num, denom;
	double	ccx, ccy, ux, uy, vx, vy;
	double	theta1, dtheta, ang, ex, ey, mx;
	int	i, nseg;

	rx = (rx < 0.0) ? -rx : rx;
	ry = (ry < 0.0) ? -ry : ry;
	if (rx == 0.0 || ry == 0.0 || (x1 == x2 && y1 == y2)) {
		svg_matrix_map(ctm, x2, y2, &ex, &ey);
		sb_add_point(bld, ex, ey);
		return;
	}

	phi = phi_deg * PI / 180.0;
	cos_phi = svg_cos(phi);
	sin_phi = svg_sin(phi);

	dx2 = (x1 - x2) / 2.0;
	dy2 = (y1 - y2) / 2.0;
	x1p = cos_phi * dx2 + sin_phi * dy2;
	y1p = -sin_phi * dx2 + cos_phi * dy2;

	lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
	if (lambda > 1.0) {
		factor = svg_sqrt(lambda);
		rx *= factor;
		ry *= factor;
	}

	num = rx * rx * ry * ry - rx * rx * y1p * y1p -
	    ry * ry * x1p * x1p;
	denom = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
	num = (denom == 0.0 || num < 0.0) ? 0.0 : svg_sqrt(num / denom);
	if (large_arc == sweep) {
		num = -num;
	}
	ccx = num * rx * y1p / ry;
	ccy = -num * ry * x1p / rx;

	ux = (x1p - ccx) / rx;
	uy = (y1p - ccy) / ry;
	vx = (-x1p - ccx) / rx;
	vy = (-y1p - ccy) / ry;

	theta1 = svg_atan2(uy, ux);
	dtheta = svg_atan2(vy, vx) - theta1;
	if (sweep == 0 && dtheta > 0.0) {
		dtheta -= 2.0 * PI;
	} else if (sweep != 0 && dtheta < 0.0) {
		dtheta += 2.0 * PI;
	}

	nseg = (int)((dtheta < 0.0 ? -dtheta : dtheta) / (PI / 8.0)) + 1;
	if (nseg < 8) {
		nseg = 8;
	}
	if (nseg > SVG_ARC_SEG_MAX) {
		nseg = SVG_ARC_SEG_MAX;
	}

	for (i = 1; i <= nseg; i++) {
		ang = theta1 + dtheta * (double)i / (double)nseg;
		ex = ccx + rx * svg_cos(ang);
		ey = ccy + ry * svg_sin(ang);
		mx = ex * cos_phi - ey * sin_phi + (x1 + x2) / 2.0;
		ey = ex * sin_phi + ey * cos_phi + (y1 + y2) / 2.0;
		svg_matrix_map(ctm, mx, ey, &ex, &ey);
		sb_add_point(bld, ex, ey);
	}
}


static void
path_line_to(svg_builder_t *bld, const svg_matrix_t *ctm,
    double cx, double cy)
{
	double	tx, ty;

	svg_matrix_map(ctm, cx, cy, &tx, &ty);
	sb_add_point(bld, tx, ty);
}

static int
svg_parse_path_d(svg_builder_t *bld, const svg_matrix_t *ctm,
    const char *d)
{
	const char	*p;
	char		cmd;
	double		cx, cy, sx, sy;
	double		prev_cx, prev_cy;
	double		v[7];
	int		rel, i, have_sub, prev_is_c, prev_is_q;

	if (d == NULL) {
		return (-1);
	}

	p = d;
	cmd = 0;
	cx = cy = sx = sy = 0.0;
	prev_cx = prev_cy = 0.0;
	prev_is_c = 0;
	prev_is_q = 0;
	have_sub = 0;

	while (*p != '\0') {
		p = svg_skip_ws_commas(p);
		if (*p == '\0') {
			break;
		}
		if (isalpha((unsigned char)*p)) {
			cmd = *p++;
			p = svg_skip_ws_commas(p);
		} else if (cmd == 0) {
			return (-1);
		}

		switch (cmd) {
		case 'M': case 'm':
			rel = (cmd == 'm');
			p = svg_skip_ws_commas(p);
			v[0] = svg_strtod(p, (char **)&p);
			p = svg_skip_ws_commas(p);
			v[1] = svg_strtod(p, (char **)&p);
			cx = rel ? cx + v[0] : v[0];
			cy = rel ? cy + v[1] : v[1];
			sx = cx;
			sy = cy;
			sb_subpath_begin(bld);
			path_line_to(bld, ctm, cx, cy);
			have_sub = 1;
			prev_is_c = 0;
			prev_is_q = 0;
			cmd = rel ? 'l' : 'L';
			continue;
		case 'Z': case 'z':
			if (have_sub != 0 && (cx != sx || cy != sy)) {
				path_line_to(bld, ctm, sx, sy);
				cx = sx;
				cy = sy;
			}
			prev_is_c = 0;
			prev_is_q = 0;
			continue;
		default:
			break;
		}

		switch (cmd) {
		case 'L': case 'l':
			rel = (cmd == 'l');
			p = svg_skip_ws_commas(p);
			v[0] = svg_strtod(p, (char **)&p);
			p = svg_skip_ws_commas(p);
			v[1] = svg_strtod(p, (char **)&p);
			cx = rel ? cx + v[0] : v[0];
			cy = rel ? cy + v[1] : v[1];
			path_line_to(bld, ctm, cx, cy);
			prev_is_c = 0;
			prev_is_q = 0;
			continue;
		case 'H': case 'h':
			rel = (cmd == 'h');
			p = svg_skip_ws_commas(p);
			v[0] = svg_strtod(p, (char **)&p);
			cx = rel ? cx + v[0] : v[0];
			path_line_to(bld, ctm, cx, cy);
			prev_is_c = 0;
			prev_is_q = 0;
			continue;
		case 'V': case 'v':
			rel = (cmd == 'v');
			p = svg_skip_ws_commas(p);
			v[0] = svg_strtod(p, (char **)&p);
			cy = rel ? cy + v[0] : v[0];
			path_line_to(bld, ctm, cx, cy);
			prev_is_c = 0;
			prev_is_q = 0;
			continue;
		case 'C': case 'c': {
			double	x0, y0, r[6];
			double	m0x, m0y, m1x, m1y, m2x, m2y, m3x, m3y;

			x0 = cx;
			y0 = cy;
			for (i = 0; i < 6; i++) {
				p = svg_skip_ws_commas(p);
				v[i] = svg_strtod(p, (char **)&p);
				p = svg_skip_ws_commas(p);
			}
			rel = (cmd == 'c');
			r[0] = rel ? x0 + v[0] : v[0];
			r[1] = rel ? y0 + v[1] : v[1];
			r[2] = rel ? x0 + v[2] : v[2];
			r[3] = rel ? y0 + v[3] : v[3];
			r[4] = rel ? x0 + v[4] : v[4];
			r[5] = rel ? y0 + v[5] : v[5];
			prev_cx = r[2];
			prev_cy = r[3];
			prev_is_c = 1;
			prev_is_q = 0;
			cx = r[4];
			cy = r[5];
			svg_matrix_map(ctm, x0, y0, &m0x, &m0y);
			svg_matrix_map(ctm, r[0], r[1], &m1x, &m1y);
			svg_matrix_map(ctm, r[2], r[3], &m2x, &m2y);
			svg_matrix_map(ctm, r[4], r[5], &m3x, &m3y);
			svg_emit_cubic(bld, m0x, m0y, m1x, m1y, m2x, m2y,
			    m3x, m3y);
			continue;
		}
		case 'S': case 's': {
			double	x0, y0, c1x, c1y, r[4];
			double	m0x, m0y, m1x, m1y, m2x, m2y, m3x, m3y;

			x0 = cx;
			y0 = cy;
			for (i = 0; i < 4; i++) {
				p = svg_skip_ws_commas(p);
				v[i] = svg_strtod(p, (char **)&p);
				p = svg_skip_ws_commas(p);
			}
			rel = (cmd == 's');
			if (prev_is_c != 0) {
				c1x = 2.0 * x0 - prev_cx;
				c1y = 2.0 * y0 - prev_cy;
			} else {
				c1x = x0;
				c1y = y0;
			}
			r[0] = rel ? x0 + v[0] : v[0];
			r[1] = rel ? y0 + v[1] : v[1];
			r[2] = rel ? x0 + v[2] : v[2];
			r[3] = rel ? y0 + v[3] : v[3];
			prev_cx = r[0];
			prev_cy = r[1];
			prev_is_c = 1;
			prev_is_q = 0;
			cx = r[2];
			cy = r[3];
			svg_matrix_map(ctm, x0, y0, &m0x, &m0y);
			svg_matrix_map(ctm, c1x, c1y, &m1x, &m1y);
			svg_matrix_map(ctm, r[0], r[1], &m2x, &m2y);
			svg_matrix_map(ctm, r[2], r[3], &m3x, &m3y);
			svg_emit_cubic(bld, m0x, m0y, m1x, m1y, m2x, m2y,
			    m3x, m3y);
			continue;
		}
		case 'Q': case 'q':
		case 'T': case 't': {
			double	x0, y0, qx, qy, r[2];
			double	c1x, c1y, c2x, c2y;
			double	m0x, m0y, m1x, m1y, m2x, m2y, m3x, m3y;

			x0 = cx;
			y0 = cy;
			if (cmd == 'Q' || cmd == 'q') {
				for (i = 0; i < 4; i++) {
					p = svg_skip_ws_commas(p);
					v[i] = svg_strtod(p, (char **)&p);
					p = svg_skip_ws_commas(p);
				}
				rel = (cmd == 'q');
				qx = rel ? x0 + v[0] : v[0];
				qy = rel ? y0 + v[1] : v[1];
				r[0] = rel ? x0 + v[2] : v[2];
				r[1] = rel ? y0 + v[3] : v[3];
			} else {
				if (prev_is_q != 0) {
					qx = 2.0 * x0 - prev_cx;
					qy = 2.0 * y0 - prev_cy;
				} else {
					qx = x0;
					qy = y0;
				}
				p = svg_skip_ws_commas(p);
				r[0] = svg_strtod(p, (char **)&p);
				p = svg_skip_ws_commas(p);
				r[1] = svg_strtod(p, (char **)&p);
				rel = (cmd == 't');
				r[0] = rel ? x0 + r[0] : r[0];
				r[1] = rel ? y0 + r[1] : r[1];
			}
			prev_cx = qx;
			prev_cy = qy;
			prev_is_c = 0;
			prev_is_q = 1;
			cx = r[0];
			cy = r[1];
			c1x = x0 + 2.0 / 3.0 * (qx - x0);
			c1y = y0 + 2.0 / 3.0 * (qy - y0);
			c2x = r[0] + 2.0 / 3.0 * (qx - r[0]);
			c2y = r[1] + 2.0 / 3.0 * (qy - r[1]);
			svg_matrix_map(ctm, x0, y0, &m0x, &m0y);
			svg_matrix_map(ctm, c1x, c1y, &m1x, &m1y);
			svg_matrix_map(ctm, c2x, c2y, &m2x, &m2y);
			svg_matrix_map(ctm, r[0], r[1], &m3x, &m3y);
			svg_emit_cubic(bld, m0x, m0y, m1x, m1y, m2x, m2y,
			    m3x, m3y);
			continue;
		}
		case 'A': case 'a': {
			double	rx, ry, rot, x2, y2;
			int	laf, swp;

			p = svg_skip_ws_commas(p);
			rx = svg_strtod(p, (char **)&p);
			p = svg_skip_ws_commas(p);
			ry = svg_strtod(p, (char **)&p);
			p = svg_skip_ws_commas(p);
			rot = svg_strtod(p, (char **)&p);
			p = svg_skip_ws_commas(p);
			laf = 0;
			swp = 0;
			if (*p == '0' || *p == '1') {
				laf = *p - '0';
				p++;
			}
			p = svg_skip_ws_commas(p);
			if (*p == '0' || *p == '1') {
				swp = *p - '0';
				p++;
			}
			p = svg_skip_ws_commas(p);
			x2 = svg_strtod(p, (char **)&p);
			p = svg_skip_ws_commas(p);
			y2 = svg_strtod(p, (char **)&p);
			rel = (cmd == 'a');
			x2 = rel ? cx + x2 : x2;
			y2 = rel ? cy + y2 : y2;
			svg_flatten_arc(bld, ctm, cx, cy, rx, ry, rot,
			    laf, swp, x2, y2);
			cx = x2;
			cy = y2;
			prev_is_c = 0;
			prev_is_q = 0;
			continue;
		}
		default:
			return (-1);
		}
	}
	return (0);
}


#define M_ID(m)		((m)->a = 1.0, (m)->b = 0.0, (m)->c = 0.0, \
			(m)->d = 1.0, (m)->e = 0.0, (m)->f = 0.0)

static void
svg_matrix_multiply(svg_matrix_t *out, const svg_matrix_t *m,
    const svg_matrix_t *n)
{
	svg_matrix_t	r;

	r.a = m->a * n->a + m->b * n->c;
	r.b = m->a * n->b + m->b * n->d;
	r.c = m->c * n->a + m->d * n->c;
	r.d = m->c * n->b + m->d * n->d;
	r.e = m->e * n->a + m->f * n->c + n->e;
	r.f = m->e * n->b + m->f * n->d + n->f;
	*out = r;
}

static void
svg_matrix_map(const svg_matrix_t *m, double x, double y,
    double *out_x, double *out_y)
{
	*out_x = m->a * x + m->c * y + m->e;
	*out_y = m->b * x + m->d * y + m->f;
}

static double
svg_matrix_scale(const svg_matrix_t *m)
{
	double	sx, sy;

	sx = svg_sqrt(m->a * m->a + m->b * m->b);
	sy = svg_sqrt(m->c * m->c + m->d * m->d);
	return ((sx + sy) / 2.0);
}

static int
svg_parse_transform_attr(const char *str, svg_matrix_t *out)
{
	const char	*p;
	svg_matrix_t	op, acc, tmp;
	double		v[6];
	char		name[16];
	size_t		nl;
	int		i;

	M_ID(&acc);
	p = str;
	while (*p != '\0') {
		p = svg_skip_ws_commas(p);
		if (*p == '\0') {
			break;
		}
		nl = 0;
		while (isalpha((unsigned char)*p) && nl < sizeof(name) - 1) {
			name[nl++] = *p++;
		}
		name[nl] = '\0';
		p = svg_skip_ws_commas(p);
		if (*p != '(') {
			return (-1);
		}
		p++;

		for (i = 0; i < 6; i++) {
			v[i] = 0.0;
		}
		i = 0;
		while (*p != '\0' && *p != ')' && i < 6) {
			v[i++] = svg_strtod(p, (char **)&p);
			p = svg_skip_ws_commas(p);
			if (*p == ',') {
				p++;
				p = svg_skip_ws_commas(p);
			}
		}
		if (*p == ')') {
			p++;
		} else {
			return (-1);
		}

		M_ID(&op);
		if (strcmp(name, "translate") == 0) {
			op.e = v[0];
			op.f = (i > 1) ? v[1] : 0.0;
		} else if (strcmp(name, "scale") == 0) {
			op.a = v[0];
			op.d = (i > 1) ? v[1] : v[0];
			if (op.a == 0.0 && op.d == 0.0) {
				op.a = op.d = 1.0;
			}
		} else if (strcmp(name, "rotate") == 0) {
			double	ang, cs, sn, ccx, ccy;
			svg_matrix_t	t1, t2, t3;

			ang = v[0] * PI / 180.0;
			cs = svg_cos(ang);
			sn = svg_sin(ang);
			if (i >= 3) {
				ccx = v[1];
				ccy = v[2];
				M_ID(&t1);
				t1.e = ccx;
				t1.f = ccy;
				M_ID(&t2);
				t2.a = cs;
				t2.b = sn;
				t2.c = -sn;
				t2.d = cs;
				M_ID(&t3);
				t3.e = -ccx;
				t3.f = -ccy;
				svg_matrix_multiply(&tmp, &t1, &t2);
				svg_matrix_multiply(&t1, &tmp, &t3);
				op = t1;
			} else {
				op.a = cs;
				op.b = sn;
				op.c = -sn;
				op.d = cs;
			}
		} else if (strcmp(name, "skewX") == 0) {
			op.c = svg_sin(v[0] * PI / 180.0) /
			    svg_cos(v[0] * PI / 180.0);
		} else if (strcmp(name, "skewY") == 0) {
			op.b = svg_sin(v[0] * PI / 180.0) /
			    svg_cos(v[0] * PI / 180.0);
		} else if (strcmp(name, "matrix") == 0) {
			if (i < 6) {
				return (-1);
			}
			op.a = v[0];
			op.b = v[1];
			op.c = v[2];
			op.d = v[3];
			op.e = v[4];
			op.f = v[5];
		} else {
			return (-1);
		}

		svg_matrix_multiply(&acc, &acc, &op);
	}
	*out = acc;
	return (0);
}

static int
svg_length_parse(const char *str, double *out)
{
	double	v;
	char	*q;

	v = svg_strtod(str, &q);
	if (q == str) {
		return (-1);
	}
	if (*q == '%') {
		return (-1);
	}
	*out = v;
	return (0);
}


static svg_elem_kind_t
svg_classify(const char *name)
{
	if (strcasecmp(name, "svg") == 0) return (SVG_ELEM_ROOT);
	if (strcasecmp(name, "g") == 0) return (SVG_ELEM_GROUP);
	if (strcasecmp(name, "a") == 0) return (SVG_ELEM_GROUP);
	if (strcasecmp(name, "switch") == 0) return (SVG_ELEM_GROUP);
	if (strcasecmp(name, "path") == 0) return (SVG_ELEM_PATH);
	if (strcasecmp(name, "rect") == 0) return (SVG_ELEM_RECT);
	if (strcasecmp(name, "circle") == 0) return (SVG_ELEM_CIRCLE);
	if (strcasecmp(name, "ellipse") == 0) return (SVG_ELEM_ELLIPSE);
	if (strcasecmp(name, "line") == 0) return (SVG_ELEM_LINE);
	if (strcasecmp(name, "polyline") == 0) return (SVG_ELEM_POLYLINE);
	if (strcasecmp(name, "polygon") == 0) return (SVG_ELEM_POLYGON);
	if (strcasecmp(name, "defs") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "symbol") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "metadata") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "title") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "desc") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "clipPath") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "mask") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "pattern") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "marker") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "style") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "script") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "text") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "tspan") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "textPath") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "use") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "image") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "linearGradient") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "radialGradient") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "stop") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "filter") == 0) return (SVG_ELEM_SKIP);
	if (strcasecmp(name, "foreignObject") == 0) return (SVG_ELEM_SKIP);
	return (SVG_ELEM_IGNORE);
}

static void
svg_collect_attrs(const char **pp, const char *end,
    svg_parsed_attr_t *attrs, int *nattrs, int *self_closing)
{
	const char	*p;
	int		count;

	p = *pp;
	count = 0;
	*self_closing = 0;
	memset(attrs, 0, sizeof(attrs[0]) * SVG_ATTRS_MAX);

	while (p < end && *p != '>') {
		while (p < end && isspace((unsigned char)*p)) {
			p++;
		}
		if (p >= end || *p == '>') {
			break;
		}
		if (*p == '/') {
			*self_closing = 1;
			p++;
			continue;
		}
		{
			const char	*ns;
			size_t		nl;

			ns = p;
			while (p < end && *p != '=' && *p != '>' &&
			    *p != '/' && !isspace((unsigned char)*p)) {
				p++;
			}
			nl = (size_t)(p - ns);
			if (nl == 0) {
				p++;
				continue;
			}
			while (p < end && isspace((unsigned char)*p)) {
				p++;
			}
			if (count < SVG_ATTRS_MAX) {
				attrs[count].name = svg_xdups(ns, nl);
				attrs[count].value = NULL;
			}
			if (p < end && *p == '=') {
				char	*dec = NULL;

				p++;
				while (p < end &&
				    isspace((unsigned char)*p)) {
					p++;
				}
				if (p < end && (*p == '"' || *p == '\'')) {
					const char	*vs;
					char		quote;

					quote = *p++;
					vs = p;
					while (p < end && *p != quote) {
						p++;
					}
					dec = svg_decode_entities(vs,
					    (size_t)(p - vs));
					if (p < end) {
						p++;
					}
				} else {
					const char	*vs;

					vs = p;
					while (p < end && *p != '>' &&
					    !isspace((unsigned char)*p)) {
						p++;
					}
					dec = svg_decode_entities(vs,
					    (size_t)(p - vs));
				}
				if (count < SVG_ATTRS_MAX) {
					attrs[count].value = dec;
				} else {
					free(dec);
				}
			}
			count++;
		}
	}
	*pp = p;
	*nattrs = (count > SVG_ATTRS_MAX) ? SVG_ATTRS_MAX : count;
}

static void
svg_attrs_free(svg_parsed_attr_t *attrs, int nattrs)
{
	int	i;

	for (i = 0; i < nattrs; i++) {
		free(attrs[i].name);
		free(attrs[i].value);
		attrs[i].name = NULL;
		attrs[i].value = NULL;
	}
}

static const char *
svg_find_attr(svg_parsed_attr_t *attrs, int nattrs, const char *key)
{
	int	i;

	for (i = 0; i < nattrs; i++) {
		if (attrs[i].value != NULL &&
		    strcasecmp(attrs[i].name, key) == 0) {
			return (attrs[i].value);
		}
	}
	return (NULL);
}

static void
svg_apply_paint_attrs(svg_paint_t *paint, svg_parsed_attr_t *attrs,
    int nattrs)
{
	const char	*style;
	int		i;

	for (i = 0; i < nattrs; i++) {
		svg_paint_apply(paint, attrs[i].name, attrs[i].value);
	}
	style = svg_find_attr(attrs, nattrs, "style");
	if (style != NULL) {
		const char	*p = style;

		while (*p != '\0') {
			char	name[32], value[64];
			size_t	nl = 0, vl = 0;

			while (*p == ' ' || *p == ';') {
				p++;
			}
			if (*p == '\0') {
				break;
			}
			while (*p != '\0' && *p != ':' && *p != ';' &&
			    nl < sizeof(name) - 1) {
				name[nl++] = *p++;
			}
			name[nl] = '\0';
			if (*p == ':') {
				p++;
				while (*p != '\0' && *p != ';' &&
				    vl < sizeof(value) - 1) {
					value[vl++] = *p++;
				}
			}
			value[vl] = '\0';
			if (nl != 0 && vl != 0) {
				svg_paint_apply(paint, name, value);
			}
			while (*p != '\0' && *p != ';') {
				p++;
			}
		}
	}
}

static double
attr_num_or(svg_parsed_attr_t *attrs, int nattrs, const char *key,
    double fallback)
{
	const char	*v;

	v = svg_find_attr(attrs, nattrs, key);
	return ((v != NULL) ? svg_strtod(v, NULL) : fallback);
}

static void
svg_build_rect(svg_builder_t *bld, const svg_matrix_t *ctm,
    svg_parsed_attr_t *attrs, int nattrs)
{
	double	x, y, w, h, rx, ry;
	double	ccx[4], ccy[4], a0[4];
	double	tx, ty, step, ang, rad;
	int	corner, k;

	x = attr_num_or(attrs, nattrs, "x", 0.0);
	y = attr_num_or(attrs, nattrs, "y", 0.0);
	w = attr_num_or(attrs, nattrs, "width", 0.0);
	h = attr_num_or(attrs, nattrs, "height", 0.0);
	rx = attr_num_or(attrs, nattrs, "rx", 0.0);
	ry = attr_num_or(attrs, nattrs, "ry", 0.0);

	if (w <= 0.0 || h <= 0.0) {
		return;
	}
	if (rx == 0.0 && ry != 0.0) {
		rx = ry;
	}
	if (ry == 0.0 && rx != 0.0) {
		ry = rx;
	}
	if (rx > w / 2.0) {
		rx = w / 2.0;
	}
	if (ry > h / 2.0) {
		ry = h / 2.0;
	}

	sb_subpath_begin(bld);

	if (rx > 0.0 && ry > 0.0) {
		ccx[0] = x + w - rx;	ccy[0] = y + ry;	a0[0] = 270.0;
		ccx[1] = x + w - rx;	ccy[1] = y + h - ry;	a0[1] = 0.0;
		ccx[2] = x + rx;	ccy[2] = y + h - ry;	a0[2] = 90.0;
		ccx[3] = x + rx;	ccy[3] = y + ry;	a0[3] = 180.0;

		step = 90.0 / 6.0;
		for (corner = 0; corner < 4; corner++) {
			for (k = 0; k <= 6; k++) {
				ang = (a0[corner] + step * k) *
				    PI / 180.0;
				tx = ccx[corner] + rx * svg_cos(ang);
				ty = ccy[corner] + ry * svg_sin(ang);
				svg_matrix_map(ctm, tx, ty, &tx, &ty);
				sb_add_point(bld, tx, ty);
			}
		}
	} else {
		svg_matrix_map(ctm, x, y, &tx, &ty);
		sb_add_point(bld, tx, ty);
		svg_matrix_map(ctm, x + w, y, &tx, &ty);
		sb_add_point(bld, tx, ty);
		svg_matrix_map(ctm, x + w, y + h, &tx, &ty);
		sb_add_point(bld, tx, ty);
		svg_matrix_map(ctm, x, y + h, &tx, &ty);
		sb_add_point(bld, tx, ty);
	}
	(void)rad;
}

static void
svg_build_ellipse_geom(svg_builder_t *bld, const svg_matrix_t *ctm,
    double cx, double cy, double rx, double ry)
{
	double	ang, tx, ty;
	int	i, nseg;

	if (rx <= 0.0 || ry <= 0.0) {
		return;
	}

	nseg = (int)(svg_sqrt((rx + ry) / 2.0) * 12.0) + 12;
	if (nseg < 16) {
		nseg = 16;
	}
	if (nseg > 256) {
		nseg = 256;
	}

	sb_subpath_begin(bld);
	for (i = 0; i <= nseg; i++) {
		ang = 2.0 * PI * (double)i / (double)nseg;
		svg_matrix_map(ctm, cx + rx * svg_cos(ang),
		    cy + ry * svg_sin(ang), &tx, &ty);
		sb_add_point(bld, tx, ty);
	}
}

int
svg_parse(const char *data, size_t len, svg_doc_t **out)
{
	svg_doc_t		*doc;
	svg_builder_t		bld;
	svg_frame_t		frames[SVG_MAX_DEPTH];
	svg_parsed_attr_t	attrs[SVG_ATTRS_MAX];
	const char		*p, *end;
	int			sp, nattrs, self_closing;
	svg_elem_kind_t		kind;
	char			name[SVG_NAME_MAX];
	int			saw_root;

	*out = NULL;
	if (data == NULL || out == NULL) {
		return (-1);
	}
	if (len == 0) {
		len = strlen(data);
	}

	doc = (svg_doc_t *)malloc(sizeof(svg_doc_t));
	if (doc == NULL) {
		return (-1);
	}
	memset(doc, 0, sizeof(*doc));

	svg_builder_init(&bld);

	memset(frames, 0, sizeof(frames));
	M_ID(&frames[0].ctm);
	frames[0].paint.flags = SVG_FILL;
	frames[0].paint.fill = 0x000000;
	frames[0].paint.stroke_width = 1.0;
	frames[0].paint.fill_op = 1.0;
	frames[0].paint.stroke_op = 1.0;
	sp = 0;
	saw_root = 0;

	p = data;
	end = data + len;

	while (p < end) {
		if (*p != '<') {
			p++;
			continue;
		}

		if (end - p >= 4 && strncmp(p, "<!--", 4) == 0) {
			p += 4;
			while (p + 2 < end &&
			    !(p[0] == '-' && p[1] == '-' && p[2] == '>')) {
				p++;
			}
			p = (p + 2 < end) ? p + 3 : end;
			continue;
		}
		if (end - p >= 2 && p[1] == '?') {
			while (p + 1 < end && !(p[0] == '?' && p[1] == '>')) {
				p++;
			}
			p = (p + 1 < end) ? p + 2 : end;
			continue;
		}
		if (end - p >= 2 && p[1] == '!') {
			while (p < end && *p != '>') {
				p++;
			}
			p = (p < end) ? p + 1 : end;
			continue;
		}

		if (end - p >= 2 && p[1] == '/') {
			const char	*ns;
			size_t		nl;

			p += 2;
			ns = p;
			while (p < end && *p != '>' &&
			    !isspace((unsigned char)*p)) {
				p++;
			}
			nl = (size_t)(p - ns);
			while (p < end && *p != '>') {
				p++;
			}
			p = (p < end) ? p + 1 : end;
			while (sp > 0) {
				int	match =
				    (strlen(frames[sp].name) == nl) &&
				    (strncasecmp(frames[sp].name, ns,
				    nl) == 0);

				sp--;
				if (match != 0) {
					break;
				}
			}
			continue;
		}

		p++;
		{
			const char	*ns = p;
			size_t		nl;

			while (p < end && *p != '>' && *p != '/' &&
			    !isspace((unsigned char)*p)) {
				p++;
			}
			nl = (size_t)(p - ns);
			if (nl >= sizeof(name)) {
				nl = sizeof(name) - 1;
			}
			memcpy(name, ns, nl);
			name[nl] = '\0';
		}

		svg_collect_attrs(&p, end, attrs, &nattrs, &self_closing);
		if (p < end) {
			p++;
		}

		if (frames[sp].skip) {
			if (!self_closing && sp + 1 < SVG_MAX_DEPTH) {
				sp++;
				frame_set_name(&frames[sp], name);
				frames[sp].skip = 1;
			}
			svg_attrs_free(attrs, nattrs);
			continue;
		}

		kind = svg_classify(name);

		if (kind == SVG_ELEM_SKIP) {
			if (!self_closing && sp + 1 < SVG_MAX_DEPTH) {
				sp++;
				frame_set_name(&frames[sp], name);
				frames[sp].skip = 1;
				frames[sp].ctm = frames[sp - 1].ctm;
			}
			svg_attrs_free(attrs, nattrs);
			continue;
		}

		if (kind == SVG_ELEM_ROOT) {
			const char	*v;

			if (saw_root == 0) {
				saw_root = 1;
				v = svg_find_attr(attrs, nattrs, "width");
				if (v != NULL) {
					(void)svg_length_parse(v,
					    &doc->width);
				}
				v = svg_find_attr(attrs, nattrs, "height");
				if (v != NULL) {
					(void)svg_length_parse(v,
					    &doc->height);
				}
				v = svg_find_attr(attrs, nattrs, "viewBox");
				if (v != NULL) {
					char	*q;

					doc->view_x = svg_strtod(v, &q);
					doc->view_y = svg_strtod(q, &q);
					doc->view_w = svg_strtod(q, &q);
					doc->view_h = svg_strtod(q, &q);
					if (doc->view_w > 0.0 &&
					    doc->view_h > 0.0) {
						doc->has_viewbox = 1;
					}
				}
			}
			kind = SVG_ELEM_GROUP;
		}

		if (kind == SVG_ELEM_PATH || kind == SVG_ELEM_RECT ||
		    kind == SVG_ELEM_CIRCLE || kind == SVG_ELEM_ELLIPSE ||
		    kind == SVG_ELEM_LINE || kind == SVG_ELEM_POLYLINE ||
		    kind == SVG_ELEM_POLYGON) {
			svg_paint_t		paint;
			svg_matrix_t		ctm, local;
			const char		*t, *v;
			double			scale;
			uint32_t		extra = 0;

			paint = frames[sp].paint;
			ctm = frames[sp].ctm;
			t = svg_find_attr(attrs, nattrs, "transform");
			if (t != NULL &&
			    svg_parse_transform_attr(t, &local) == 0) {
				svg_matrix_multiply(&ctm, &ctm, &local);
			}
			svg_apply_paint_attrs(&paint, attrs, nattrs);

			scale = svg_matrix_scale(&ctm);
			paint.stroke_width *= scale;

			v = svg_find_attr(attrs, nattrs, "fill-rule");
			if (v != NULL && strcasecmp(v, "evenodd") == 0) {
				extra |= SVG_EVENODD;
			}
			if (paint.stroke_width > SVG_STROKE_MAX) {
				paint.stroke_width = SVG_STROKE_MAX;
			}

			switch (kind) {
			case SVG_ELEM_PATH:
				v = svg_find_attr(attrs, nattrs, "d");
				if (v != NULL) {
					svg_parse_path_d(&bld, &ctm, v);
				}
				break;
			case SVG_ELEM_RECT:
				svg_build_rect(&bld, &ctm, attrs, nattrs);
				extra |= SVG_CLOSED;
				break;
			case SVG_ELEM_CIRCLE:
				svg_build_ellipse_geom(&bld, &ctm,
				    attr_num_or(attrs, nattrs, "cx", 0.0),
				    attr_num_or(attrs, nattrs, "cy", 0.0),
				    attr_num_or(attrs, nattrs, "r", 0.0),
				    attr_num_or(attrs, nattrs, "r", 0.0));
				extra |= SVG_CLOSED;
				break;
			case SVG_ELEM_ELLIPSE:
				svg_build_ellipse_geom(&bld, &ctm,
				    attr_num_or(attrs, nattrs, "cx", 0.0),
				    attr_num_or(attrs, nattrs, "cy", 0.0),
				    attr_num_or(attrs, nattrs, "rx", 0.0),
				    attr_num_or(attrs, nattrs, "ry", 0.0));
				extra |= SVG_CLOSED;
				break;
			case SVG_ELEM_LINE: {
				double	tx, ty;

				sb_subpath_begin(&bld);
				svg_matrix_map(&ctm,
				    attr_num_or(attrs, nattrs, "x1", 0.0),
				    attr_num_or(attrs, nattrs, "y1", 0.0),
				    &tx, &ty);
				sb_add_point(&bld, tx, ty);
				svg_matrix_map(&ctm,
				    attr_num_or(attrs, nattrs, "x2", 0.0),
				    attr_num_or(attrs, nattrs, "y2", 0.0),
				    &tx, &ty);
				sb_add_point(&bld, tx, ty);
				paint.flags &= ~(uint32_t)SVG_FILL;
				break;
			}
			default: {
				v = svg_find_attr(attrs, nattrs, "points");
				if (v != NULL) {
					const char	*q = v;
					double		px, py;

					sb_subpath_begin(&bld);
					while (*q != '\0') {
						q = svg_skip_ws_commas(q);
						if (*q == '\0') {
							break;
						}
						px = svg_strtod(q,
						    (char **)&q);
						q = svg_skip_ws_commas(q);
						py = svg_strtod(q,
						    (char **)&q);
						svg_matrix_map(&ctm, px, py,
						    &px, &py);
						sb_add_point(&bld, px, py);
						if (bld.truncated != 0) {
							break;
						}
					}
				}
				if (kind == SVG_ELEM_POLYGON) {
					extra |= SVG_CLOSED;
				}
				break;
			}
			}

			sb_commit(&bld, &paint, extra);
			svg_attrs_free(attrs, nattrs);

			if (bld.truncated != 0) {
				goto done;
			}
			continue;
		}

		if (!self_closing && sp + 1 < SVG_MAX_DEPTH) {
			svg_matrix_t	local;
			const char	*t;

			sp++;
			frame_set_name(&frames[sp], name);
			frames[sp].paint = frames[sp - 1].paint;
			frames[sp].ctm = frames[sp - 1].ctm;
			frames[sp].skip = 0;
			t = svg_find_attr(attrs, nattrs, "transform");
			if (t != NULL &&
			    svg_parse_transform_attr(t, &local) == 0) {
				svg_matrix_multiply(&frames[sp].ctm,
				    &frames[sp].ctm, &local);
			}
			svg_apply_paint_attrs(&frames[sp].paint, attrs,
			    nattrs);
		}
		svg_attrs_free(attrs, nattrs);
	}

done:
	if (bld.truncated != 0 && bld.nshapes == 0) {
		svg_builder_free(&bld);
		free(doc);
		return (-1);
	}

	if (doc->has_viewbox == 0 && doc->width > 0.0 && doc->height > 0.0) {
		doc->view_x = 0.0;
		doc->view_y = 0.0;
		doc->view_w = doc->width;
		doc->view_h = doc->height;
		doc->has_viewbox = 1;
	}

	doc->shapes = bld.shapes;
	doc->nshapes = bld.nshapes;
	bld.shapes = NULL;
	bld.nshapes = 0;
	svg_builder_free(&bld);

	*out = doc;
	return (0);
}

void
svg_doc_free(svg_doc_t *doc)
{
	int	i;

	if (doc == NULL) {
		return;
	}
	for (i = 0; i < doc->nshapes; i++) {
		free(doc->shapes[i].pts);
		free(doc->shapes[i].subs);
	}
	free(doc->shapes);
	free(doc);
}

int
svg_view_transform(const svg_doc_t *doc, double out_w, double out_h,
    double m[6])
{
	double	s, tx, ty;

	if (doc == NULL || m == NULL || doc->has_viewbox == 0 ||
	    doc->view_w <= 0.0 || doc->view_h <= 0.0 ||
	    out_w <= 0.0 || out_h <= 0.0) {
		return (-1);
	}

	s = (out_w / doc->view_w < out_h / doc->view_h) ?
	    out_w / doc->view_w : out_h / doc->view_h;
	tx = (out_w - doc->view_w * s) / 2.0 - doc->view_x * s;
	ty = (out_h - doc->view_h * s) / 2.0 - doc->view_y * s;

	m[0] = s;
	m[1] = 0.0;
	m[2] = 0.0;
	m[3] = s;
	m[4] = tx;
	m[5] = ty;
	return (0);
}
