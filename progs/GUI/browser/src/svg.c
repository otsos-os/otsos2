/* !DEFINES!

$define %type browser_svg_entry as cached parsed SVG document keyed by source
$define %func browser_svg_cache_free as procedure with args browser_state *
$define %func browser_svg_draw as function with args void *, libg_context *, libg_rect_t, const char *

*/

/* !SPACE!

$space %internal browser_svg_lookup
$space %export browser_svg_cache_free, browser_svg_draw

*/

#include <browser.h>
#include <libg.h>
#include <stdlib.h>
#include <string.h>

static svg_doc_t *
browser_svg_lookup(browser_state_t *st, const char *ref)
{
	svg_doc_t	*doc;
	int		i;

	for (i = 0; i < st->svg_count; i++) {
		if (strcmp(st->svg_cache[i].ref, ref) == 0) {
			return (st->svg_cache[i].doc);
		}
	}

	doc = NULL;
	(void)svg_parse(ref, 0, &doc);

	if (st->svg_count < BROWSER_SVG_CACHE_MAX) {
		st->svg_cache[st->svg_count].ref = strdup(ref);
		st->svg_cache[st->svg_count].doc = doc;
		st->svg_count++;
		return (doc);
	}
	free(st->svg_cache[0].ref);
	memmove(&st->svg_cache[0], &st->svg_cache[1],
	    sizeof(browser_svg_entry_t) * (BROWSER_SVG_CACHE_MAX - 1));
	st->svg_cache[BROWSER_SVG_CACHE_MAX - 1].ref = strdup(ref);
	st->svg_cache[BROWSER_SVG_CACHE_MAX - 1].doc = doc;
	return (doc);
}

void
browser_svg_cache_free(browser_state_t *st)
{
	int	i;

	if (st == NULL) {
		return;
	}
	for (i = 0; i < st->svg_count; i++) {
		free(st->svg_cache[i].ref);
		svg_doc_free(st->svg_cache[i].doc);
	}
	st->svg_count = 0;
}

int
browser_svg_draw(void *userdata, libg_context_t *ctx, libg_rect_t rect,
    const char *ref)
{
	browser_state_t	*st;
	svg_doc_t	*doc;

	st = (browser_state_t *)userdata;
	if (st == NULL || ctx == NULL || ref == NULL || ref[0] != '<') {
		return (-1);
	}

	doc = browser_svg_lookup(st, ref);
	if (doc == NULL) {
		return (-1);
	}

	libgSvgDoc(ctx, doc, rect);
	return (0);
}
