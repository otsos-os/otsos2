/* !DEFINES!

$define %func css_parse as function with args const char *, css_sheet **
$define %func css_free as procedure with args css_sheet *

*/

/* !SPACE!

$space %internal css_skip_ws, css_skip_comment, css_read_ident
$space %internal css_compound_parse, css_selector_parse
$space %internal css_decls_parse, css_rule_add, css_xdups

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

static void
css_compound_reset(css_compound_t *cp)
{
	int	k;

	free(cp->tag);
	free(cp->id);
	for (k = 0; k < cp->ncls; k++) {
		free(cp->cls[k]);
	}
	memset(cp, 0, sizeof(*cp));
}

static int
css_compound_parse(const char **pp, const char *end, css_compound_t *out,
    int *spec_ids, int *spec_classes, int *spec_types)
{
	const char	*p;

	memset(out, 0, sizeof(*out));
	p = css_skip_blank(*pp, end);

	while (p < end && *p != ' ' && *p != '\t' && *p != '\n' &&
	    *p != '\r' && *p != ',' && *p != '{') {
		if (*p == '.' || *p == '#') {
			char	*name;
			char	quote = *p;

			p++;
			name = css_read_ident(&p, end);
			if (name == NULL) {
				goto fail;
			}
			if (quote == '#') {
				if (out->id != NULL) {
					free(name);
					continue;
				}
				out->id = name;
				(*spec_ids)++;
			} else {
				if (out->ncls < CSS_MAX_CLASSES) {
					out->cls[out->ncls++] = name;
					(*spec_classes)++;
				} else {
					free(name);
				}
			}
			continue;
		}
		if (*p == ':' || *p == '[' || *p == '>' || *p == '+' ||
		    *p == '~' || *p == '*') {
			goto fail;
		}
		if (out->tag == NULL && css_is_ident_char(*p) != 0) {
			out->tag = css_read_ident(&p, end);
			if (out->tag == NULL) {
				goto fail;
			}
			(*spec_types)++;
			continue;
		}
		goto fail;
	}

	*pp = p;
	return (0);

fail:
	css_compound_reset(out);
	return (-1);
}

static int
css_selector_parse(const char **pp, const char *end, css_selector_t *out)
{
	const char	*p;
	int		expect_compound;

	memset(out, 0, sizeof(*out));
	p = *pp;
	expect_compound = 1;

	while (p < end) {
		p = css_skip_blank(p, end);
		if (p >= end || *p == ',' || *p == '{') {
			break;
		}
		if (expect_compound == 0) {
			expect_compound = 1;
			continue;
		}
		if (out->nparts >= CSS_MAX_COMPOUNDS ||
		    css_compound_parse(&p, end,
		    &out->parts[out->nparts], &out->spec_ids,
		    &out->spec_classes, &out->spec_types) != 0) {
			return (-1);
		}
		out->nparts++;
		expect_compound = 0;
	}

	if (expect_compound != 0 || out->nparts == 0) {
		return (-1);
	}
	*pp = p;
	return (0);
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
	int	i, k;

	for (i = 0; i < sel->nparts; i++) {
		free(sel->parts[i].tag);
		free(sel->parts[i].id);
		for (k = 0; k < sel->parts[i].ncls; k++) {
			free(sel->parts[i].cls[k]);
		}
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

int
css_parse(const char *text, css_sheet_t **out)
{
	css_sheet_t	*sheet;
	const char	*p, *end;
	int		order, i;

	*out = NULL;
	if (text == NULL || out == NULL) {
		return (-1);
	}

	sheet = (css_sheet_t *)malloc(sizeof(css_sheet_t));
	if (sheet == NULL) {
		return (-1);
	}
	memset(sheet, 0, sizeof(*sheet));

	end = text + strlen(text);
	p = text;
	order = 0;

	while (p < end) {
		const char	*sel_start, *sel_end, *body_start, *body_end;
		css_rule_t	rule;
		int		depth;

		p = css_skip_blank(p, end);
		if (p >= end) {
			break;
		}

		if (*p == '@') {
			while (p < end && *p != ';' && *p != '{' &&
			    *p != '}') {
				p++;
			}
			if (p < end && *p == '{') {
				depth = 1;
				p++;
				while (p < end && depth > 0) {
					if (*p == '{') {
						depth++;
					} else if (*p == '}') {
						depth--;
					}
					p++;
				}
			} else if (p < end) {
				p++;
			}
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
				rule.order = order++;
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

	*out = sheet;
	return (0);
}
