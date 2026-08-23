/* !DEFINES!

$define %type html_parser as internal parser state
$define %func html_parse as function with args const char *, size_t
$define %func html_doc_free as procedure with args html_doc *

*/

/* !SPACE!

$space %internal html_decode_entities, html_tag_lookup, html_is_void_tag
$space %internal html_node_create, html_node_add_child, html_attr_add
$space %internal html_is_raw_text, html_auto_closes, html_entity_named
$space %internal html_put_utf8_fold, html_node_collect_text
$space %export html_parse, html_doc_free, html_node_get_attr
$space %export html_node_find, html_node_text

*/

#include <ctype.h>
#include <html.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Depth ceiling for the open-element stack.  Real documents nest a few dozen
 * levels; a hostile or truncated one can nest without bound, and the layout
 * walk is recursive.  Past this limit new elements are still parsed but
 * attached at the cap instead of deepening the tree.
 */
#define HTML_MAX_DEPTH		128
#define HTML_TAG_NAME_MAX	64

static html_tag_t
html_tag_lookup(const char *name)
{
	if (strcasecmp(name, "html") == 0) return (HTML_TAG_HTML);
	if (strcasecmp(name, "head") == 0) return (HTML_TAG_HEAD);
	if (strcasecmp(name, "title") == 0) return (HTML_TAG_TITLE);
	if (strcasecmp(name, "body") == 0) return (HTML_TAG_BODY);
	if (strcasecmp(name, "h1") == 0) return (HTML_TAG_H1);
	if (strcasecmp(name, "h2") == 0) return (HTML_TAG_H2);
	if (strcasecmp(name, "h3") == 0) return (HTML_TAG_H3);
	if (strcasecmp(name, "h4") == 0) return (HTML_TAG_H4);
	if (strcasecmp(name, "h5") == 0) return (HTML_TAG_H5);
	if (strcasecmp(name, "h6") == 0) return (HTML_TAG_H6);
	if (strcasecmp(name, "p") == 0) return (HTML_TAG_P);
	if (strcasecmp(name, "a") == 0) return (HTML_TAG_A);
	if (strcasecmp(name, "div") == 0) return (HTML_TAG_DIV);
	if (strcasecmp(name, "span") == 0) return (HTML_TAG_SPAN);
	if (strcasecmp(name, "ul") == 0) return (HTML_TAG_UL);
	if (strcasecmp(name, "ol") == 0) return (HTML_TAG_OL);
	if (strcasecmp(name, "li") == 0) return (HTML_TAG_LI);
	if (strcasecmp(name, "br") == 0) return (HTML_TAG_BR);
	if (strcasecmp(name, "hr") == 0) return (HTML_TAG_HR);
	if (strcasecmp(name, "pre") == 0) return (HTML_TAG_PRE);
	if (strcasecmp(name, "code") == 0) return (HTML_TAG_CODE);
	if (strcasecmp(name, "b") == 0) return (HTML_TAG_B);
	if (strcasecmp(name, "strong") == 0) return (HTML_TAG_STRONG);
	if (strcasecmp(name, "i") == 0) return (HTML_TAG_I);
	if (strcasecmp(name, "em") == 0) return (HTML_TAG_EM);
	if (strcasecmp(name, "u") == 0) return (HTML_TAG_U);
	if (strcasecmp(name, "img") == 0) return (HTML_TAG_IMG);
	if (strcasecmp(name, "table") == 0) return (HTML_TAG_TABLE);
	if (strcasecmp(name, "tr") == 0) return (HTML_TAG_TR);
	if (strcasecmp(name, "td") == 0) return (HTML_TAG_TD);
	if (strcasecmp(name, "th") == 0) return (HTML_TAG_TH);
	if (strcasecmp(name, "form") == 0) return (HTML_TAG_FORM);
	if (strcasecmp(name, "input") == 0) return (HTML_TAG_INPUT);
	if (strcasecmp(name, "button") == 0) return (HTML_TAG_BUTTON);
	if (strcasecmp(name, "blockquote") == 0) return (HTML_TAG_BLOCKQUOTE);
	if (strcasecmp(name, "style") == 0) return (HTML_TAG_STYLE);
	if (strcasecmp(name, "script") == 0) return (HTML_TAG_SCRIPT);
	if (strcasecmp(name, "noscript") == 0) return (HTML_TAG_NOSCRIPT);
	if (strcasecmp(name, "template") == 0) return (HTML_TAG_TEMPLATE);
	if (strcasecmp(name, "textarea") == 0) return (HTML_TAG_TEXTAREA);
	if (strcasecmp(name, "meta") == 0) return (HTML_TAG_META);
	if (strcasecmp(name, "link") == 0) return (HTML_TAG_LINK);
	if (strcasecmp(name, "base") == 0) return (HTML_TAG_BASE);
	if (strcasecmp(name, "article") == 0) return (HTML_TAG_ARTICLE);
	if (strcasecmp(name, "section") == 0) return (HTML_TAG_SECTION);
	if (strcasecmp(name, "nav") == 0) return (HTML_TAG_NAV);
	if (strcasecmp(name, "aside") == 0) return (HTML_TAG_ASIDE);
	if (strcasecmp(name, "header") == 0) return (HTML_TAG_HEADER);
	if (strcasecmp(name, "footer") == 0) return (HTML_TAG_FOOTER);
	if (strcasecmp(name, "main") == 0) return (HTML_TAG_MAIN);
	if (strcasecmp(name, "figure") == 0) return (HTML_TAG_FIGURE);
	if (strcasecmp(name, "figcaption") == 0) return (HTML_TAG_FIGCAPTION);
	if (strcasecmp(name, "dl") == 0) return (HTML_TAG_DL);
	if (strcasecmp(name, "dt") == 0) return (HTML_TAG_DT);
	if (strcasecmp(name, "dd") == 0) return (HTML_TAG_DD);
	if (strcasecmp(name, "thead") == 0) return (HTML_TAG_THEAD);
	if (strcasecmp(name, "tbody") == 0) return (HTML_TAG_TBODY);
	if (strcasecmp(name, "tfoot") == 0) return (HTML_TAG_TFOOT);
	if (strcasecmp(name, "caption") == 0) return (HTML_TAG_CAPTION);
	if (strcasecmp(name, "colgroup") == 0) return (HTML_TAG_COLGROUP);
	if (strcasecmp(name, "col") == 0) return (HTML_TAG_COL);
	if (strcasecmp(name, "small") == 0) return (HTML_TAG_SMALL);
	if (strcasecmp(name, "big") == 0) return (HTML_TAG_BIG);
	if (strcasecmp(name, "sub") == 0) return (HTML_TAG_SUB);
	if (strcasecmp(name, "sup") == 0) return (HTML_TAG_SUP);
	if (strcasecmp(name, "del") == 0) return (HTML_TAG_DEL);
	if (strcasecmp(name, "s") == 0) return (HTML_TAG_DEL);
	if (strcasecmp(name, "strike") == 0) return (HTML_TAG_DEL);
	if (strcasecmp(name, "ins") == 0) return (HTML_TAG_INS);
	if (strcasecmp(name, "mark") == 0) return (HTML_TAG_MARK);
	if (strcasecmp(name, "abbr") == 0) return (HTML_TAG_ABBR);
	if (strcasecmp(name, "cite") == 0) return (HTML_TAG_CITE);
	if (strcasecmp(name, "q") == 0) return (HTML_TAG_Q);
	if (strcasecmp(name, "kbd") == 0) return (HTML_TAG_KBD);
	if (strcasecmp(name, "samp") == 0) return (HTML_TAG_SAMP);
	if (strcasecmp(name, "var") == 0) return (HTML_TAG_VAR);
	if (strcasecmp(name, "tt") == 0) return (HTML_TAG_TT);
	if (strcasecmp(name, "label") == 0) return (HTML_TAG_LABEL);
	if (strcasecmp(name, "select") == 0) return (HTML_TAG_SELECT);
	if (strcasecmp(name, "option") == 0) return (HTML_TAG_OPTION);
	if (strcasecmp(name, "fieldset") == 0) return (HTML_TAG_FIELDSET);
	if (strcasecmp(name, "legend") == 0) return (HTML_TAG_LEGEND);
	if (strcasecmp(name, "wbr") == 0) return (HTML_TAG_WBR);
	if (strcasecmp(name, "center") == 0) return (HTML_TAG_CENTER);
	if (strcasecmp(name, "font") == 0) return (HTML_TAG_FONT);
	if (strcasecmp(name, "address") == 0) return (HTML_TAG_ADDRESS);
	if (strcasecmp(name, "time") == 0) return (HTML_TAG_TIME);
	if (strcasecmp(name, "param") == 0) return (HTML_TAG_PARAM);
	if (strcasecmp(name, "source") == 0) return (HTML_TAG_SOURCE);
	if (strcasecmp(name, "track") == 0) return (HTML_TAG_TRACK);
	if (strcasecmp(name, "area") == 0) return (HTML_TAG_AREA);
	if (strcasecmp(name, "embed") == 0) return (HTML_TAG_EMBED);
	if (strcasecmp(name, "svg") == 0) return (HTML_TAG_SVG);
	return (HTML_TAG_UNKNOWN);
}

static int
html_is_void_tag(html_tag_t tag)
{
	switch (tag) {
	case HTML_TAG_BR:
	case HTML_TAG_HR:
	case HTML_TAG_IMG:
	case HTML_TAG_INPUT:
	case HTML_TAG_META:
	case HTML_TAG_LINK:
	case HTML_TAG_BASE:
	case HTML_TAG_COL:
	case HTML_TAG_WBR:
	case HTML_TAG_PARAM:
	case HTML_TAG_SOURCE:
	case HTML_TAG_TRACK:
	case HTML_TAG_AREA:
	case HTML_TAG_EMBED:
		return (1);
	default:
		return (0);
	}
}

/*
 * Elements whose content is not markup.  Their bodies are consumed verbatim
 * up to the matching close tag, so a "<" inside them never opens an element.
 * STYLE and SCRIPT are here to be discarded -- LibHtml implements no CSS and
 * no scripting, and this is what keeps their text off the page.
 */
static int
html_is_raw_text(html_tag_t tag)
{
	switch (tag) {
	case HTML_TAG_STYLE:
	case HTML_TAG_SCRIPT:
	case HTML_TAG_TEXTAREA:
	case HTML_TAG_TEMPLATE:
	case HTML_TAG_NOSCRIPT:
		return (1);
	default:
		return (0);
	}
}

/*
 * Optional-end-tag rules (HTML5 12.2.6.4, reduced to what matters for
 * rendering): opening `next` while `open` is still current implies `open`
 * closed.  Without this a page that never writes </p> or </li> -- which is
 * most real HTML -- nests every paragraph inside the previous one, and the
 * whole document marches right off the viewport.
 */
static int
html_auto_closes(html_tag_t open, html_tag_t next)
{
	switch (open) {
	case HTML_TAG_P:
		/* Any block-level start closes an open paragraph. */
		switch (next) {
		case HTML_TAG_P:
		case HTML_TAG_DIV:
		case HTML_TAG_UL:
		case HTML_TAG_OL:
		case HTML_TAG_LI:
		case HTML_TAG_DL:
		case HTML_TAG_DT:
		case HTML_TAG_DD:
		case HTML_TAG_TABLE:
		case HTML_TAG_PRE:
		case HTML_TAG_HR:
		case HTML_TAG_BLOCKQUOTE:
		case HTML_TAG_H1:
		case HTML_TAG_H2:
		case HTML_TAG_H3:
		case HTML_TAG_H4:
		case HTML_TAG_H5:
		case HTML_TAG_H6:
		case HTML_TAG_ARTICLE:
		case HTML_TAG_SECTION:
		case HTML_TAG_NAV:
		case HTML_TAG_ASIDE:
		case HTML_TAG_HEADER:
		case HTML_TAG_FOOTER:
		case HTML_TAG_MAIN:
		case HTML_TAG_FIGURE:
		case HTML_TAG_FIGCAPTION:
		case HTML_TAG_ADDRESS:
		case HTML_TAG_FIELDSET:
		case HTML_TAG_FORM:
		case HTML_TAG_CENTER:
			return (1);
		default:
			return (0);
		}
	case HTML_TAG_LI:
		return (next == HTML_TAG_LI);
	case HTML_TAG_DT:
	case HTML_TAG_DD:
		return (next == HTML_TAG_DT || next == HTML_TAG_DD);
	case HTML_TAG_TD:
	case HTML_TAG_TH:
		return (next == HTML_TAG_TD || next == HTML_TAG_TH ||
		    next == HTML_TAG_TR || next == HTML_TAG_THEAD ||
		    next == HTML_TAG_TBODY || next == HTML_TAG_TFOOT);
	case HTML_TAG_TR:
		return (next == HTML_TAG_TR || next == HTML_TAG_THEAD ||
		    next == HTML_TAG_TBODY || next == HTML_TAG_TFOOT);
	case HTML_TAG_THEAD:
	case HTML_TAG_TBODY:
	case HTML_TAG_TFOOT:
		return (next == HTML_TAG_TBODY || next == HTML_TAG_TFOOT ||
		    next == HTML_TAG_THEAD);
	case HTML_TAG_OPTION:
		return (next == HTML_TAG_OPTION);
	default:
		return (0);
	}
}

typedef struct html_entity_map {
	const char	*name;
	unsigned long	code;
} html_entity_map_t;

/*
 * Named references worth carrying.  The full HTML5 table is ~2200 entries;
 * these are the ones that actually appear in prose and would otherwise show
 * up as literal "&mdash;" on screen.
 */
static const html_entity_map_t html_entities[] = {
	{ "amp",	'&' },
	{ "lt",		'<' },
	{ "gt",		'>' },
	{ "quot",	'"' },
	{ "apos",	'\'' },
	{ "nbsp",	' ' },
	{ "ensp",	' ' },
	{ "emsp",	' ' },
	{ "thinsp",	' ' },
	{ "shy",	'-' },
	{ "ndash",	0x2013 },
	{ "mdash",	0x2014 },
	{ "lsquo",	0x2018 },
	{ "rsquo",	0x2019 },
	{ "sbquo",	0x201A },
	{ "ldquo",	0x201C },
	{ "rdquo",	0x201D },
	{ "bdquo",	0x201E },
	{ "dagger",	0x2020 },
	{ "Dagger",	0x2021 },
	{ "bull",	0x2022 },
	{ "hellip",	0x2026 },
	{ "permil",	0x2030 },
	{ "prime",	0x2032 },
	{ "Prime",	0x2033 },
	{ "lsaquo",	0x2039 },
	{ "rsaquo",	0x203A },
	{ "oline",	0x203E },
	{ "frasl",	'/' },
	{ "euro",	0x20AC },
	{ "trade",	0x2122 },
	{ "larr",	0x2190 },
	{ "uarr",	0x2191 },
	{ "rarr",	0x2192 },
	{ "darr",	0x2193 },
	{ "harr",	0x2194 },
	{ "minus",	'-' },
	{ "lowast",	'*' },
	{ "asymp",	0x2248 },
	{ "ne",		0x2260 },
	{ "le",		0x2264 },
	{ "ge",		0x2265 },
	{ "iexcl",	0xA1 },
	{ "cent",	0xA2 },
	{ "pound",	0xA3 },
	{ "curren",	0xA4 },
	{ "yen",	0xA5 },
	{ "brvbar",	0xA6 },
	{ "sect",	0xA7 },
	{ "uml",	0xA8 },
	{ "copy",	0xA9 },
	{ "ordf",	0xAA },
	{ "laquo",	0xAB },
	{ "not",	0xAC },
	{ "reg",	0xAE },
	{ "macr",	0xAF },
	{ "deg",	0xB0 },
	{ "plusmn",	0xB1 },
	{ "sup2",	0xB2 },
	{ "sup3",	0xB3 },
	{ "acute",	0xB4 },
	{ "micro",	0xB5 },
	{ "para",	0xB6 },
	{ "middot",	0xB7 },
	{ "cedil",	0xB8 },
	{ "sup1",	0xB9 },
	{ "ordm",	0xBA },
	{ "raquo",	0xBB },
	{ "frac14",	0xBC },
	{ "frac12",	0xBD },
	{ "frac34",	0xBE },
	{ "iquest",	0xBF },
	{ "times",	0xD7 },
	{ "divide",	0xF7 },
	{ NULL,		0 }
};

static int
html_entity_named(const char *name, unsigned long *out_code)
{
	int	i;

	/* Case-sensitive on purpose: &Dagger; and &dagger; differ. */
	for (i = 0; html_entities[i].name != NULL; i++) {
		if (strcmp(name, html_entities[i].name) == 0) {
			*out_code = html_entities[i].code;
			return (1);
		}
	}
	return (0);
}

/*
 * Fold a code point into what the 5x7 bitmap font can actually draw, which
 * is byte-indexed.  ASCII passes through; the punctuation that shows up
 * constantly in prose degrades to a sensible ASCII stand-in; Latin-1 stays
 * as its single byte; everything else becomes '?' rather than emitting a
 * multi-byte sequence the renderer would paint as garbage.
 *
 * Returns bytes written (never more than 3).
 */
static size_t
html_put_utf8_fold(char *dst, unsigned long code)
{
	if (code == 0) {
		return (0);
	}
	if (code < 0x80) {
		dst[0] = (char)code;
		return (1);
	}

	switch (code) {
	case 0x2013:			/* en dash */
	case 0x2014:			/* em dash */
	case 0x2015:
		dst[0] = '-';
		return (1);
	case 0x2018:
	case 0x2019:
	case 0x201A:
	case 0x2032:
		dst[0] = '\'';
		return (1);
	case 0x201C:
	case 0x201D:
	case 0x201E:
	case 0x2033:
		dst[0] = '"';
		return (1);
	case 0x2022:
	case 0x00B7:
		dst[0] = '*';
		return (1);
	case 0x2026:			/* ellipsis */
		dst[0] = '.';
		dst[1] = '.';
		dst[2] = '.';
		return (3);
	case 0x2190:
		dst[0] = '<';
		dst[1] = '-';
		return (2);
	case 0x2192:
		dst[0] = '-';
		dst[1] = '>';
		return (2);
	case 0x2194:
		dst[0] = '<';
		dst[1] = '>';
		return (2);
	case 0x2191:
		dst[0] = '^';
		return (1);
	case 0x2193:
		dst[0] = 'v';
		return (1);
	case 0x2260:
		dst[0] = '!';
		dst[1] = '=';
		return (2);
	case 0x2264:
		dst[0] = '<';
		dst[1] = '=';
		return (2);
	case 0x2265:
		dst[0] = '>';
		dst[1] = '=';
		return (2);
	case 0x2248:
		dst[0] = '~';
		return (1);
	case 0x00D7:
		dst[0] = 'x';
		return (1);
	case 0x00F7:
		dst[0] = '/';
		return (1);
	case 0x20AC:
		dst[0] = 'E';
		dst[1] = 'U';
		dst[2] = 'R';
		return (3);
	case 0x2122:
		dst[0] = '(';
		dst[1] = 'T';
		dst[2] = ')';
		return (3);
	case 0x00A0:
		dst[0] = ' ';
		return (1);
	default:
		break;
	}

	if (code < 0x100) {
		dst[0] = (char)code;
		return (1);
	}
	dst[0] = '?';
	return (1);
}

static char *
html_decode_entities(const char *str, size_t len)
{
	char		*out;
	char		name[32];
	unsigned long	code;
	size_t		i, j, k, name_len;
	int		is_hex;

	/*
	 * Worst case an entity expands to 3 bytes ("&#8230;" -> "...").  The
	 * shortest possible reference is 3 characters ("&#;"), so 3 bytes of
	 * output per input byte is a safe ceiling and needs no realloc.
	 */
	out = (char *)malloc(len * 3 + 1);
	if (out == NULL) {
		return (NULL);
	}

	i = 0;
	j = 0;
	while (i < len) {
		if (str[i] != '&') {
			out[j++] = str[i++];
			continue;
		}

		/* Numeric reference: &#1234; or &#x4D2; */
		if (i + 2 < len && str[i + 1] == '#') {
			k = i + 2;
			code = 0;
			is_hex = 0;
			if (k < len && (str[k] == 'x' || str[k] == 'X')) {
				is_hex = 1;
				k++;
			}
			while (k < len && str[k] != ';') {
				if (is_hex &&
				    isxdigit((unsigned char)str[k])) {
					code = code * 16 +
					    (unsigned long)(isdigit(
					    (unsigned char)str[k]) ?
					    str[k] - '0' :
					    tolower((unsigned char)str[k]) -
					    'a' + 10);
				} else if (!is_hex &&
				    isdigit((unsigned char)str[k])) {
					code = code * 10 +
					    (unsigned long)(str[k] - '0');
				} else {
					break;
				}
				/* Reject absurd values before they overflow. */
				if (code > 0x10FFFF) {
					break;
				}
				k++;
			}
			if (k < len && str[k] == ';' && code != 0 &&
			    code <= 0x10FFFF) {
				j += html_put_utf8_fold(out + j, code);
				i = k + 1;
				continue;
			}
			/* Malformed: emit the '&' literally and move on. */
			out[j++] = str[i++];
			continue;
		}

		/* Named reference. */
		k = i + 1;
		name_len = 0;
		while (k < len && name_len < sizeof(name) - 1 &&
		    (isalnum((unsigned char)str[k]) != 0)) {
			name[name_len++] = str[k];
			k++;
		}
		name[name_len] = '\0';
		if (name_len != 0 && k < len && str[k] == ';' &&
		    html_entity_named(name, &code)) {
			j += html_put_utf8_fold(out + j, code);
			i = k + 1;
			continue;
		}

		out[j++] = str[i++];
	}
	out[j] = '\0';
	return (out);
}

static html_node_t *
html_node_create(html_tag_t tag, const char *tag_name)
{
	html_node_t *node;

	node = (html_node_t *)malloc(sizeof(html_node_t));
	if (node == NULL) {
		return (NULL);
	}
	memset(node, 0, sizeof(html_node_t));
	node->tag = tag;
	if (tag_name != NULL) {
		node->tag_name = strdup(tag_name);
	}
	return (node);
}

static void
html_node_add_child(html_node_t *parent, html_node_t *child)
{
	if (parent == NULL || child == NULL) {
		return;
	}
	child->parent = parent;
	child->next_sibling = NULL;
	child->prev_sibling = parent->last_child;
	if (parent->last_child != NULL) {
		parent->last_child->next_sibling = child;
	} else {
		parent->first_child = child;
	}
	parent->last_child = child;
}

static void
html_attr_add(html_node_t *node, const char *name, const char *value)
{
	html_attr_t *attr, **tail;

	if (node == NULL || name == NULL) {
		return;
	}
	attr = (html_attr_t *)malloc(sizeof(html_attr_t));
	if (attr == NULL) {
		return;
	}
	attr->name = strdup(name);
	attr->value = value ? strdup(value) : strdup("");
	attr->next = NULL;

	tail = &node->attrs;
	while (*tail != NULL) {
		tail = &(*tail)->next;
	}
	*tail = attr;
}

const char *
html_node_get_attr(const html_node_t *node, const char *key)
{
	const html_attr_t *a;

	if (node == NULL || key == NULL) {
		return (NULL);
	}
	for (a = node->attrs; a != NULL; a = a->next) {
		if (a->name && strcasecmp(a->name, key) == 0) {
			return (a->value);
		}
	}
	return (NULL);
}

const html_node_t *
html_node_find(const html_node_t *root, html_tag_t tag)
{
	const html_node_t	*child, *found;

	if (root == NULL) {
		return (NULL);
	}
	if (root->tag == tag) {
		return (root);
	}
	for (child = root->first_child; child != NULL;
	    child = child->next_sibling) {
		found = html_node_find(child, tag);
		if (found != NULL) {
			return (found);
		}
	}
	return (NULL);
}

static void
html_node_collect_text(const html_node_t *node, char *buf, size_t cap,
    size_t *used)
{
	const html_node_t	*child;
	size_t			len;

	if (node == NULL || *used + 1 >= cap) {
		return;
	}
	if (node->tag == HTML_TAG_TEXT) {
		if (node->text == NULL) {
			return;
		}
		len = strlen(node->text);
		if (*used + len >= cap) {
			len = cap - *used - 1;
		}
		memcpy(buf + *used, node->text, len);
		*used += len;
		buf[*used] = '\0';
		return;
	}
	/* Raw-text bodies are not content: never fold them into text. */
	if (html_is_raw_text(node->tag)) {
		return;
	}
	for (child = node->first_child; child != NULL;
	    child = child->next_sibling) {
		html_node_collect_text(child, buf, cap, used);
	}
}

char *
html_node_text(const html_node_t *node)
{
	char	*out;
	size_t	used;

	if (node == NULL) {
		return (NULL);
	}
	/*
	 * Bounded on purpose: callers want a title or a cell label, not a
	 * whole subtree, and an unbounded join on a deep document is a easy
	 * way to burn memory for nothing.
	 */
	out = (char *)malloc(1024);
	if (out == NULL) {
		return (NULL);
	}
	out[0] = '\0';
	used = 0;
	html_node_collect_text(node, out, 1024, &used);
	return (out);
}

static void
html_node_free(html_node_t *node)
{
	html_node_t	*child, *next_child;
	html_attr_t	*attr, *next_attr;

	if (node == NULL) {
		return;
	}

	child = node->first_child;
	while (child != NULL) {
		next_child = child->next_sibling;
		html_node_free(child);
		child = next_child;
	}

	attr = node->attrs;
	while (attr != NULL) {
		next_attr = attr->next;
		if (attr->name) free(attr->name);
		if (attr->value) free(attr->value);
		free(attr);
		attr = next_attr;
	}

	if (node->tag_name) free(node->tag_name);
	if (node->text) free(node->text);
	free(node);
}

void
html_doc_free(html_doc_t *doc)
{
	if (doc == NULL) {
		return;
	}
	if (doc->root != NULL) {
		html_node_free(doc->root);
	}
	if (doc->title != NULL) {
		free(doc->title);
	}
	if (doc->base_url != NULL) {
		free(doc->base_url);
	}
	free(doc);
}

/*
 * Is `node` inside a PRE (or other preformatted context)?  Whitespace-only
 * text is dropped everywhere else, but inside PRE it is content.
 */
static int
html_in_preformatted(const html_node_t *node)
{
	const html_node_t	*scan;
	int			guard;

	guard = 0;
	for (scan = node; scan != NULL && guard < HTML_MAX_DEPTH;
	    scan = scan->parent, guard++) {
		if (scan->tag == HTML_TAG_PRE ||
		    scan->tag == HTML_TAG_TEXTAREA) {
			return (1);
		}
	}
	return (0);
}

html_doc_t *
html_parse(const char *source, size_t len)
{
	html_doc_t	*doc;
	html_node_t	*current;
	const html_node_t *title_node;
	const char	*p, *end;

	if (source == NULL) {
		return (NULL);
	}
	if (len == 0) {
		len = strlen(source);
	}

	doc = (html_doc_t *)malloc(sizeof(html_doc_t));
	if (doc == NULL) {
		return (NULL);
	}
	memset(doc, 0, sizeof(html_doc_t));

	doc->root = html_node_create(HTML_TAG_HTML, "html");
	if (doc->root == NULL) {
		free(doc);
		return (NULL);
	}
	current = doc->root;

	p = source;
	end = source + len;

	while (p < end) {
		if (*p == '<') {
			/* Comment or DOCTYPE */
			if (p + 3 < end && strncmp(p, "<!--", 4) == 0) {
				p += 4;
				while (p + 2 < end && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) {
					p++;
				}
				if (p + 2 < end) {
					p += 3;
				}
				continue;
			}
			if (p + 8 < end && strncasecmp(p, "<!doctype", 9) == 0) {
				while (p < end && *p != '>') {
					p++;
				}
				if (p < end) {
					p++;
				}
				continue;
			}

			/* Closing tag */
			if (p + 1 < end && p[1] == '/') {
				p += 2;
				const char *tag_start = p;
				while (p < end && *p != '>' && !isspace((unsigned char)*p)) {
					p++;
				}
				size_t tag_len = p - tag_start;
				char tag_name[HTML_TAG_NAME_MAX];
				if (tag_len >= sizeof(tag_name)) {
					tag_len = sizeof(tag_name) - 1;
				}
				memcpy(tag_name, tag_start, tag_len);
				tag_name[tag_len] = '\0';

				while (p < end && *p != '>') {
					p++;
				}
				if (p < end) {
					p++;
				}

				html_tag_t close_tag = html_tag_lookup(tag_name);
				/*
				 * Walk up to the nearest matching open element.
				 * A close tag with no match anywhere (stray
				 * </div>) must be ignored, not allowed to pop
				 * the tree -- bounded by the depth guard.
				 */
				html_node_t *scan = current;
				int guard = 0;
				while (scan != NULL && scan != doc->root &&
				    scan->tag != close_tag &&
				    guard < HTML_MAX_DEPTH) {
					scan = scan->parent;
					guard++;
				}
				if (scan != NULL && scan != doc->root &&
				    scan->tag == close_tag &&
				    scan->parent != NULL) {
					current = scan->parent;
				}
				continue;
			}

			/*
			 * Processing instruction or bogus comment: `<?xml`,
			 * `<!ENTITY`, and friends.  Skip to '>' so their text
			 * never reaches the page.
			 */
			if (p + 1 < end && (p[1] == '?' || p[1] == '!')) {
				while (p < end && *p != '>') {
					p++;
				}
				if (p < end) {
					p++;
				}
				continue;
			}

			/* A '<' not starting a tag name is literal text. */
			if (p + 1 < end && !isalpha((unsigned char)p[1])) {
				const char *lit_start = p;
				p++;
				char *lit = html_decode_entities(lit_start,
				    (size_t)(p - lit_start));
				if (lit != NULL) {
					html_node_t *tn = html_node_create(
					    HTML_TAG_TEXT, "#text");
					if (tn != NULL) {
						tn->text = lit;
						html_node_add_child(current,
						    tn);
					} else {
						free(lit);
					}
				}
				continue;
			}

			/* Opening / void tag */
			p++;
			const char *tag_start = p;
			while (p < end && *p != '>' && *p != '/' && !isspace((unsigned char)*p)) {
				p++;
			}
			size_t tag_len = p - tag_start;
			char tag_name[HTML_TAG_NAME_MAX];
			if (tag_len >= sizeof(tag_name)) {
				tag_len = sizeof(tag_name) - 1;
			}
			memcpy(tag_name, tag_start, tag_len);
			tag_name[tag_len] = '\0';

			html_tag_t tag_type = html_tag_lookup(tag_name);

			/*
			 * Apply optional-end-tag rules before attaching.  Pop
			 * every open element this start tag implicitly closes,
			 * so <p>a<p>b becomes siblings rather than nesting.
			 */
			{
				int guard = 0;
				while (current != NULL &&
				    current != doc->root &&
				    html_auto_closes(current->tag, tag_type) &&
				    current->parent != NULL &&
				    guard < HTML_MAX_DEPTH) {
					current = current->parent;
					guard++;
				}
			}

			html_node_t *elem = html_node_create(tag_type, tag_name);
			if (elem == NULL) {
				/* Out of memory: skip the tag, keep going. */
				while (p < end && *p != '>') {
					p++;
				}
				if (p < end) {
					p++;
				}
				continue;
			}

			/* Parse attributes */
			int is_self_closing = 0;
			while (p < end && *p != '>') {
				while (p < end && isspace((unsigned char)*p)) {
					p++;
				}
				if (p >= end || *p == '>') {
					break;
				}
				if (*p == '/') {
					is_self_closing = 1;
					p++;
					continue;
				}

				const char *attr_name_start = p;
				while (p < end && *p != '=' && *p != '>' && *p != '/' && !isspace((unsigned char)*p)) {
					p++;
				}
				size_t attr_name_len = p - attr_name_start;
				/*
				 * Zero-length name means the cursor did not
				 * move; without this the enclosing loop would
				 * spin on the same byte forever.
				 */
				if (attr_name_len == 0) {
					p++;
					continue;
				}
				char attr_name[HTML_TAG_NAME_MAX];
				if (attr_name_len >= sizeof(attr_name)) {
					attr_name_len = sizeof(attr_name) - 1;
				}
				memcpy(attr_name, attr_name_start, attr_name_len);
				attr_name[attr_name_len] = '\0';

				while (p < end && isspace((unsigned char)*p)) {
					p++;
				}

				char *attr_val = NULL;
				if (p < end && *p == '=') {
					p++;
					while (p < end && isspace((unsigned char)*p)) {
						p++;
					}
					if (p < end && (*p == '"' || *p == '\'')) {
						char quote = *p++;
						const char *val_start = p;
						while (p < end && *p != quote) {
							p++;
						}
						size_t val_len = p - val_start;
						attr_val = html_decode_entities(val_start, val_len);
						if (p < end) {
							p++;
						}
					} else {
						const char *val_start = p;
						while (p < end && *p != '>' && !isspace((unsigned char)*p)) {
							p++;
						}
						size_t val_len = p - val_start;
						attr_val = html_decode_entities(val_start, val_len);
					}
				}

				if (attr_name[0] != '\0') {
					html_attr_add(elem, attr_name, attr_val);
				}
				if (attr_val != NULL) {
					free(attr_val);
				}
			}

			if (p < end && *p == '>') {
				p++;
			}

			html_node_add_child(current, elem);

			if (tag_type == HTML_TAG_BASE && doc->base_url == NULL) {
				const char *href = html_node_get_attr(elem,
				    "href");
				if (href != NULL && href[0] != '\0') {
					doc->base_url = strdup(href);
				}
			}

			if (tag_type == HTML_TAG_SVG) {
				const char	*open_start;
				const char	*scan;
				char		*src;
				size_t		total;

				open_start = tag_start - 1;
				if (!is_self_closing) {
					scan = p;
					while (scan < end) {
						if (scan[0] != '<' ||
						    scan[1] != '/') {
							scan++;
							continue;
						}
						if ((size_t)(end - scan) >=
						    tag_len + 3 &&
						    strncasecmp(scan + 2,
						    tag_name, tag_len) == 0 &&
						    (scan[2 + tag_len] ==
						    '>' ||
						    isspace((unsigned char)
						    scan[2 + tag_len]))) {
							break;
						}
						scan++;
					}
					if (scan < end) {
						p = scan;
						while (p < end && *p != '>') {
							p++;
						}
						if (p < end) {
							p++;
						}
					} else {
						p = end;
					}
				}
				total = (size_t)(p - open_start);
				src = (char *)malloc(total + 1);
				if (src != NULL) {
					memcpy(src, open_start, total);
					src[total] = '\0';
					elem->text = src;
				}
				continue;
			}

			/*
			 * Raw-text elements: consume the body verbatim to the
			 * matching close tag.  This is what keeps CSS and
			 * script source from being laid out as prose -- and
			 * it must scan raw, because a '<' inside a script is
			 * not markup.
			 */
			if (html_is_raw_text(tag_type) && !is_self_closing) {
				char close_str[HTML_TAG_NAME_MAX + 4];
				const char *body_start = p;
				size_t close_len;

				close_str[0] = '<';
				close_str[1] = '/';
				memcpy(close_str + 2, tag_name, tag_len);
				close_str[2 + tag_len] = '>';
				close_str[3 + tag_len] = '\0';
				close_len = tag_len + 3;

				while (p < end) {
					if ((size_t)(end - p) >= close_len &&
					    strncasecmp(p, close_str,
					    close_len) == 0) {
						break;
					}
					p++;
				}
				/*
				 * TEXTAREA content is user-visible text; the
				 * rest (style/script/template/noscript) is
				 * dropped on the floor by design.
				 */
				if (tag_type == HTML_TAG_TEXTAREA &&
				    p > body_start) {
					char *raw = html_decode_entities(
					    body_start,
					    (size_t)(p - body_start));
					if (raw != NULL) {
						html_node_t *tn =
						    html_node_create(
						    HTML_TAG_TEXT, "#text");
						if (tn != NULL) {
							tn->text = raw;
							html_node_add_child(
							    elem, tn);
						} else {
							free(raw);
						}
					}
				}
				if (p < end) {
					p += close_len;
				}
				continue;
			}

			if (!is_self_closing && !html_is_void_tag(tag_type)) {
				/*
				 * Depth cap: the layout walk is recursive, so
				 * a pathologically nested document must not be
				 * allowed to deepen the tree without bound.
				 * Past the cap the element is still parsed and
				 * attached, it just does not become current.
				 */
				int depth = 0;
				html_node_t *scan;

				for (scan = elem; scan != NULL &&
				    depth < HTML_MAX_DEPTH;
				    scan = scan->parent) {
					depth++;
				}
				if (depth < HTML_MAX_DEPTH) {
					current = elem;
				}
			}
		} else {
			/* Text content */
			const char *text_start = p;
			while (p < end && *p != '<') {
				p++;
			}
			size_t text_len = p - text_start;
			char *decoded = html_decode_entities(text_start, text_len);
			if (decoded != NULL) {
				/* Collapse whitespace unless empty */
				int only_spaces = 1;
				for (size_t k = 0; decoded[k] != '\0'; k++) {
					if (!isspace((unsigned char)decoded[k])) {
						only_spaces = 0;
						break;
					}
				}

				/*
				 * Whitespace-only runs are structural noise
				 * everywhere except preformatted content,
				 * where they are the formatting.  Checking
				 * ancestry, not just the parent, is what makes
				 * <pre><code> indentation survive.
				 */
				if (!only_spaces ||
				    html_in_preformatted(current)) {
					html_node_t *text_node = html_node_create(HTML_TAG_TEXT, "#text");
					if (text_node != NULL) {
						text_node->text = decoded;
						html_node_add_child(current, text_node);
					} else {
						free(decoded);
					}
				} else {
					free(decoded);
				}
			}
		}
	}

	/*
	 * Title is read back off the tree rather than captured mid-parse, so
	 * entity decoding and nested markup inside <title> are handled by the
	 * same path as everything else.
	 */
	if (doc->title == NULL) {
		title_node = html_node_find(doc->root, HTML_TAG_TITLE);
		if (title_node != NULL) {
			doc->title = html_node_text(title_node);
		}
	}

	return (doc);
}
