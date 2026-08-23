/* !DEFINES!

$define %type browser_main as application entry point
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal browser_present_cb, browser_wait_surface
$space %export main

*/

#include <browser.h>
#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BROWSER_DEF_W 800
#define BROWSER_DEF_H 600
#define BROWSER_SURFACE_TRIES 100
#define BROWSER_SURFACE_WAIT_MS 20

static int
browser_present_cb(void *userdata, const struct srapi_region *region)
{
	sprot_surface_t	*surface;

	surface = (sprot_surface_t *)userdata;
	if (surface == NULL) {
		return (-1);
	}
	if (region != NULL) {
		if (sprot_damage(surface, (int32_t)region->x,
		    (int32_t)region->y, region->width, region->height) != 0) {
			return (-1);
		}
	} else if (sprot_damage(surface, 0, 0, sprot_surface_width(surface),
	    sprot_surface_height(surface)) != 0) {
		return (-1);
	}
	return (sprot_commit(surface));
}

static int
browser_wait_surface(sprot_connection_t *conn, sprot_surface_t *surface)
{
	sprot_event_t	event;
	int		i, ret;

	for (i = 0; i < BROWSER_SURFACE_TRIES; i++) {
		if (sprot_surface_id(surface) != 0) {
			return (0);
		}
		ret = sprot_poll_event(conn, &event, BROWSER_SURFACE_WAIT_MS);
		if (ret < 0) {
			return (-1);
		}
		if (ret == 0) {
			continue;
		}
		if (event.kind == SPROT_EVENT_SURFACE_CREATED &&
		    sprot_surface_id(surface) != 0) {
			return (0);
		}
		if (event.kind == SPROT_EVENT_DISCONNECT) {
			return (-1);
		}
	}
	return (sprot_surface_id(surface) != 0 ? 0 : -1);
}

int
main(int argc, char **argv, char **envp)
{
	browser_state_t	st;
	sprot_event_t	event;
	libg_style_t	style;
	const char	*start_url;
	int		ret, burst;

	(void)envp;
	personality(0);

	start_url = BROWSER_DEFAULT_URL;
	if (argc >= 2 && argv[1] != NULL && argv[1][0] != '\0') {
		start_url = argv[1];
	}

	memset(&st, 0, sizeof(st));
	st.width = BROWSER_DEF_W;
	st.height = BROWSER_DEF_H;
	st.running = 1;
	st.dirty_flags = BROWSER_DIRTY_ALL;

	st.conn = sprot_connect(SPROT_DEFAULT_SERVICE);
	if (st.conn == NULL) {
		termPrint("browser: cannot connect to SWM / Sprot service\n");
		return (1);
	}

	st.surface = sprot_create_surface(st.conn, st.width, st.height);
	if (st.surface == NULL || browser_wait_surface(st.conn, st.surface) != 0) {
		termPrint("browser: cannot create surface\n");
		sprot_disconnect(st.conn);
		return (1);
	}

	if (sprot_set_role(st.surface, SPROT_SURFACE_ROLE_TOPLEVEL, 0, 70, 45) != 0 ||
	    sprot_set_title(st.surface, "Dillo: Web Browser") != 0 ||
	    sprot_set_visible(st.surface, 1) != 0) {
		termPrint("browser: failed to configure window surface\n");
		sprot_destroy_surface(st.surface);
		sprot_disconnect(st.conn);
		return (1);
	}

	libgDefaultStyle(&style);
	style.background = 0xFFEDEAE6;
	ret = libgCreateForTarget(sprot_surface_pixels(st.surface),
	    st.width, st.height, sprot_surface_stride(st.surface),
	    browser_present_cb, st.surface, &style, &st.ui);
	if (ret != LIBG_OK || st.ui == NULL) {
		termPrint("browser: LibG initialization failed\n");
		sprot_destroy_surface(st.surface);
		sprot_disconnect(st.conn);
		return (1);
	}

	browser_ui_init(&st);
	st.kq = eventKqueue();
	if (st.kq < 0) {
		termPrint("browser: cannot open kqueue\n");
		libgDestroy(st.ui);
		sprot_destroy_surface(st.surface);
		sprot_disconnect(st.conn);
		return (1);
	}
	browser_loader_init(&st.loader, st.kq);
	browser_navigate(&st, start_url);
	browser_draw(&st);

	burst = 0;
	while (st.running) {
		ret = sprot_poll_event(st.conn, &event, st.dirty_flags ? 0 : 20);
		if (ret < 0) {
			st.running = 0;
			break;
		}
		if (ret > 0) {
			browser_handle_event(&st, &event);
			burst++;
			if (burst < 16) {
				continue;
			}
		}
		burst = 0;

		/*
		 * Advance the in-flight load once per pass; it arms the
		 * dirty flags itself whenever its status changes.
		 */
		browser_load_poll(&st);

		if (st.dirty_flags != 0) {
			browser_draw(&st);
		}
	}

	if (st.layout != NULL) {
		html_layout_free(st.layout);
	}
	if (st.doc != NULL) {
		html_doc_free(st.doc);
	}
	if (st.raw_html != NULL) {
		free(st.raw_html);
	}
	browser_loader_reset(&st.loader);
	if (st.kq >= 0) {
		eventClose(st.kq);
	}
	if (st.ui != NULL) {
		libgDestroy(st.ui);
	}
	if (st.surface != NULL) {
		sprot_destroy_surface(st.surface);
	}
	if (st.conn != NULL) {
		sprot_disconnect(st.conn);
	}

	return (0);
}
