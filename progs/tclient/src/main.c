/* !DEFINES!

$define %type tclient as graphical Telegram client for SWM
$define %func tclient_draw_login as procedure with args state
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal tclient_copy, tclient_input, tclient_start, tclient_send
$space %internal tclient_list_metrics, tclient_list_clamp, tclient_wheel
$space %internal tclient_hint, tclient_auth_status, tclient_retry_row
$space %internal tclient_submit_password, tclient_password_row
$space %internal tclient_password_bits
$space %export tclient_present, tclient_wait_surface, tclient_draw
$space %export tclient_draw_login, tclient_event, tclient_tick, main

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tclient.h"

#define ID_API_ID	1
#define ID_API_HASH	2
#define ID_CONNECT	3
#define ID_PHONE	4
#define ID_SEND_CODE	5
#define ID_CODE	6
#define ID_LOGIN	7
#define ID_CHANGE_PHONE	8
#define ID_MESSAGE	9
#define ID_SEND	10
#define ID_REFRESH	11
#define ID_RECONNECT	12
#define ID_PASSWORD	13
#define ID_CHECK_PASSWORD	14
#define ID_DIALOG_SCROLL	15
#define ID_DIALOG_BASE	100
#define TCLIENT_LIST_Y		52
#define TCLIENT_LIST_PAD	8
#define TCLIENT_LIST_GAP	4
#define TCLIENT_WHEEL_ROWS	3
#define TCLIENT_ARROW_ROWS	1
#define TCLIENT_SURFACE_TRIES	100
#define TCLIENT_SURFACE_WAIT	20
#define TCLIENT_POLL_MS		20
#define LOGIN_X		30
#define LOGIN_FIELD_W	300
#define LOGIN_BTN_W	150
#define LOGIN_ROW_H	32
#define LOGIN_STEP1_Y	124
#define LOGIN_STEP2_Y	228
#define LOGIN_STEP3_Y	316
#define LOGIN_STATUS_Y	412
#define LOGIN_RETRY_Y	456

static uint32_t
tclient_key_char(uint32_t key, uint32_t mods)
{
	int	shift;

	shift = (mods & 3u) != 0;
	if (key >= 0x04 && key <= 0x1d) {
		return (shift ? 'A' : 'a') + key - 0x04;
	}
	if (key >= 0x1e && key <= 0x27) {
		return (shift ? ")!@#$%^&*("[key - 0x1e] :
		    "1234567890"[key - 0x1e]);
	}
	switch (key) {
	case 0x28: case 0x58: return ('\n');
	case 0x2a: return ('\b');
	case 0x2c: return (' ');
	case 0x2d: return (shift ? '_' : '-');
	case 0x2e: return (shift ? '+' : '=');
	case 0x36: return (shift ? '<' : ',');
	case 0x37: return (shift ? '>' : '.');
	case 0x38: return (shift ? '?' : '/');
	default: return (0);
	}
}

typedef struct tclient_list {
	int32_t	top;
	int32_t	rows;
	int32_t	total;
	int32_t	row_w;
	int32_t	bar_x;
	int32_t	bar_w;
} tclient_list_t;

static void
tclient_list_metrics(const tclient_state_t *st, tclient_list_t *l)
{
	int32_t	avail, right;

	l->top = TCLIENT_LIST_Y;
	if (mtpState(st->mtp) == MTP_STATE_FAILED) {
		l->top += LOGIN_ROW_H + TCLIENT_LIST_PAD;
	}
	l->total = mtpDialogCount(st->mtp);
	avail = TCLIENT_HEIGHT - l->top - TCLIENT_LIST_PAD;
	l->rows = avail > 0 ? avail / TCLIENT_ROW_H : 0;
	l->bar_w = l->total > l->rows ? libgScrollbarThickness() : 0;
	l->bar_x = TCLIENT_DIALOG_W - TCLIENT_LIST_PAD - l->bar_w;
	right = l->bar_w != 0 ? l->bar_x - TCLIENT_LIST_GAP :
	    TCLIENT_DIALOG_W - TCLIENT_LIST_PAD;
	l->row_w = right - TCLIENT_LIST_PAD;
	if (l->row_w < 0) {
		l->row_w = 0;
	}
}

static void
tclient_list_clamp(tclient_state_t *st, const tclient_list_t *l)
{
	int32_t	max;

	max = l->total - l->rows;
	if (max < 0) {
		max = 0;
	}
	if (st->dialog_scroll > max) {
		st->dialog_scroll = max;
	}
	if (st->dialog_scroll < 0) {
		st->dialog_scroll = 0;
	}
}


static void
tclient_wheel(tclient_state_t *st, int32_t dy)
{
	tclient_list_t	l;
	int32_t		x;

	if (st->mtp == NULL || !mtpIsAuthorized(st->mtp) || dy == 0) {
		return;
	}
	libgMousePosition(st->ui, &x, NULL, NULL);
	if (x < 0 || x >= TCLIENT_DIALOG_W) {
		return;
	}
	tclient_list_metrics(st, &l);
	st->dialog_scroll += dy > 0 ? TCLIENT_WHEEL_ROWS : -TCLIENT_WHEEL_ROWS;
	tclient_list_clamp(st, &l);
}


static void
tclient_input(tclient_state_t *st, const sprot_event_t *event)
{
	struct srapi_input_event	input;

	memset(&input, 0, sizeof(input));
	if (event->kind == SPROT_EVENT_POINTER_MOTION ||
	    event->kind == SPROT_EVENT_POINTER_ENTER) {
		input.type = SRAPI_INPUT_MOUSE;
		input.flags = SRAPI_MOUSE_MOVE | SRAPI_MOUSE_ABSOLUTE;
		input.x = event->u.pointer_motion.x;
		input.y = event->u.pointer_motion.y;
	} else if (event->kind == SPROT_EVENT_POINTER_BUTTON) {
		input.type = SRAPI_INPUT_MOUSE;
		input.flags = SRAPI_MOUSE_BUTTON;
		input.buttons = event->u.pointer_button.state == SPROT_BUTTON_STATE_PRESSED ?
		    event->u.pointer_button.button : 0;
	} else if (event->kind == SPROT_EVENT_KEY) {
		input.type = SRAPI_INPUT_KEYBOARD;
		input.flags = event->u.key.state == SPROT_KEY_STATE_PRESSED ?
		    SRAPI_KEY_PRESS : SRAPI_KEY_RELEASE;
		input.key = event->u.key.scancode;
		input.ch = tclient_key_char(event->u.key.scancode, event->u.key.modifiers);
	} else if (event->kind == SPROT_EVENT_POINTER_AXIS) {
		tclient_wheel(st, event->u.pointer_axis.dy);
		return;
	} else {
		return;
	}
	(void)libgHandleInput(st->ui, &input);
}

int
tclient_present(void *userdata, const struct srapi_region *region)
{
	sprot_surface_t	*surface;

	surface = (sprot_surface_t *)userdata;
	if (surface == NULL) {
		return (-1);
	}
	if (region == NULL) {
		if (sprot_damage(surface, 0, 0, sprot_surface_width(surface),
		    sprot_surface_height(surface)) != 0) {
			return (-1);
		}
	} else if (sprot_damage(surface, region->x, region->y, region->width,
	    region->height) != 0) {
		return (-1);
	}
	return (sprot_commit(surface));
}

int
tclient_wait_surface(sprot_connection_t *conn, sprot_surface_t *surface)
{
	sprot_event_t	event;
	int		i, ret;

	for (i = 0; i < TCLIENT_SURFACE_TRIES; i++) {
		if (sprot_surface_id(surface) != 0) {
			return (0);
		}
		ret = sprot_poll_event(conn, &event, TCLIENT_SURFACE_WAIT);
		if (ret < 0 || event.kind == SPROT_EVENT_DISCONNECT) {
			return (-1);
		}
	}
	return (sprot_surface_id(surface) == 0 ? -1 : 0);
}

static void
tclient_start(tclient_state_t *st)
{
	mtp_config_t	cfg;
	int		ret;

	if (st->mtp != NULL) {
		return;
	}
	memset(&cfg, 0, sizeof(cfg));
	cfg.api_id = atoi(st->api_id);
	cfg.api_hash = st->api_hash;
	cfg.device_model = "otsos tclient";
	cfg.system_version = "otsos";
	cfg.app_version = "1.0";
	cfg.lang_code = "en";
	cfg.auth_path = "/tclient.auth";
	cfg.dc_index = 0;
	tclient_trace(st, "connecting with api_id=%d, auth file %s", cfg.api_id,
	    cfg.auth_path);
	st->dialogs_asked = 0;
	st->dialog_scroll = 0;
	ret = mtpCreate(&cfg, st->kq, &st->mtp);
	if (ret != MTP_OK) {
		snprintf(st->status, sizeof(st->status),
		    "Connect: %s%s", mtpStrerror(ret),
		    st->verbose > 0 ? "" : " (run tclient -v for the reason)");
	}
}


static void
tclient_hint(tclient_state_t *st, libg_rect_t r, const char *buf,
    const char *hint)
{
	if (buf[0] != '\0') {
		return;
	}
	libgTextScale(st->ui, r.x + 8, r.y + (r.height - 14) / 2, hint,
	    0xFF7A8590, 2);
}


static void
tclient_auth_status(tclient_state_t *st, const char *what, int ret)
{
	if (ret == MTP_OK) {
		snprintf(st->status, sizeof(st->status), "%s sent, waiting for "
		    "Telegram", what);
		return;
	}
	if (ret == MTP_ERR_NOTREADY) {
		snprintf(st->status, sizeof(st->status), "Still connecting (%s) "
		    "-- press %s again in a moment",
		    mtpStateName(mtpState(st->mtp)), what);
		return;
	}
	if (ret == MTP_ERR_FLOOD) {
		snprintf(st->status, sizeof(st->status), "Telegram rate limit: "
		    "wait %d s before trying again", mtpFloodWait(st->mtp));
		return;
	}
	snprintf(st->status, sizeof(st->status), "%s: %s", what,
	    mtpError(st->mtp)[0] != '\0' ? mtpError(st->mtp) : mtpStrerror(ret));
}


static void
tclient_retry_row(tclient_state_t *st, int x, int y)
{
	libg_rect_t	r;
	int		ret;

	if (mtpState(st->mtp) != MTP_STATE_FAILED) {
		return;
	}
	r = (libg_rect_t){ x, y, LOGIN_BTN_W, LOGIN_ROW_H };
	if (!(libgButton(st->ui, ID_RECONNECT, r, "Reconnect") &
	    LIBG_WIDGET_CLICKED)) {
		return;
	}
	ret = mtpReconnect(st->mtp);
	if (ret == MTP_OK) {
		st->dialogs_asked = 0;
		st->dialog_scroll = 0;
		snprintf(st->status, sizeof(st->status),
		    "Reconnecting to DC%d...", mtpDcId(st->mtp));
		return;
	}
	snprintf(st->status, sizeof(st->status), "Reconnect: %s",
	    mtpError(st->mtp)[0] != '\0' ? mtpError(st->mtp) : mtpStrerror(ret));
}

static int
tclient_password_bits(mtp_client_t *c)
{
	if (c == NULL) {
		return (0);
	}
	return ((mtpPasswordNeeded(c) ? 1 : 0) |
	    (mtpPasswordBusy(c) ? 2 : 0) |
	    (mtpPasswordReady(c) ? 4 : 0));
}

static void
tclient_submit_password(tclient_state_t *st)
{
	int	ret;

	if (mtpPasswordBusy(st->mtp)) {
		snprintf(st->status, sizeof(st->status), "Already checking the "
		    "previous attempt");
		return;
	}
	if (!mtpPasswordReady(st->mtp)) {
		snprintf(st->status, sizeof(st->status), "Waiting for a fresh "
		    "challenge from Telegram -- try again in a moment");
		return;
	}
	if (st->password[0] == '\0') {
		snprintf(st->status, sizeof(st->status), "Enter your cloud "
		    "password first");
		return;
	}
	ret = mtpCheckPassword(st->mtp, st->password);
	memset(st->password, 0, sizeof(st->password));
	if (ret == MTP_OK) {
		snprintf(st->status, sizeof(st->status), "Checking the password "
		    "-- this takes a few seconds");
		return;
	}
	tclient_auth_status(st, "Check password", ret);
}

static void
tclient_password_row(tclient_state_t *st)
{
	libg_rect_t	r;
	const char	*hint;
	char		line[160];
	int		busy, ready;

	if (!mtpPasswordNeeded(st->mtp)) {
		return;
	}
	busy = mtpPasswordBusy(st->mtp);
	ready = mtpPasswordReady(st->mtp);
	hint = mtpPasswordHint(st->mtp);
	if (hint[0] != '\0') {
		snprintf(line, sizeof(line), "3. Cloud password (hint: %s)",
		    hint);
	} else {
		snprintf(line, sizeof(line), "3. Cloud password for this "
		    "account");
	}
	libgText(st->ui, LOGIN_X, LOGIN_STEP3_Y - 22, line, 0xFF20252B);

	r = (libg_rect_t){ LOGIN_X, LOGIN_STEP3_Y, LOGIN_FIELD_W, LOGIN_ROW_H };
	if (libgPasswordField(st->ui, ID_PASSWORD, r, st->password,
	    sizeof(st->password)) & LIBG_WIDGET_SUBMIT) {
		tclient_submit_password(st);
	}
	if (st->password[0] == '\0') {
		tclient_hint(st, r, st->password, "your 2FA password");
	}
	r.x += LOGIN_FIELD_W + 20;
	r.width = LOGIN_BTN_W;
	if (libgButton(st->ui, ID_CHECK_PASSWORD, r, "Check") &
	    LIBG_WIDGET_CLICKED) {
		tclient_submit_password(st);
	}
	if (busy) {
		libgText(st->ui, LOGIN_X, LOGIN_STEP3_Y + LOGIN_ROW_H + 8,
		    "Checking: deriving the SRP proof (100000 PBKDF2 rounds), "
		    "then one request", 0xFF9A6700);
	} else if (!ready) {
		libgText(st->ui, LOGIN_X, LOGIN_STEP3_Y + LOGIN_ROW_H + 8,
		    "Fetching a fresh challenge from the DC -- each attempt "
		    "needs its own", 0xFF9A6700);
	}
}

void
tclient_draw_login(tclient_state_t *st)
{
	libg_rect_t	r;
	const char	*wait_for;
	int		flood, ret, have_code;

	flood = mtpFloodWait(st->mtp);
	have_code = mtpCodeHashPresent(st->mtp);
	libgTextScale(st->ui, LOGIN_X, 30, "Sign in to Telegram", 0xFF20252B, 3);
	libgText(st->ui, LOGIN_X, 76, "Connection:", 0xFF5E6872);
	libgText(st->ui, LOGIN_X + 90, 76, mtpStateName(mtpState(st->mtp)),
	    mtpIsReady(st->mtp) ? 0xFF2F7D32 : 0xFF9A6700);

	libgText(st->ui, LOGIN_X, LOGIN_STEP1_Y - 22,
	    "1. Phone number, with country code", 0xFF20252B);
	r = (libg_rect_t){ LOGIN_X, LOGIN_STEP1_Y, LOGIN_FIELD_W, LOGIN_ROW_H };
	if (libgTextField(st->ui, ID_PHONE, r, st->phone, sizeof(st->phone)) &
	    LIBG_WIDGET_SUBMIT) {
		if (!have_code) {
			tclient_auth_status(st, "Send code",
			    mtpSendCode(st->mtp, st->phone));
		} else {
			snprintf(st->status, sizeof(st->status), "A code was "
			    "already sent -- enter it below, or press Change "
			    "number to start over");
		}
	}
	tclient_hint(st, r, st->phone, "+1234567890");
	r.x += LOGIN_FIELD_W + 20;
	r.width = LOGIN_BTN_W;
	if (libgButton(st->ui, ID_SEND_CODE, r,
	    flood > 0 ? "Rate limited" : "Send code") & LIBG_WIDGET_CLICKED) {
		ret = mtpSendCode(st->mtp, st->phone);
		tclient_auth_status(st, "Send code", ret);
		if (ret == MTP_OK) {
			st->code[0] = '\0';
		}
	}

	if (have_code) {
		libgText(st->ui, LOGIN_X, LOGIN_STEP2_Y - 22,
		    "2. Code Telegram just sent you", 0xFF20252B);
		r = (libg_rect_t){ LOGIN_X, LOGIN_STEP2_Y, LOGIN_FIELD_W,
		    LOGIN_ROW_H };
		if (libgTextField(st->ui, ID_CODE, r, st->code,
		    sizeof(st->code)) & LIBG_WIDGET_SUBMIT) {
			tclient_auth_status(st, "Sign in",
			    mtpSignIn(st->mtp, st->phone, st->code));
		}
		tclient_hint(st, r, st->code, "12345");
		r.x += LOGIN_FIELD_W + 20;
		r.width = LOGIN_BTN_W;
		if (libgButton(st->ui, ID_LOGIN, r, "Sign in") &
		    LIBG_WIDGET_CLICKED) {
			tclient_auth_status(st, "Sign in",
			    mtpSignIn(st->mtp, st->phone, st->code));
		}
		r.x += LOGIN_BTN_W + 12;
		r.width = 170;
		if (libgButton(st->ui, ID_CHANGE_PHONE, r, "Change number") &
		    LIBG_WIDGET_CLICKED) {
			mtpResetCode(st->mtp);
			st->code[0] = '\0';
			snprintf(st->status, sizeof(st->status),
			    "Enter the number again, then press Send code");
		}
	} else {
		libgText(st->ui, LOGIN_X, LOGIN_STEP2_Y - 22,
		    "2. The code field appears once Telegram has sent one",
		    0xFF9AA4AE);
	}
	tclient_password_row(st);

	if (flood > 0) {
		wait_for = mtpFloodRequest(st->mtp);
		libgText(st->ui, LOGIN_X, LOGIN_STATUS_Y - 24, "Telegram is "
		    "rate limiting this account", 0xFF9A6700);
		{
			char	line[96];

			snprintf(line, sizeof(line), "%s is refused for another "
			    "%d s", wait_for[0] != '\0' ? wait_for : "the request",
			    flood);
			libgText(st->ui, LOGIN_X, LOGIN_STATUS_Y - 10, line,
			    0xFF9A6700);
		}
	}
	libgText(st->ui, LOGIN_X, LOGIN_STATUS_Y + 14, st->status, 0xFF9B2C2C);
	tclient_retry_row(st, LOGIN_X, LOGIN_RETRY_Y);
}

static void
tclient_send(tclient_state_t *st)
{
	int	ret;

	if (st->mtp == NULL || st->selected.kind == MTP_PEER_EMPTY ||
	    st->message[0] == '\0') {
		return;
	}
	ret = mtpSendMessage(st->mtp, &st->selected, st->message);
	if (ret == MTP_OK) {
		st->message[0] = '\0';
		return;
	}
	snprintf(st->status, sizeof(st->status), "Send: %s",
	    mtpError(st->mtp)[0] != '\0' ? mtpError(st->mtp) :
	    mtpStrerror(ret));
}

void
tclient_draw(tclient_state_t *st)
{
	const mtp_dialog_t	*d;
	const mtp_message_t	*m;
	tclient_list_t		l;
	libg_rect_t		r;
	uint32_t		result;
	int32_t			i, y;

	(void)libgBegin(st->ui, 0xFFF3F4F6);
	if (st->mtp == NULL) {
		libgTextScale(st->ui, 30, 32, "Telegram", 0xFF20252B, 3);
		libgText(st->ui, 30, 80, "API ID and API hash from my.telegram.org", 0xFF5E6872);
		r = (libg_rect_t){ 30, 110, 220, 32 };
		(void)libgTextField(st->ui, ID_API_ID, r, st->api_id, sizeof(st->api_id));
		r.x = 270; r.width = 400;
		(void)libgTextField(st->ui, ID_API_HASH, r, st->api_hash, sizeof(st->api_hash));
		r = (libg_rect_t){ 690, 110, 130, 32 };
		if (libgButton(st->ui, ID_CONNECT, r, "Connect") & LIBG_WIDGET_CLICKED) {
			tclient_start(st);
		}
		libgText(st->ui, 30, 160, st->status, 0xFF9B2C2C);
	} else if (!mtpIsAuthorized(st->mtp)) {
		tclient_draw_login(st);
	} else {
		libgFillRect(st->ui, (libg_rect_t){ 0, 0, TCLIENT_DIALOG_W,
		    TCLIENT_HEIGHT }, 0xFFE6E9ED);
		libgTextScale(st->ui, 16, 18, "Chats", 0xFF20252B, 2);
		r = (libg_rect_t){ 185, 12, 80, 28 };
		if (libgButton(st->ui, ID_REFRESH, r, "Refresh") & LIBG_WIDGET_CLICKED) {
			(void)mtpGetDialogs(st->mtp, MTP_MAX_DIALOGS);
		}
		tclient_list_metrics(st, &l);
		tclient_list_clamp(st, &l);
		tclient_retry_row(st, TCLIENT_LIST_PAD, TCLIENT_LIST_Y);
		libgSetClip(st->ui, (libg_rect_t){ 0, l.top, TCLIENT_DIALOG_W,
		    l.rows * TCLIENT_ROW_H });
		y = l.top;
		for (i = st->dialog_scroll;
		    i < l.total && i < st->dialog_scroll + l.rows; i++) {
			d = mtpDialogAt(st->mtp, i);
			r = (libg_rect_t){ TCLIENT_LIST_PAD, y, l.row_w,
			    TCLIENT_ROW_H - 4 };
			result = libgButton(st->ui,
			    ID_DIALOG_BASE + (uint32_t)i, r,
			    d->title[0] != '\0' ? d->title : "Unknown chat");
			if (result & LIBG_WIDGET_CLICKED) {
				st->selected = d->peer;
				st->selected_index = i;
				(void)mtpGetHistory(st->mtp, &st->selected,
				    MTP_MAX_HISTORY, 0);
			}
			y += TCLIENT_ROW_H;
		}
		libgClearClip(st->ui);
		if (l.bar_w != 0) {
			r = (libg_rect_t){ l.bar_x, l.top, l.bar_w,
			    l.rows * TCLIENT_ROW_H };
			(void)libgScrollbar(st->ui, ID_DIALOG_SCROLL, r,
			    LIBG_SCROLL_VERTICAL, l.rows, l.total,
			    TCLIENT_ARROW_ROWS, &st->dialog_scroll);
		}
		libgTextScale(st->ui, TCLIENT_DIALOG_W + 20, 18,
		    st->selected.kind == MTP_PEER_EMPTY ? "Choose a chat" : "Messages",
		    0xFF20252B, 2);
		y = 58;
		for (i = 0; i < mtpHistoryCount(st->mtp) && y < TCLIENT_HEIGHT - 72; i++) {
			m = mtpHistoryAt(st->mtp, i);
			libgText(st->ui, TCLIENT_DIALOG_W + 20, y, m->author[0] != '\0' ?
			    m->author : (m->out ? "You" : "Unknown"), 0xFF4E5964);
			libgText(st->ui, TCLIENT_DIALOG_W + 120, y, m->text, 0xFF20252B);
			y += 22;
		}
		r = (libg_rect_t){ TCLIENT_DIALOG_W + 16, TCLIENT_HEIGHT - 48,
		    TCLIENT_WIDTH - TCLIENT_DIALOG_W - 130, 32 };
		(void)libgTextField(st->ui, ID_MESSAGE, r, st->message, sizeof(st->message));
		r.x += r.width + 8; r.width = 90;
		if (libgButton(st->ui, ID_SEND, r, "Send") & LIBG_WIDGET_CLICKED) {
			tclient_send(st);
		}
	}
	(void)libgPresent(st->ui);
}

int
tclient_event(tclient_state_t *st, const sprot_event_t *event)
{
	uint32_t	buttons;

	if (event->kind == SPROT_EVENT_SURFACE_CLOSE ||
	    event->kind == SPROT_EVENT_DISCONNECT) {
		st->running = 0;
		return (1);
	}
	tclient_input(st, event);
	if (event->kind != SPROT_EVENT_POINTER_MOTION &&
	    event->kind != SPROT_EVENT_POINTER_ENTER) {
		return (1);
	}
	libgMousePosition(st->ui, NULL, NULL, &buttons);
	return ((buttons & SRAPI_MOUSE_LEFT) != 0);
}

void
tclient_tick(tclient_state_t *st)
{
	int	ret;

	if (st->mtp == NULL) {
		return;
	}
	ret = mtpStep(st->mtp);
	if (ret != MTP_OK) {
		snprintf(st->status, sizeof(st->status), "%s", mtpError(st->mtp));
		return;
	}
	if (!st->dialogs_asked && mtpState(st->mtp) == MTP_STATE_READY &&
	    mtpIsAuthorized(st->mtp) && mtpDialogCount(st->mtp) == 0 &&
	    mtpPendingCount(st->mtp) == 0) {
		if (mtpGetDialogs(st->mtp, MTP_MAX_DIALOGS) == MTP_OK) {
			st->dialogs_asked = 1;
		}
	}
}

int
main(int argc, char **argv, char **envp)
{
	tclient_state_t	st;
	libg_style_t	style;
	sprot_event_t	event;
	int		ret;

	(void)envp;
	personality(0);
	memset(&st, 0, sizeof(st));
	st.kq = -1;
	st.running = 1;
	st.dirty = 1;
	ret = tclient_parse_args(argc, argv, &st);
	if (ret != 0) {
		tclient_usage();
		return (ret > 0 ? 0 : 1);
	}
	tclient_trace_install(&st);
	st.conn = sprot_connect(SPROT_DEFAULT_SERVICE);
	if (st.conn == NULL) {
		termPrint("tclient: cannot connect to SWM / Sprot service\n");
		return (1);
	}
	tclient_trace(&st, "connected to the Sprot service");
	st.surface = sprot_create_surface(st.conn, TCLIENT_WIDTH, TCLIENT_HEIGHT);
	if (st.surface == NULL || tclient_wait_surface(st.conn, st.surface) != 0 ||
	    sprot_set_role(st.surface, SPROT_SURFACE_ROLE_TOPLEVEL, 0, 80, 55) != 0 ||
	    sprot_set_title(st.surface, "Telegram") != 0 ||
	    sprot_set_visible(st.surface, 1) != 0) {
		termPrint("tclient: cannot create window\n");
		sprot_disconnect(st.conn);
		return (1);
	}
	libgDefaultStyle(&style);
	if (libgCreateForTarget(sprot_surface_pixels(st.surface), TCLIENT_WIDTH,
	    TCLIENT_HEIGHT, sprot_surface_stride(st.surface), tclient_present,
	    st.surface, &style, &st.ui) != LIBG_OK) {
		termPrint("tclient: LibG initialization failed\n");
		sprot_destroy_surface(st.surface);
		sprot_disconnect(st.conn);
		return (1);
	}
	st.kq = eventKqueue();
	if (st.kq < 0) {
		termPrint("tclient: cannot open kqueue\n");
		libgDestroy(st.ui);
		sprot_destroy_surface(st.surface);
		sprot_disconnect(st.conn);
		return (1);
	}
	tclient_trace(&st, "window and kqueue %d ready, entering the event loop",
	    st.kq);
	while (st.running) {
		mtp_client_t *old_mtp;
		int old_authorized, old_dialogs, old_history, old_pending;
		int old_state, old_code, old_pwneeded, flood, wait;
		char old_status[MTP_MAX_ERROR];

		wait = TCLIENT_POLL_MS;
		if (st.mtp != NULL) {
			ret = mtpTimeout(st.mtp);
			if (ret >= 0 && ret < wait) {
				wait = ret;
			}
		}
		ret = sprot_poll_event(st.conn, &event, wait);
		if (ret < 0) {
			break;
		}
		if (ret > 0 && tclient_event(&st, &event)) {
			st.dirty = 1;
		}

		old_mtp = st.mtp;
		old_authorized = old_mtp != NULL && mtpIsAuthorized(old_mtp);
		old_dialogs = old_mtp != NULL ? mtpDialogCount(old_mtp) : 0;
		old_history = old_mtp != NULL ? mtpHistoryCount(old_mtp) : 0;
		old_pending = old_mtp != NULL ? mtpPendingCount(old_mtp) : 0;
		old_state = old_mtp != NULL ? mtpState(old_mtp) : MTP_STATE_IDLE;
		old_code = old_mtp != NULL && mtpCodeHashPresent(old_mtp);
		old_pwneeded = tclient_password_bits(old_mtp);
		strncpy(old_status, st.status, sizeof(old_status));
		old_status[sizeof(old_status) - 1] = '\0';
		tclient_tick(&st);
		flood = st.mtp != NULL ? mtpFloodWait(st.mtp) : 0;
		if (flood != st.flood_shown) {
			st.flood_shown = flood;
			st.dirty = 1;
		}
		if (st.mtp != old_mtp ||
		    (st.mtp != NULL &&
		    (mtpIsAuthorized(st.mtp) != old_authorized ||
		    mtpDialogCount(st.mtp) != old_dialogs ||
		    mtpHistoryCount(st.mtp) != old_history ||
		    mtpPendingCount(st.mtp) != old_pending ||
		    mtpCodeHashPresent(st.mtp) != old_code ||
		    tclient_password_bits(st.mtp) != old_pwneeded ||
		    mtpState(st.mtp) != old_state)) ||
	    strncmp(st.status, old_status, sizeof(old_status)) != 0) {
			st.dirty = 1;
		}
		if (st.dirty) {
			tclient_draw(&st);
			st.dirty = 0;
		}
	}
	tclient_trace(&st, "event loop finished, tearing down");
	if (st.mtp != NULL) mtpDestroy(st.mtp);
	if (st.kq >= 0) eventClose(st.kq);
	if (st.ui != NULL) libgDestroy(st.ui);
	if (st.surface != NULL) sprot_destroy_surface(st.surface);
	sprot_disconnect(st.conn);
	return (0);
}
