#ifndef _BROWSER_H
#define _BROWSER_H

#include <html.h>
#include <libg.h>
#include <sprot/client.h>
#include <sprot/sprot.h>
#include <stdint.h>
#include <stddef.h>

#define BROWSER_VERSION "0.1"
#define BROWSER_DEFAULT_URL "http://example.com"
#define BROWSER_MAX_URL 1024
#define BROWSER_HISTORY_MAX 32

#define BROWSER_DIRTY_HEADER   0x01
#define BROWSER_DIRTY_VIEWPORT 0x02
#define BROWSER_DIRTY_STATUS   0x04
#define BROWSER_DIRTY_ALL      (BROWSER_DIRTY_HEADER | BROWSER_DIRTY_VIEWPORT | BROWSER_DIRTY_STATUS)

typedef struct browser_history_entry {
	char	url[BROWSER_MAX_URL];
	char	title[128];
} browser_history_entry_t;

typedef struct browser_state {
	sprot_connection_t	*conn;
	sprot_surface_t		*surface;
	libg_context_t		*ui;
	uint32_t		width;
	uint32_t		height;
	int			running;
	uint32_t		dirty_flags;
	uint32_t		last_hot_id;
	int32_t			mouse_x;
	int32_t			mouse_y;
	uint32_t		mouse_buttons;

	/* URL and navigation */
	char			current_url[BROWSER_MAX_URL];
	char			input_url[BROWSER_MAX_URL];
	char			status_msg[256];
	char			hover_href[BROWSER_MAX_URL];

	/* History */
	browser_history_entry_t	history[BROWSER_HISTORY_MAX];
	int			history_count;
	int			history_pos;

	/* Current document */
	char			*raw_html;
	size_t			raw_html_len;
	html_doc_t		*doc;
	html_layout_t		*layout;
	int32_t			scroll_y;
	int32_t			max_scroll_y;
	int			is_loading;

	/* Scrollbar dragging */
	int			dragging_scrollbar;
	int32_t			drag_start_y;
	int32_t			drag_start_scroll;

	/* Stats */
	int			images_count;
	int			images_loaded;
	size_t			page_size_bytes;
} browser_state_t;

/* Networking */
int	browser_fetch_url(const char *url, char **out_body, size_t *out_len, char *out_final_url, size_t max_url_len);
int	browser_navigate(browser_state_t *st, const char *url);
void	browser_history_push(browser_state_t *st, const char *url, const char *title);
void	browser_history_back(browser_state_t *st);
void	browser_history_forward(browser_state_t *st);

/* UI & Drawing */
void	browser_ui_init(browser_state_t *st);
void	browser_draw(browser_state_t *st);
void	browser_handle_event(browser_state_t *st, const sprot_event_t *event);
void	browser_status_set(browser_state_t *st, const char *msg);

#endif
