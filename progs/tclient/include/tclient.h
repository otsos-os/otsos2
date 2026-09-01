#ifndef TCLIENT_H
#define TCLIENT_H

#include <libg.h>
#include <mtproto.h>
#include <sprot/client.h>

#define TCLIENT_WIDTH	900
#define TCLIENT_HEIGHT	620
#define TCLIENT_DIALOG_W	280
#define TCLIENT_ROW_H	48

typedef struct tclient_state {
	sprot_connection_t	*conn;
	sprot_surface_t		*surface;
	libg_context_t		*ui;
	mtp_client_t		*mtp;
	mtp_peer_t		selected;
	int			kq;
	int			running;
	int			dirty;
	int			selected_index;
	int			verbose;
	int			flood_shown;
	int			dialogs_asked;
	int32_t			dialog_scroll;
	int32_t			history_scroll;
	int32_t			topic_scroll;
	int32_t			selected_topic;
	uint32_t		update_version;
	char			api_id[16];
	char			api_hash[64];
	char			phone[MTP_MAX_PHONE];
	char			code[32];
	char			password[MTP_MAX_PASSWORD];
	char			message[MTP_MAX_TEXT];
	char			status[MTP_MAX_ERROR];
} tclient_state_t;

int	tclient_present(void *userdata, const struct srapi_region *region);
int	tclient_wait_surface(sprot_connection_t *conn, sprot_surface_t *surface);
void	tclient_draw(tclient_state_t *st);
void	tclient_draw_login(tclient_state_t *st);
int	tclient_event(tclient_state_t *st, const sprot_event_t *event);
void	tclient_tick(tclient_state_t *st);

int	tclient_parse_args(int argc, char **argv, tclient_state_t *st);
void	tclient_usage(void);
void	tclient_trace_install(tclient_state_t *st);
void	tclient_trace(const tclient_state_t *st, const char *fmt, ...);

#endif
