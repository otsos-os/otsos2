#ifndef _BROWSER_H
#define _BROWSER_H

#include <html.h>
#include <libg.h>
#include <native.h>
#include <sprot/client.h>
#include <sprot/sprot.h>
#include <stdint.h>
#include <stddef.h>

#define BROWSER_VERSION "0.1"
#define BROWSER_DEFAULT_URL "http://example.com"
#define BROWSER_MAX_URL 1024
#define BROWSER_HISTORY_MAX 32
#define BROWSER_MAX_HOST 256
#define BROWSER_MAX_PATH 1024
#define BROWSER_MAX_REDIRECTS 5

#define BROWSER_DIRTY_HEADER   0x01
#define BROWSER_DIRTY_VIEWPORT 0x02
#define BROWSER_DIRTY_STATUS   0x04
#define BROWSER_DIRTY_ALL      (BROWSER_DIRTY_HEADER | BROWSER_DIRTY_VIEWPORT | BROWSER_DIRTY_STATUS)

/*
 * DNS is tried against each server in turn, each attempt retried, so a dead
 * resolver costs BROWSER_DNS_SERVERS * BROWSER_DNS_TRIES * timeout in the
 * worst case and never blocks the UI while it happens.
 */
#define BROWSER_DNS_SERVERS 3
#define BROWSER_DNS_TRIES 2
#define BROWSER_DNS_TIMEOUT_MS 2000
#define BROWSER_CONNECT_TIMEOUT_MS 10000
#define BROWSER_RECV_TIMEOUT_MS 20000

/* Response ceiling: a hostile or endless stream must not exhaust the heap. */
#define BROWSER_MAX_RESPONSE (8u * 1024u * 1024u)
#define BROWSER_CHUNK_SIZE 4096

/* Ceiling on a document walk, so a malformed tree cannot spin the UI. */
#define BROWSER_MAX_NODES 200000
#define BROWSER_SVG_CACHE_MAX 16

/*
 * External stylesheets fetched per page.  Sheets are pulled one at a time
 * through the same loader as the document, so each one costs a full DNS +
 * connect + read; past a handful the page would take visibly longer to appear
 * for progressively less styling.  Sheets beyond the limit are ignored.
 */
#define BROWSER_MAX_CSS_LINKS 8

/*
 * A stylesheet larger than this is skipped rather than parsed.  The CSS engine
 * caps its own text at 256 KB, and a sheet that big is a bundle full of rules
 * this renderer cannot express anyway.
 */
#define BROWSER_MAX_CSS_BYTES (256u * 1024u)

/* Caret blink half-period.  Long enough not to be a strobe, short enough to
 * read as a cursor rather than a stuck pixel. */
#define BROWSER_CARET_MS 500

/*
 * Longest value a page text field will hold.  A query string longer than this
 * would not survive BROWSER_MAX_URL after percent-encoding anyway.
 */
#define BROWSER_CTRL_VALUE_MAX 512

/*
 * LibG's glyph advance at scale 1.  Duplicated from LibHtml's own copy on
 * purpose: this one converts a click x into a caret index, and if the two ever
 * disagree the symptom is the caret landing one character off from where the
 * user clicked.
 */
#define BROWSER_GLYPH_ADVANCE 6

typedef struct browser_svg_entry {
	char		*ref;
	svg_doc_t	*doc;
} browser_svg_entry_t;

typedef enum browser_load_state {
	BROWSER_LOAD_IDLE = 0,
	BROWSER_LOAD_DNS,
	BROWSER_LOAD_CONNECT,
	BROWSER_LOAD_SEND,
	BROWSER_LOAD_RECV,
	BROWSER_LOAD_DONE,
	BROWSER_LOAD_ERROR
} browser_load_state_t;

typedef struct browser_history_entry {
	char	url[BROWSER_MAX_URL];
	char	title[128];
} browser_history_entry_t;

/*
 * One in-flight HTTP GET, advanced a step at a time by browser_loader_step().
 * Nothing in here blocks: every socket is opened API_NET_OPEN_NONBLOCK and
 * every wait is expressed as (wait_fd, wait_filter, deadline) for the caller's
 * kqueue.  That is what keeps the UI responsive during a load.
 */
typedef struct browser_loader {
	browser_load_state_t	state;
	char			url[BROWSER_MAX_URL];
	char			host[BROWSER_MAX_HOST];
	char			path[BROWSER_MAX_PATH];
	char			error[128];
	int			port;
	uint32_t		ip;

	/* DNS */
	int			dns_fd;
	int			dns_server;
	int			dns_try;
	uint16_t		dns_id;
	uint64_t		dns_deadline;

	/* TCP */
	int			sock;
	uint64_t		deadline;

	/* Request */
	char			request[2048];
	size_t			request_len;
	size_t			request_sent;

	/* Response */
	char			*response;
	size_t			response_len;
	size_t			response_cap;

	int			redirects;

	/*
	 * The loader owns its own kqueue registrations because closing a handle
	 * does not detach its knote - only EV_DELETE or kqueue teardown does.
	 * Exactly one (wait_fd, wait_filter) pair is armed at a time; wait_fd is
	 * -1 when nothing is pending.
	 */
	int			kq;
	int			wait_fd;
	int16_t			wait_filter;
} browser_loader_t;

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

	/*
	 * External stylesheet fetch phase.  css_next is the index of the <link>
	 * being fetched; css_fetching says the loader currently holds a sheet
	 * rather than a document, which is what tells browser_load_poll() not to
	 * replace the page with the CSS it just received.
	 */
	int			css_next;
	int			css_fetching;
	int			css_applied;

	/*
	 * Caret blink state for a focused page control.  Kept here and not in
	 * the layout so that rebuilding the layout does not reset the phase
	 * mid-blink.
	 */
	uint64_t		caret_next_ms;

	/* In-flight load */
	int			kq;
	browser_loader_t	loader;
	/*
	 * Set when a navigation is requested from inside a draw pass (a button
	 * or a link click).  Starting the load right there would recurse into
	 * drawing, so the main loop picks it up after the frame.
	 */
	char			pending_url[BROWSER_MAX_URL];
	int			pending_nav;

	/* Scrollbar dragging */
	int			dragging_scrollbar;
	int32_t			drag_start_y;
	int32_t			drag_start_scroll;

	/* Stats */
	int			images_count;
	int			images_loaded;
	size_t			page_size_bytes;
	/*
	 * Last byte count published to the header, in KB.  Compared before
	 * repainting: without it the stats box would be marked dirty on every
	 * received packet and the window would redraw hundreds of times a page.
	 */
	size_t			progress_kb;
	browser_svg_entry_t	svg_cache[BROWSER_SVG_CACHE_MAX];
	int			svg_count;
} browser_state_t;

/* URL and HTTP helpers (pure, no I/O) */
void	browser_url_normalize(const char *url, char *out, size_t max_out);
int	browser_url_parse(const char *url, char *host, size_t max_host,
	    int *port, char *path, size_t max_path, int *is_https);
void	browser_url_resolve(const char *base, const char *ref, char *out,
	    size_t max_out);
uint64_t browser_now_ms(void);
int	browser_dns_build(const char *host, uint16_t id, unsigned char *out,
	    size_t max_out);
uint32_t browser_dns_parse(const unsigned char *resp, size_t len,
	    uint16_t id);
uint32_t browser_dns_server(int index);
uint32_t browser_ip_literal(const char *host);
int	browser_http_request(char *out, size_t max_out, const char *host,
	    int port, const char *path);
int	browser_http_status(const char *resp, size_t len);
int	browser_http_location(const char *resp, size_t len, char *out,
	    size_t max_out);
size_t	browser_http_body_offset(const char *resp, size_t len);

/* Async loader */
void	browser_loader_init(browser_loader_t *ld, int kq);
int	browser_loader_start(browser_loader_t *ld, const char *url);
int	browser_loader_step(browser_loader_t *ld);
void	browser_loader_abort(browser_loader_t *ld);
void	browser_loader_reset(browser_loader_t *ld);
int	browser_loader_timeout(const browser_loader_t *ld);

void	browser_svg_cache_free(browser_state_t *st);
int	browser_svg_draw(void *userdata, libg_context_t *ctx,
	    libg_rect_t rect, const char *ref);

/* Navigation */
int	browser_navigate(browser_state_t *st, const char *url);
void	browser_load_poll(browser_state_t *st);
void	browser_history_push(browser_state_t *st, const char *url, const char *title);
void	browser_history_back(browser_state_t *st);
void	browser_history_forward(browser_state_t *st);

/* UI & Drawing */
void	browser_ui_init(browser_state_t *st);
void	browser_draw(browser_state_t *st);
void	browser_handle_event(browser_state_t *st, const sprot_event_t *event);
void	browser_status_set(browser_state_t *st, const char *msg);

#endif
