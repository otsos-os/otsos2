/* !DEFINES!

$define %func svg_sin as function with args double
$define %func svg_cos as function with args double
$define %func svg_sqrt as function with args double
$define %func svg_atan2 as function with args double, double
$define %func svg_acos as function with args double
$define %func svg_strtod as function with args const char *, char **

*/

/* !SPACE!

$space %internal svg_sin, svg_cos, svg_sqrt, svg_atan2, svg_acos
$space %internal svg_strtod

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
 */

#ifndef _SVG_INT_H
#define _SVG_INT_H

#include <svg.h>

#define SVG_MAX_SHAPES		4096
#define SVG_MAX_POINTS		65536
#define SVG_MAX_SUBPATHS	1024
#define SVG_MAX_DEPTH		32
#define SVG_STROKE_MAX		4096.0

double	svg_sin(double x);
double	svg_cos(double x);
double	svg_sqrt(double x);
double	svg_atan2(double y, double x);
double	svg_acos(double x);
double	svg_strtod(const char *str, char **endptr);

#endif
