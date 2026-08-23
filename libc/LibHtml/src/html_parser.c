/* !DEFINES!

$define %type html_parser as internal parser state
$define %func html_parse as function with args const char *, size_t
$define %func html_doc_free as procedure with args html_doc *

*/

/* !SPACE!

$space %internal html_decode_entities, html_tag_lookup, html_is_void_tag
$space %internal html_node_create, html_node_add_child, html_attr_add
$space %export html_parse, html_doc_free, html_node_get_attr

*/

#include <ctype.h>
#include <html.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
		return (1);
	default:
		return (0);
	}
}

static char *
html_decode_entities(const char *str, size_t len)
{
	char	*out;
	size_t	i, j;

	out = (char *)malloc(len + 1);
	if (out == NULL) {
		return (NULL);
	}

	i = 0;
	j = 0;
	while (i < len) {
		if (str[i] == '&') {
			if (i + 5 <= len && strncmp(str + i, "&nbsp;", 6) == 0) {
				out[j++] = ' ';
				i += 6;
				continue;
			}
			if (i + 4 <= len && strncmp(str + i, "&amp;", 5) == 0) {
				out[j++] = '&';
				i += 5;
				continue;
			}
			if (i + 3 <= len && strncmp(str + i, "&lt;", 4) == 0) {
				out[j++] = '<';
				i += 4;
				continue;
			}
			if (i + 3 <= len && strncmp(str + i, "&gt;", 4) == 0) {
				out[j++] = '>';
				i += 4;
				continue;
			}
			if (i + 5 <= len && strncmp(str + i, "&quot;", 6) == 0) {
				out[j++] = '"';
				i += 6;
				continue;
			}
			if (i + 5 <= len && strncmp(str + i, "&apos;", 6) == 0) {
				out[j++] = '\'';
				i += 6;
				continue;
			}
			if (i + 2 < len && str[i + 1] == '#') {
				size_t k = i + 2;
				long code = 0;
				int is_hex = 0;
				if (k < len && (str[k] == 'x' || str[k] == 'X')) {
					is_hex = 1;
					k++;
				}
				while (k < len && str[k] != ';') {
					if (is_hex && isxdigit((unsigned char)str[k])) {
						code = code * 16 + (isdigit((unsigned char)str[k]) ?
						    str[k] - '0' : tolower((unsigned char)str[k]) - 'a' + 10);
					} else if (!is_hex && isdigit((unsigned char)str[k])) {
						code = code * 10 + (str[k] - '0');
					} else {
						break;
					}
					k++;
				}
				if (k < len && str[k] == ';') {
					if (code > 0 && code < 256) {
						out[j++] = (char)code;
					} else {
						out[j++] = '?';
					}
					i = k + 1;
					continue;
				}
			}
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

html_doc_t *
html_parse(const char *source, size_t len)
{
	html_doc_t	*doc;
	html_node_t	*current;
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
				char tag_name[64];
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
				html_node_t *scan = current;
				while (scan != NULL && scan != doc->root && scan->tag != close_tag) {
					scan = scan->parent;
				}
				if (scan != NULL && scan != doc->root && scan->tag == close_tag) {
					current = scan->parent;
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
			char tag_name[64];
			if (tag_len >= sizeof(tag_name)) {
				tag_len = sizeof(tag_name) - 1;
			}
			memcpy(tag_name, tag_start, tag_len);
			tag_name[tag_len] = '\0';

			html_tag_t tag_type = html_tag_lookup(tag_name);
			html_node_t *elem = html_node_create(tag_type, tag_name);

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
				char attr_name[64];
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

			/* Special tags handling */
			if (tag_type == HTML_TAG_STYLE || tag_type == HTML_TAG_SCRIPT) {
				/* Skip content until closing tag */
				const char *close_str = (tag_type == HTML_TAG_STYLE) ? "</style>" : "</script>";
				size_t close_len = strlen(close_str);
				while (p + close_len <= end) {
					if (strncasecmp(p, close_str, close_len) == 0) {
						p += close_len;
						break;
					}
					p++;
				}
				continue;
			}

			if (tag_type == HTML_TAG_TITLE) {
				const char *title_start = p;
				while (p + 8 <= end && strncasecmp(p, "</title>", 8) != 0 && *p != '<') {
					p++;
				}
				size_t title_len = p - title_start;
				doc->title = html_decode_entities(title_start, title_len);
			}

			if (!is_self_closing && !html_is_void_tag(tag_type)) {
				current = elem;
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

				if (!only_spaces || current->tag == HTML_TAG_PRE || current->tag == HTML_TAG_CODE) {
					html_node_t *text_node = html_node_create(HTML_TAG_TEXT, "#text");
					text_node->text = decoded;
					html_node_add_child(current, text_node);
				} else {
					free(decoded);
				}
			}
		}
	}

	return (doc);
}
