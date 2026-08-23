/* !DEFINES!

$define %type browser_ui as UI layout and rendering
$define %func browser_draw as procedure with args browser_state *
$define %func browser_handle_event as procedure with args browser_state *, const sprot_event *
$define %func browser_navigate as function with args browser_state *, const char *

*/

/* !SPACE!

$space %internal draw_top_bar, draw_toolbar, draw_viewport, draw_status_bar, draw_scrollbar
$space %internal translate_key_to_char, get_header_widget_id
$space %export browser_ui_init, browser_draw, browser_handle_event
$space %export browser_navigate, browser_status_set
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
		strncpy(st->history[st->history_pos].title, title, sizeof(st->history[0].title) - 1);
	} else {
		st->history[st->history_pos].title[0] = '\0';
	}
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

int
browser_navigate(browser_state_t *st, const char *url)
{
	char	final_url[BROWSER_MAX_URL];
	char	*body = NULL;
	size_t	len = 0;
	int	ret;

	if (st == NULL || url == NULL || url[0] == '\0') {
		return (-1);
	}

	char full_url[BROWSER_MAX_URL];
	if (url[0] == '/' && st->current_url[0] != '\0') {
		char proto[16], host[256];
		if (sscanf(st->current_url, "%15[^:]://%255[^/]", proto, host) == 2) {
			snprintf(full_url, sizeof(full_url), "%s://%s%s", proto, host, url);
		} else {
			strncpy(full_url, url, sizeof(full_url) - 1);
		}
	} else {
		strncpy(full_url, url, sizeof(full_url) - 1);
	}
	full_url[sizeof(full_url) - 1] = '\0';

	char status[256];
	snprintf(status, sizeof(status), "Connecting to %s...", full_url);
	browser_status_set(st, status);

	st->is_loading = 1;
	ret = browser_fetch_url(full_url, &body, &len, final_url, sizeof(final_url));
	st->is_loading = 0;

	if (ret != 0 || body == NULL) {
		snprintf(status, sizeof(status), "Error loading %s", full_url);
		browser_status_set(st, status);
		return (-1);
	}

	if (st->layout != NULL) {
		html_layout_free(st->layout);
		st->layout = NULL;
	}
	if (st->doc != NULL) {
		html_doc_free(st->doc);
		st->doc = NULL;
	}
	if (st->raw_html != NULL) {
		free(st->raw_html);
	}

	st->raw_html = body;
	st->raw_html_len = len;
	st->page_size_bytes = len;
	strncpy(st->current_url, final_url, sizeof(st->current_url) - 1);
	st->current_url[sizeof(st->current_url) - 1] = '\0';
	strncpy(st->input_url, final_url, sizeof(st->input_url) - 1);
	st->input_url[sizeof(st->input_url) - 1] = '\0';

	st->doc = html_parse(body, len);
	int32_t viewport_w = (int32_t)st->width - SCROLLBAR_W;
	if (viewport_w < 100) viewport_w = 600;
	st->layout = html_layout_create(st->doc, viewport_w);

	st->scroll_y = 0;
	int32_t view_h = (int32_t)st->height - HEADER_TOTAL_H - STATUS_BAR_H;
	if (st->layout != NULL && st->layout->content_height > view_h) {
		st->max_scroll_y = st->layout->content_height - view_h;
	} else {
		st->max_scroll_y = 0;
	}

	char win_title[256];
	if (st->doc != NULL && st->doc->title != NULL && st->doc->title[0] != '\0') {
		snprintf(win_title, sizeof(win_title), "Dillo: %s", st->doc->title);
		browser_history_push(st, final_url, st->doc->title);
	} else {
		snprintf(win_title, sizeof(win_title), "Dillo: %s", final_url);
		browser_history_push(st, final_url, final_url);
	}
	if (st->surface != NULL) {
		sprot_set_title(st->surface, win_title);
	}

	snprintf(status, sizeof(status), "Loaded %u bytes", (unsigned int)len);
	browser_status_set(st, status);
	st->dirty_flags = BROWSER_DIRTY_ALL;
	return (0);
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
	st->history_count = 0;
	st->history_pos = -1;
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

	libgTextScale(st->ui, box1.x + 6, box1.y + 2, "Images", 0xFF666666, 1);
	libgTextScale(st->ui, box1.x + 10, box1.y + 13, "0 of 0", 0xFF333333, 1);

	char page_str[32];
	double kb = (double)st->page_size_bytes / 1024.0;
	snprintf(page_str, sizeof(page_str), "%.1f KB", kb);
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
	libgFillRect(st->ui, rect, 0xFFFFFFFF);

	if (st->layout != NULL) {
		html_layout_render(st->ui, st->layout, vx, vy, vw, vh, st->scroll_y);
	}

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

	if (st->dirty_flags & BROWSER_DIRTY_HEADER) {
		draw_top_bar(st);
		draw_toolbar(st);
	}
	if (st->dirty_flags & BROWSER_DIRTY_VIEWPORT) {
		draw_viewport(st);
	}
	if (st->dirty_flags & BROWSER_DIRTY_STATUS) {
		draw_status_bar(st);
	}

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
