/* !DEFINES!

$define %type svg_shape_flags as bit flags for shape paint and closure
$define %type svg_shape as flattened outline with paint, in SVG user units
$define %type svg_doc as parsed SVG document geometry
$define %func svg_parse as function with args const char *, size_t, svg_doc **
$define %func svg_doc_free as procedure with args svg_doc *
$define %func svg_view_transform as function with args const svg_doc *, double, double, double *

*/

/* !SPACE!

$space %export svg_shape_t, svg_doc_t
$space %export SVG_FILL, SVG_STROKE, SVG_CLOSED
$space %export svg_parse, svg_doc_free, svg_view_transform

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.
 */

#ifndef _SVG_H
#define _SVG_H

#include <stddef.h>
#include <stdint.h>

#define SVG_VERSION_MAJOR	0
#define SVG_VERSION_MINOR	1
#define SVG_FILL		0x00000001u
#define SVG_STROKE		0x00000002u
#define SVG_CLOSED		0x00000004u
#define SVG_EVENODD		0x00000008u

typedef struct svg_shape {
	uint32_t	flags;
	uint32_t	fill;
	uint32_t	stroke;
	double		stroke_width;
	double		fill_opacity;
	double		stroke_opacity;
	double		*pts;
	int		npts;
	int		*subs;
	int		nsubs;
} svg_shape_t;

typedef struct svg_doc {
	double		width;
	double		height;
	double		view_x;
	double		view_y;
	double		view_w;
	double		view_h;
	int		has_viewbox;
	svg_shape_t	*shapes;
	int		nshapes;
} svg_doc_t;

int	svg_parse(const char *data, size_t len, svg_doc_t **out);
void	svg_doc_free(svg_doc_t *doc);
int	svg_view_transform(const svg_doc_t *doc, double out_w, double out_h,
	    double m[6]);

#endif
