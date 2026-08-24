/* !DEFINES!

$define %type browser_ui as UI layout and rendering
$define %func browser_draw as procedure with args browser_state *
$define %func browser_handle_event as procedure with args browser_state *, const sprot_event *
$define %func browser_navigate as function with args browser_state *, const char *
$define %func browser_load_poll as procedure with args browser_state *

*/

/* !SPACE!

$space %internal draw_top_bar, draw_toolbar, draw_viewport, draw_status_bar, draw_scrollbar
$space %internal translate_key_to_char, get_header_widget_id
$space %internal browser_load_apply, browser_load_progress, browser_load_drop
$space %internal browser_count_images
$space %export browser_ui_init, browser_draw, browser_handle_event
$space %export browser_navigate, browser_status_set, browser_load_poll
$space %export browser_history_push, browser_history_back, browser_history_forward

*/

#include <browser.h>
#include <ctype.h>
#include <html.h>
#include <libg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOP_BAR_H 32
#define TOOLBAR_H 36
#define STATUS_BAR_H 24
#define HEADER_TOTAL_H (TOP_BAR_H + TOOLBAR_H)
#define SCROLLBAR_W 16

#define ID_LOCATION 101
#define ID_GO 102
#define ID_STOP_ICON 103
#define ID_BACK 104
#define ID_FORW 105
#define ID_HOME 106
#define ID_RELOAD 107
#define ID_STOP 108
#define ID_SCROLL_UP 109
#define ID_SCROLL_DOWN 110

void
browser_status_set(browser_state_t *st, const char *msg)
{
	if (st == NULL || msg == NULL) {
		return;
	}
	/*
	 * Compared before storing: the loader re-derives its status text on every
	 * step, and marking the bar dirty each time would repaint the window as
	 * fast as the event loop spins for the whole duration of a load.
	 */
	if (strcmp(st->status_msg, msg) == 0) {
		return;
	}
	strncpy(st->status_msg, msg, sizeof(st->status_msg) - 1);
	st->status_msg[sizeof(st->status_msg) - 1] = '\0';
	st->dirty_flags |= BROWSER_DIRTY_STATUS;
}

void
browser_history_push(browser_state_t *st, const char *url, const char *title)
{
	if (st == NULL || url == NULL) {
		return;
	}
	/*
	 * Reload and back/forward re-enter here with the URL already at the
	 * cursor.  Pushing it again would truncate the forward list and make Back
	 * appear to do nothing, so the entry is only refreshed in place.
	 */
	if (st->history_pos >= 0 && st->history_pos < st->history_count &&
	    strcmp(st->history[st->history_pos].url, url) == 0) {
		if (title != NULL) {
			strncpy(st->history[st->history_pos].title, title,
			    sizeof(st->history[0].title) - 1);
			st->history[st->history_pos].title[
			    sizeof(st->history[0].title) - 1] = '\0';
		}
		return;
	}
	if (st->history_pos + 1 < BROWSER_HISTORY_MAX) {
		st->history_pos++;
	} else {
		for (int i = 0; i < BROWSER_HISTORY_MAX - 1; i++) {
			st->history[i] = st->history[i + 1];
		}
		st->history_pos = BROWSER_HISTORY_MAX - 1;
	}
	strncpy(st->history[st->history_pos].url, url, BROWSER_MAX_URL - 1);
	st->history[st->history_pos].url[BROWSER_MAX_URL - 1] = '\0';
	if (title != NULL) {
		strncpy(st->history[st->history_pos].title, title,
		    sizeof(st->history[0].title) - 1);
	} else {
		st->history[st->history_pos].title[0] = '\0';
	}
	/*
	 * strncpy leaves no terminator when the title fills the field, and the
	 * shift above copies entries by value, so the truncated tail would travel
	 * with the entry and later be printed as the window title.
	 */
	st->history[st->history_pos].title[sizeof(st->history[0].title) - 1] = '\0';
	st->history_count = st->history_pos + 1;
}

void
browser_history_back(browser_state_t *st)
{
	if (st == NULL || st->history_pos <= 0) {
		return;
	}
	st->history_pos--;
	browser_navigate(st, st->history[st->history_pos].url);
}

void
browser_history_forward(browser_state_t *st)
{
	if (st == NULL || st->history_pos + 1 >= st->history_count) {
		return;
	}
	st->history_pos++;
	browser_navigate(st, st->history[st->history_pos].url);
}

/*
 * Queues a navigation instead of performing it.  Every caller reaches this from
 * inside browser_draw() - a link click, a toolbar button, a submitted URL - and
 * fetching there is what froze the window for the whole duration of a load.  The
 * main loop picks the request up after the frame is presented.
 */
int
browser_navigate(browser_state_t *st, const char *url)
{
	char	full_url[BROWSER_MAX_URL];
	char	status[BROWSER_MAX_URL + 64];

	if (st == NULL || url == NULL || url[0] == '\0') {
		return (-1);
	}

	/*
	 * Resolved against the current page here rather than in the loader: by
	 * the time the queued request runs, a redirect may already have moved
	 * current_url, and a relative href must bind to the page it came from.
	 */
	if (st->current_url[0] != '\0' &&
	    strncasecmp(url, "http://", 7) != 0 &&
	    strncasecmp(url, "https://", 8) != 0) {
		browser_url_resolve(st->current_url, url, full_url,
		    sizeof(full_url));
	} else {
		browser_url_normalize(url, full_url, sizeof(full_url));
	}
	if (full_url[0] == '\0') {
		return (-1);
	}

	strncpy(st->pending_url, full_url, sizeof(st->pending_url) - 1);
	st->pending_url[sizeof(st->pending_url) - 1] = '\0';
	st->pending_nav = 1;

	snprintf(status, sizeof(status), "Looking up %s...", full_url);
	browser_status_set(st, status);
	return (0);
}

/*
 * Iterative rather than recursive, with a node ceiling: a deeply nested document
 * would otherwise put an unbounded frame count on the stack just to fill in a
 * counter in the status area.
 */
static int
browser_count_images(const html_node_t *root)
{
	const html_node_t	*node;
	int			count, visited;

	count = 0;
	visited = 0;
	node = root;
	while (node != NULL && visited < BROWSER_MAX_NODES) {
		visited++;
		if (node->tag == HTML_TAG_IMG) {
			count++;
		}
		if (node->first_child != NULL) {
			node = node->first_child;
			continue;
		}
		while (node != NULL && node->next_sibling == NULL) {
			node = node->parent;
			if (node == root) {
				return (count);
			}
		}
		if (node == NULL) {
			break;
		}
		node = node->next_sibling;
	}
	return (count);
}

static void
browser_load_drop(browser_state_t *st)
{
	if (st->layout != NULL) {
		html_layout_free(st->layout);
		st->layout = NULL;
	}
	if (st->doc != NULL) {
		html_doc_free(st->doc);
		st->doc = NULL;
	}
	free(st->raw_html);
	st->raw_html = NULL;
	st->raw_html_len = 0;
}

/*
 * Takes ownership of the loader's response buffer and turns it into the shown
 * document.  The body is moved to the front of that same allocation rather than
 * copied out: pages run to megabytes and a second buffer would double the peak.
 */
static void
browser_load_apply(browser_state_t *st)
{
	browser_loader_t	*ld;
	char			win_title[256];
	char			status[128];
	const char		*title;
	size_t			off, body_len;
	int32_t			viewport_w, view_h;

	ld = &st->loader;
	off = browser_http_body_offset(ld->response, ld->response_len);
	if (off > ld->response_len) {
		off = ld->response_len;
	}
	body_len = ld->response_len - off;

	browser_load_drop(st);

	if (off != 0) {
		memmove(ld->response, ld->response + off, body_len);
	}
	ld->response[body_len] = '\0';

	st->raw_html = ld->response;
	st->raw_html_len = body_len;
	st->page_size_bytes = body_len;

	/* Ownership moved; cleared so browser_loader_reset does not free it. */
	ld->response = NULL;
	ld->response_len = 0;
	ld->response_cap = 0;

	strncpy(st->current_url, ld->url, sizeof(st->current_url) - 1);
	st->current_url[sizeof(st->current_url) - 1] = '\0';
	strncpy(st->input_url, ld->url, sizeof(st->input_url) - 1);
	st->input_url[sizeof(st->input_url) - 1] = '\0';

	st->doc = html_parse(st->raw_html, st->raw_html_len);
	st->images_count = (st->doc != NULL) ?
	    browser_count_images(st->doc->root) : 0;
	st->images_loaded = 0;
	viewport_w = (int32_t)st->width - SCROLLBAR_W;
	if (viewport_w < 100) {
		viewport_w = 600;
	}
	st->layout = html_layout_create(st->doc, viewport_w);

	st->scroll_y = 0;
	view_h = (int32_t)st->height - HEADER_TOTAL_H - STATUS_BAR_H;
	if (st->layout != NULL && st->layout->content_height > view_h) {
		st->max_scroll_y = st->layout->content_height - view_h;
	} else {
		st->max_scroll_y = 0;
	}

	title = NULL;
	if (st->doc != NULL && st->doc->title != NULL &&
	    st->doc->title[0] != '\0') {
		title = st->doc->title;
	}
	snprintf(win_title, sizeof(win_title), "Dillo: %s",
	    (title != NULL) ? title : st->current_url);
	browser_history_push(st, st->current_url,
	    (title != NULL) ? title : st->current_url);
	if (st->surface != NULL) {
		sprot_set_title(st->surface, win_title);
	}

	snprintf(status, sizeof(status), "Loaded %u bytes",
	    (unsigned int)body_len);
	browser_status_set(st, status);
	st->dirty_flags = BROWSER_DIRTY_ALL;
}

/*
 * Progress text for an in-flight load.  Byte counts are reported in KB so the
 * string only changes about once per kilobyte; formatting the raw count would
 * mark the status bar dirty on every single packet.
 */
static void
browser_load_progress(browser_state_t *st)
{
	const browser_loader_t	*ld;
	char			status[BROWSER_MAX_HOST + 64];
	size_t			kb;

	ld = &st->loader;
	kb = ld->response_len / 1024u;
	if (kb != st->progress_kb) {
		st->progress_kb = kb;
		st->dirty_flags |= BROWSER_DIRTY_HEADER;
	}

	switch (ld->state) {
	case BROWSER_LOAD_DNS:
		snprintf(status, sizeof(status), "Looking up %s...", ld->host);
		break;
	case BROWSER_LOAD_CONNECT:
		snprintf(status, sizeof(status), "Connecting to %s:%d...",
		    ld->host, ld->port);
		break;
	case BROWSER_LOAD_SEND:
		snprintf(status, sizeof(status), "Requesting %s...", ld->path);
		break;
	case BROWSER_LOAD_RECV:
		snprintf(status, sizeof(status), "Receiving from %s (%u KB)...",
		    ld->host, (unsigned int)(ld->response_len / 1024u));
		break;
	default:
		return;
	}
	browser_status_set(st, status);
}

/*
 * Called once per pass of the main loop: starts whatever navigation the draw
 * pass queued, advances the in-flight load, and commits it when it finishes.
 */
void
browser_load_poll(browser_state_t *st)
{
	browser_loader_t	*ld;
	char			status[BROWSER_MAX_URL + 64];
	char			url[BROWSER_MAX_URL];

	if (st == NULL) {
		return;
	}
	ld = &st->loader;

	if (st->pending_nav) {
		st->pending_nav = 0;
		/*
		 * Copied out first: the loader's own redirect handling writes
		 * through browser_loader_start, and pending_url may be reused by
		 * a click that lands while this load is running.
		 */
		strncpy(url, st->pending_url, sizeof(url) - 1);
		url[sizeof(url) - 1] = '\0';
		st->pending_url[0] = '\0';

		browser_loader_reset(ld);
		st->is_loading = 1;
		st->page_size_bytes = 0;
		st->progress_kb = 0;
		st->dirty_flags |= BROWSER_DIRTY_HEADER;
		(void)browser_loader_start(ld, url);
	}

	if (ld->state == BROWSER_LOAD_IDLE) {
		return;
	}

	(void)browser_loader_step(ld);

	switch (ld->state) {
	case BROWSER_LOAD_DONE:
		st->is_loading = 0;
		browser_load_apply(st);
		browser_loader_reset(ld);
		break;
	case BROWSER_LOAD_ERROR:
		st->is_loading = 0;
		snprintf(status, sizeof(status), "%s: %s",
		    (ld->error[0] != '\0') ? ld->error : "Load failed",
		    ld->url);
		browser_status_set(st, status);
		browser_loader_reset(ld);
		break;
	default:
		browser_load_progress(st);
		break;
	}
}

void
browser_ui_init(browser_state_t *st)
{
	if (st == NULL) {
		return;
	}
	strncpy(st->input_url, BROWSER_DEFAULT_URL, sizeof(st->input_url) - 1);
	st->input_url[sizeof(st->input_url) - 1] = '\0';
	strncpy(st->status_msg, "Ready", sizeof(st->status_msg) - 1);
	st->status_msg[sizeof(st->status_msg) - 1] = '\0';
	st->hover_href[0] = '\0';
	st->scroll_y = 0;
	st->max_scroll_y = 0;
	st->images_count = 0;
	st->images_loaded = 0;
	st->page_size_bytes = 0;
	st->progress_kb = 0;
	st->history_count = 0;
	st->history_pos = -1;
	st->pending_url[0] = '\0';
	st->pending_nav = 0;
	st->dirty_flags = BROWSER_DIRTY_ALL;
	st->last_hot_id = 0;
}

static void
draw_top_bar(browser_state_t *st)
{
	libg_rect_t rect;
	libg_style_t style;

	libgGetStyle(st->ui, &style);

	rect.x = 0;
	rect.y = 0;
	rect.width = (int32_t)st->width;
	rect.height = TOP_BAR_H;
	libgFillRect(st->ui, rect, 0xFFE0DDD9);
	libgLine(st->ui, 0, TOP_BAR_H - 1, (int32_t)st->width, TOP_BAR_H - 1, 0xFFB0ACA6);

	/* Red stop/clear box */
	rect.x = 6;
	rect.y = 5;
	rect.width = 22;
	rect.height = 22;
	if (libgButton(st->ui, ID_STOP_ICON, rect, "X") & LIBG_WIDGET_CLICKED) {
		st->input_url[0] = '\0';
		st->dirty_flags |= BROWSER_DIRTY_HEADER;
	}

	/* Location text field (soft green background Dillo-style) */
	rect.x = 34;
	rect.y = 5;
	rect.width = (int32_t)st->width - 34 - 58;
	rect.height = 22;

	libg_style_t field_style = style;
	field_style.field = 0xFFD4EED8;
	field_style.field_focus = 0xFFC6E8CB;
	libgSetStyle(st->ui, &field_style);

	uint32_t tf_res = libgTextField(st->ui, ID_LOCATION, rect, st->input_url, sizeof(st->input_url));
	if (tf_res & LIBG_WIDGET_SUBMIT) {
		browser_navigate(st, st->input_url);
	} else if (tf_res & LIBG_WIDGET_CHANGED) {
		st->dirty_flags |= BROWSER_DIRTY_HEADER;
	}

	libgSetStyle(st->ui, &style);

	/* Go button */
	rect.x = (int32_t)st->width - 50;
	rect.y = 5;
	rect.width = 44;
	rect.height = 22;
	if (libgButton(st->ui, ID_GO, rect, "Go") & LIBG_WIDGET_CLICKED) {
		browser_navigate(st, st->input_url);
	}
}

static void
draw_toolbar(browser_state_t *st)
{
	libg_rect_t rect;
	int32_t x = 6;
	int32_t y = TOP_BAR_H + 4;
	int32_t btn_h = 28;

	rect.x = 0;
	rect.y = TOP_BAR_H;
	rect.width = (int32_t)st->width;
	rect.height = TOOLBAR_H;
	libgFillRect(st->ui, rect, 0xFFEDEAE6);
	libgLine(st->ui, 0, HEADER_TOTAL_H - 1, (int32_t)st->width, HEADER_TOTAL_H - 1, 0xFFB0ACA6);

	/* Back */
	rect.x = x; rect.y = y; rect.width = 54; rect.height = btn_h;
	if (libgButton(st->ui, ID_BACK, rect, "< Back") & LIBG_WIDGET_CLICKED) {
		browser_history_back(st);
	}
	x += 58;

	/* Forward */
	rect.x = x; rect.y = y; rect.width = 54; rect.height = btn_h;
	if (libgButton(st->ui, ID_FORW, rect, "Forw >") & LIBG_WIDGET_CLICKED) {
		browser_history_forward(st);
	}
	x += 58;

	/* Home */
	rect.x = x; rect.y = y; rect.width = 54; rect.height = btn_h;
	if (libgButton(st->ui, ID_HOME, rect, "Home") & LIBG_WIDGET_CLICKED) {
		browser_navigate(st, BROWSER_DEFAULT_URL);
	}
	x += 58;

	/* Reload */
	rect.x = x; rect.y = y; rect.width = 62; rect.height = btn_h;
	if (libgButton(st->ui, ID_RELOAD, rect, "Reload") & LIBG_WIDGET_CLICKED) {
		browser_navigate(st, st->current_url);
	}
	x += 66;

	/* Stop */
	rect.x = x; rect.y = y; rect.width = 54; rect.height = btn_h;
	if (libgButton(st->ui, ID_STOP, rect, "Stop") & LIBG_WIDGET_CLICKED) {
		/*
		 * Also clears a queued navigation: a click that arrives in the
		 * same frame as Stop would otherwise start the load Stop meant
		 * to cancel.
		 */
		st->pending_nav = 0;
		st->pending_url[0] = '\0';
		browser_loader_abort(&st->loader);
		st->is_loading = 0;
		browser_status_set(st, "Stopped");
	}

	/* Stats box */
	rect.x = (int32_t)st->width - 150;
	rect.y = TOP_BAR_H + 4;
	rect.width = 144;
	rect.height = 28;
	libgStrokeRect(st->ui, rect, 0xFF9E9A94);

	libg_rect_t box1 = { rect.x + 1, rect.y + 1, 70, 26 };
	libg_rect_t box2 = { rect.x + 72, rect.y + 1, 71, 26 };
	libgFillRect(st->ui, box1, 0xFFE0DDD9);
	libgFillRect(st->ui, box2, 0xFFE0DDD9);
	libgLine(st->ui, rect.x + 71, rect.y, rect.x + 71, rect.y + 28, 0xFF9E9A94);

	char img_str[32];
	snprintf(img_str, sizeof(img_str), "%d of %d", st->images_loaded,
	    st->images_count);
	libgTextScale(st->ui, box1.x + 6, box1.y + 2, "Images", 0xFF666666, 1);
	libgTextScale(st->ui, box1.x + 10, box1.y + 13, img_str, 0xFF333333, 1);

	/*
	 * While a load runs this shows what has arrived so far, which is the only
	 * visible sign that a slow transfer is still making progress.
	 */
	char page_str[32];
	size_t shown = st->is_loading ? st->loader.response_len :
	    st->page_size_bytes;
	snprintf(page_str, sizeof(page_str), "%u.%u KB",
	    (unsigned int)(shown / 1024u),
	    (unsigned int)((shown % 1024u) * 10u / 1024u));
	libgTextScale(st->ui, box2.x + 14, box2.y + 2, "Page", 0xFF666666, 1);
	libgTextScale(st->ui, box2.x + 6, box2.y + 13, page_str, 0xFF333333, 1);
}

static void
draw_scrollbar(browser_state_t *st, int32_t vx, int32_t vy, int32_t vw, int32_t vh)
{
	(void)vx;
	(void)vw;
	libg_rect_t rect;
	int32_t sb_x = (int32_t)st->width - SCROLLBAR_W;
	int32_t sb_y = vy;
	int32_t sb_h = vh;

	/* Track */
	rect.x = sb_x;
	rect.y = sb_y;
	rect.width = SCROLLBAR_W;
	rect.height = sb_h;
	libgFillRect(st->ui, rect, 0xFFEBE8E4);
	libgLine(st->ui, sb_x, sb_y, sb_x, sb_y + sb_h, 0xFFB0ACA6);

	/* Up button */
	rect.height = 16;
	if (libgButton(st->ui, ID_SCROLL_UP, rect, "^") & LIBG_WIDGET_CLICKED) {
		st->scroll_y -= 40;
		if (st->scroll_y < 0) st->scroll_y = 0;
		st->dirty_flags |= BROWSER_DIRTY_VIEWPORT;
	}

	/* Down button */
	rect.y = sb_y + sb_h - 16;
	rect.height = 16;
	if (libgButton(st->ui, ID_SCROLL_DOWN, rect, "v") & LIBG_WIDGET_CLICKED) {
		st->scroll_y += 40;
		if (st->scroll_y > st->max_scroll_y) st->scroll_y = st->max_scroll_y;
		st->dirty_flags |= BROWSER_DIRTY_VIEWPORT;
	}

	/* Thumb */
	int32_t track_h = sb_h - 32;
	if (track_h > 20 && st->layout != NULL && st->layout->content_height > 0) {
		int32_t thumb_h = (track_h * vh) / st->layout->content_height;
		if (thumb_h < 16) thumb_h = 16;
		if (thumb_h > track_h) thumb_h = track_h;

		int32_t thumb_y = sb_y + 16;
		if (st->max_scroll_y > 0) {
			thumb_y += (st->scroll_y * (track_h - thumb_h)) / st->max_scroll_y;
		}

		libg_rect_t thumb = { sb_x + 1, thumb_y, SCROLLBAR_W - 2, thumb_h };
		libgFillRect(st->ui, thumb, 0xFFC8C4BE);
		libgStrokeRect(st->ui, thumb, 0xFF8A8680);
	}
}

static void
draw_viewport(browser_state_t *st)
{
	libg_rect_t rect;
	int32_t vx = 0;
	int32_t vy = HEADER_TOTAL_H;
	int32_t vw = (int32_t)st->width - SCROLLBAR_W;
	int32_t vh = (int32_t)st->height - HEADER_TOTAL_H - STATUS_BAR_H;

	rect.x = vx;
	rect.y = vy;
	rect.width = vw;
	rect.height = vh;

	libgSetClip(st->ui, rect);
	{
		uint32_t	bg = 0xFFFFFFFFu;

		if (st->layout != NULL && st->layout->has_page_bg != 0) {
			bg = st->layout->page_bg;
		}
		libgFillRect(st->ui, rect, bg);
	}

	if (st->layout != NULL) {
		html_layout_render(st->ui, st->layout, vx, vy, vw, vh,
		    st->scroll_y, browser_svg_draw, st);
	}
	libgClearClip(st->ui);

	draw_scrollbar(st, vx, vy, vw, vh);
}

static void
draw_status_bar(browser_state_t *st)
{
	libg_rect_t rect;
	int32_t y = (int32_t)st->height - STATUS_BAR_H;

	rect.x = 0;
	rect.y = y;
	rect.width = (int32_t)st->width;
	rect.height = STATUS_BAR_H;
	libgFillRect(st->ui, rect, 0xFFE0DDD9);
	libgLine(st->ui, 0, y, (int32_t)st->width, y, 0xFFB0ACA6);

	if (st->hover_href[0] != '\0') {
		char link_msg[300];
		snprintf(link_msg, sizeof(link_msg), "Link to: %s", st->hover_href);
		libgTextScale(st->ui, 8, y + 5, link_msg, 0xFF003399, 2);
	} else {
		libgTextScale(st->ui, 8, y + 5, st->status_msg, 0xFF333333, 2);
	}

	libgLine(st->ui, (int32_t)st->width - 12, (int32_t)st->height - 4, (int32_t)st->width - 4, (int32_t)st->height - 12, 0xFF888888);
	libgLine(st->ui, (int32_t)st->width - 8, (int32_t)st->height - 4, (int32_t)st->width - 4, (int32_t)st->height - 8, 0xFF888888);
	libgLine(st->ui, (int32_t)st->width - 4, (int32_t)st->height - 4, (int32_t)st->width - 4, (int32_t)st->height - 4, 0xFF888888);
}

void
browser_draw(browser_state_t *st)
{
	if (st == NULL || st->ui == NULL || st->dirty_flags == 0) {
		return;
	}

	libgBeginOverlay(st->ui);

	draw_top_bar(st);
	draw_toolbar(st);
	draw_viewport(st);
	draw_status_bar(st);

	libgPresent(st->ui);
	st->dirty_flags = 0;
}

static uint32_t
get_header_widget_id(const browser_state_t *st, int32_t x, int32_t y)
{
	if (y < 0 || y >= HEADER_TOTAL_H || x < 0 || x >= (int32_t)st->width) {
		return (0);
	}
	if (y < TOP_BAR_H) {
		if (x >= 6 && x <= 28 && y >= 5 && y <= 27) return (ID_STOP_ICON);
		if (x >= 34 && x <= (int32_t)st->width - 58 && y >= 5 && y <= 27) return (ID_LOCATION);
		if (x >= (int32_t)st->width - 50 && x <= (int32_t)st->width - 6 && y >= 5 && y <= 27) return (ID_GO);
		return (0);
	}
	int32_t bx = 6;
	if (x >= bx && x <= bx + 54 && y >= 36 && y <= 64) return (ID_BACK);
	bx += 58;
	if (x >= bx && x <= bx + 54 && y >= 36 && y <= 64) return (ID_FORW);
	bx += 58;
	if (x >= bx && x <= bx + 54 && y >= 36 && y <= 64) return (ID_HOME);
	bx += 58;
	if (x >= bx && x <= bx + 62 && y >= 36 && y <= 64) return (ID_RELOAD);
	bx += 66;
	if (x >= bx && x <= bx + 54 && y >= 36 && y <= 64) return (ID_STOP);
	return (0);
}

static uint32_t
translate_key_to_char(uint32_t scancode, uint32_t mods)
{
	int shift = (mods & 0x03) != 0;
	int caps = (mods & 0x04) != 0;

	if (scancode >= 0x04 && scancode <= 0x1d) {
		return (shift ^ caps) ? ('A' + (scancode - 0x04)) : ('a' + (scancode - 0x04));
	}
	if (scancode >= 0x59 && scancode <= 0x61) {
		return ('1' + (scancode - 0x59));
	}
	if (scancode == 0x62) {
		return ('0');
	}
	switch (scancode) {
	case 0x1e: return shift ? '!' : '1';
	case 0x1f: return shift ? '@' : '2';
	case 0x20: return shift ? '#' : '3';
	case 0x21: return shift ? '$' : '4';
	case 0x22: return shift ? '%' : '5';
	case 0x23: return shift ? '^' : '6';
	case 0x24: return shift ? '&' : '7';
	case 0x25: return shift ? '*' : '8';
	case 0x26: return shift ? '(' : '9';
	case 0x27: return shift ? ')' : '0';
	case 0x28:
	case 0x58: return '\n';
	case 0x2a: return '\b';
	case 0x2c: return ' ';
	case 0x2d: return shift ? '_' : '-';
	case 0x2e: return shift ? '+' : '=';
	case 0x2f: return shift ? '{' : '[';
	case 0x30: return shift ? '}' : ']';
	case 0x31: return shift ? '|' : '\\';
	case 0x33: return shift ? ':' : ';';
	case 0x34: return shift ? '"' : '\'';
	case 0x35: return shift ? '~' : '`';
	case 0x36: return shift ? '<' : ',';
	case 0x37: return shift ? '>' : '.';
	case 0x38: return shift ? '?' : '/';
	case 0x54: return '/';
	case 0x55: return '*';
	case 0x56: return '-';
	case 0x57: return '+';
	case 0x63: return '.';
	default: return 0;
	}
}

void
browser_handle_event(browser_state_t *st, const sprot_event_t *event)
{
	if (st == NULL || event == NULL) {
		return;
	}

	if (event->kind == SPROT_EVENT_SURFACE_CLOSE || event->kind == SPROT_EVENT_DISCONNECT) {
		st->running = 0;
		return;
	}

	if (event->kind == SPROT_EVENT_POINTER_MOTION || event->kind == SPROT_EVENT_POINTER_ENTER) {
		int32_t mx = event->u.pointer_motion.x;
		int32_t my = event->u.pointer_motion.y;
		st->mouse_x = mx;
		st->mouse_y = my;

		struct srapi_input_event srapi_ev;
		memset(&srapi_ev, 0, sizeof(srapi_ev));
		srapi_ev.type = SRAPI_INPUT_MOUSE;
		srapi_ev.flags = SRAPI_MOUSE_MOVE | SRAPI_MOUSE_ABSOLUTE;
		srapi_ev.x = mx;
		srapi_ev.y = my;
		libgHandleInput(st->ui, &srapi_ev);

		int32_t vy = HEADER_TOTAL_H;
		int32_t vh = (int32_t)st->height - HEADER_TOTAL_H - STATUS_BAR_H;
		if (my >= vy && my < vy + vh && mx < (int32_t)st->width - SCROLLBAR_W) {
			int32_t doc_x = mx;
			int32_t doc_y = my - vy + st->scroll_y;
			const char *link = html_layout_hit_test(st->layout, doc_x, doc_y);
			if (link != NULL) {
				if (strcmp(st->hover_href, link) != 0) {
					strncpy(st->hover_href, link, sizeof(st->hover_href) - 1);
					st->hover_href[sizeof(st->hover_href) - 1] = '\0';
					st->dirty_flags |= BROWSER_DIRTY_STATUS;
				}
			} else if (st->hover_href[0] != '\0') {
				st->hover_href[0] = '\0';
				st->dirty_flags |= BROWSER_DIRTY_STATUS;
			}
		} else {
			if (st->hover_href[0] != '\0') {
				st->hover_href[0] = '\0';
				st->dirty_flags |= BROWSER_DIRTY_STATUS;
			}
			uint32_t wid = get_header_widget_id(st, mx, my);
			if (wid != st->last_hot_id) {
				st->last_hot_id = wid;
				st->dirty_flags |= BROWSER_DIRTY_HEADER;
			}
		}
		return;
	}

	if (event->kind == SPROT_EVENT_POINTER_BUTTON) {
		int32_t mx = st->mouse_x;
		int32_t my = st->mouse_y;
		int pressed = (event->u.pointer_button.state == SPROT_BUTTON_STATE_PRESSED);

		struct srapi_input_event srapi_ev;
		memset(&srapi_ev, 0, sizeof(srapi_ev));
		srapi_ev.type = SRAPI_INPUT_MOUSE;
		srapi_ev.flags = SRAPI_MOUSE_BUTTON;
		srapi_ev.x = mx;
		srapi_ev.y = my;
		srapi_ev.buttons = pressed ? 1 : 0;
		libgHandleInput(st->ui, &srapi_ev);

		int32_t vy = HEADER_TOTAL_H;
		int32_t vh = (int32_t)st->height - HEADER_TOTAL_H - STATUS_BAR_H;

		if (pressed && my >= vy && my < vy + vh && mx < (int32_t)st->width - SCROLLBAR_W) {
			int32_t doc_x = mx;
			int32_t doc_y = my - vy + st->scroll_y;
			const char *link = html_layout_hit_test(st->layout, doc_x, doc_y);
			if (link != NULL) {
				browser_navigate(st, link);
				return;
			}
		}

		if (my < HEADER_TOTAL_H) {
			st->dirty_flags |= BROWSER_DIRTY_HEADER;
		} else {
			st->dirty_flags |= BROWSER_DIRTY_VIEWPORT;
		}
		return;
	}

	if (event->kind == SPROT_EVENT_POINTER_AXIS) {
		if (event->u.pointer_axis.dy < 0) {
			st->scroll_y -= 48;
			if (st->scroll_y < 0) st->scroll_y = 0;
		} else if (event->u.pointer_axis.dy > 0) {
			st->scroll_y += 48;
			if (st->scroll_y > st->max_scroll_y) st->scroll_y = st->max_scroll_y;
		}
		st->dirty_flags |= BROWSER_DIRTY_VIEWPORT;
		return;
	}

	if (event->kind == SPROT_EVENT_KEY) {
		int pressed = (event->u.key.state == SPROT_KEY_STATE_PRESSED);
		if (pressed) {
			if (event->u.key.scancode == 0x51) { /* Down arrow */
				st->scroll_y += 24;
				if (st->scroll_y > st->max_scroll_y) st->scroll_y = st->max_scroll_y;
				st->dirty_flags |= BROWSER_DIRTY_VIEWPORT;
				return;
			} else if (event->u.key.scancode == 0x52) { /* Up arrow */
				st->scroll_y -= 24;
				if (st->scroll_y < 0) st->scroll_y = 0;
				st->dirty_flags |= BROWSER_DIRTY_VIEWPORT;
				return;
			} else if (event->u.key.scancode == 0x4e) { /* Page Down */
				st->scroll_y += 200;
				if (st->scroll_y > st->max_scroll_y) st->scroll_y = st->max_scroll_y;
				st->dirty_flags |= BROWSER_DIRTY_VIEWPORT;
				return;
			} else if (event->u.key.scancode == 0x4b) { /* Page Up */
				st->scroll_y -= 200;
				if (st->scroll_y < 0) st->scroll_y = 0;
				st->dirty_flags |= BROWSER_DIRTY_VIEWPORT;
				return;
			}
		}

		struct srapi_input_event srapi_ev;
		memset(&srapi_ev, 0, sizeof(srapi_ev));
		srapi_ev.type = SRAPI_INPUT_KEYBOARD;
		srapi_ev.flags = pressed ? SRAPI_KEY_PRESS : SRAPI_KEY_RELEASE;
		srapi_ev.key = event->u.key.scancode;
		srapi_ev.ch = translate_key_to_char(event->u.key.scancode, event->u.key.modifiers);
		libgHandleInput(st->ui, &srapi_ev);
		st->dirty_flags |= BROWSER_DIRTY_HEADER;
		return;
	}
}
