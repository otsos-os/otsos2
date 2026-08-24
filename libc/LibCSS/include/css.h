/* !DEFINES!

$define %type css_sheet as parsed stylesheet
$define %type css_computed as resolved property set for one node
$define %type css_node_iface as host-provided DOM access callbacks
$define %func css_parse as function with args const char *, css_sheet **
$define %func css_free as procedure with args css_sheet *
$define %func css_compute as function with args const css_sheet *, const css_node_iface *, const void *, css_computed *
$define %func css_apply_declarations as function with args css_computed *, const char *, int32_t

*/

/* !SPACE!

%export css_sheet_t, css_computed_t, css_node_iface_t
$space %export CSS_PROP_COLOR, CSS_PROP_SCALE, CSS_PROP_BOLD
$space %export CSS_PROP_UNDERLINE, CSS_PROP_STRIKE, CSS_PROP_DISPLAY_NONE
$space %export css_parse, css_free, css_compute, css_apply_declarations

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

#ifndef _CSS_H
#define _CSS_H

#include <stddef.h>
#include <stdint.h>

#define CSS_VERSION_MAJOR	0
#define CSS_VERSION_MINOR	1
#define CSS_PROP_COLOR		0x00000001u
#define CSS_PROP_SCALE		0x00000002u
#define CSS_PROP_BOLD		0x00000004u
#define CSS_PROP_UNDERLINE	0x00000008u
#define CSS_PROP_STRIKE		0x00000010u
#define CSS_PROP_DISPLAY_NONE	0x00000020u
#define CSS_PROP_BGCOLOR	0x00000040u
#define CSS_PROP_ALIGN_CENTER	0x00000080u
#define CSS_PROP_MARGIN_TOP	0x00000100u
#define CSS_PROP_MARGIN_BOTTOM	0x00000200u

typedef struct css_computed {
	uint32_t	set;
	uint32_t	color;
	uint32_t	bgcolor;
	int32_t		font_scale;
	int32_t		margin_top;
	int32_t		margin_bottom;
	int		align_center;
	int		bold;
	int		underline;
	int		strike;
	int		display_none;
} css_computed_t;
typedef struct css_node_iface {
	const char	*(*tag)(const void *node);
	const char	*(*get_attr)(const void *node, const char *key);
	const void	*(*parent)(const void *node);
} css_node_iface_t;

typedef struct css_sheet css_sheet_t;
int	css_parse(const char *text, css_sheet_t **out);
void	css_free(css_sheet_t *sheet);
int	css_compute(const css_sheet_t *sheet, const css_node_iface_t *iface,
	    const void *node, css_computed_t *out);
int	css_apply_declarations(css_computed_t *out, const char *decl_text,
	    int32_t base_scale);

#endif
