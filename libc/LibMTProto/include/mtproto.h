/* !DEFINES!

$define %type mtp_client as opaque MTProto connection and session state
$define %type mtp_config as connection parameters supplied by the caller
$define %type mtp_peer as resolved conversation identity
$define %type mtp_dialog as one chat list row
$define %type mtp_message as one decoded message
$define %type mtp_reply_cb as callback delivering one RPC result body
$define %func mtpCreate as function with args config, kqueue, out client
$define %func mtpDestroy as procedure with args client
$define %func mtpStep as function with args client
$define %func mtpTimeout as function with args client
$define %func mtpWatchFd as function with args client
$define %func mtpWatchFilter as function with args client
$define %func mtpState as function with args client
$define %func mtpError as function with args client
$define %func mtpIsAuthorized as function with args client
$define %func mtpSelfId as function with args client
$define %func mtpSendCode as function with args client, phone
$define %func mtpSignIn as function with args client, phone, code
$define %func mtpCheckPassword as function with args client, password
$define %func mtpPasswordNeeded as function with args client
$define %func mtpPasswordHint as function with args client
$define %func mtpPasswordBusy as function with args client
$define %func mtpPasswordReady as function with args client
$define %func mtpLogOut as function with args client
$define %func mtpGetDialogs as function with args client, limit
$define %func mtpGetHistory as function with args client, peer, limit, offset id
$define %func mtpSendMessage as function with args client, peer, text
$define %func mtpReadHistory as function with args client, peer, max id
$define %func mtpDialogCount as function with args client
$define %func mtpDialogAt as function with args client, index
$define %func mtpHistoryCount as function with args client
$define %func mtpHistoryAt as function with args client, index
$define %func mtpHistoryPeer as function with args client
$define %func mtpPendingCount as function with args client
$define %func mtpCodeHashPresent as function with args client
$define %func mtpIsReady as function with args client
$define %func mtpDcId as function with args client
$define %func mtpFloodWait as function with args client
$define %func mtpFloodRequest as function with args client
$define %func mtpResetCode as procedure with args client
$define %func mtpReconnect as function with args client
$define %func mtpStrerror as function with args int
$define %func mtpStateName as function with args int
$define %type mtp_log_callback as callback receiving one formatted trace line
$define %func mtpLogSet as procedure with args level, callback, context
$define %func mtpLogEnabled as function with args level

*/

/* !SPACE!

$space %export mtp_client_t, mtp_config_t, mtp_peer_t
$space %export mtp_dialog_t, mtp_message_t
$space %export mtpCreate, mtpDestroy, mtpStep, mtpTimeout
$space %export mtpWatchFd, mtpWatchFilter, mtpState, mtpError
$space %export mtpIsAuthorized, mtpSelfId
$space %export mtpSendCode, mtpSignIn, mtpLogOut
$space %export mtpCheckPassword, mtpPasswordNeeded, mtpPasswordHint
$space %export mtpPasswordBusy, mtpPasswordReady
$space %export mtpGetDialogs, mtpGetHistory, mtpSendMessage, mtpReadHistory
$space %export mtpDialogCount, mtpDialogAt
$space %export mtpHistoryCount, mtpHistoryAt, mtpHistoryPeer
$space %export mtpPendingCount, mtpCodeHashPresent, mtpIsReady, mtpDcId
$space %export mtpFloodWait, mtpFloodRequest, mtpResetCode, mtpReconnect
$space %export mtpStrerror, mtpStateName
$space %export mtp_log_callback, mtpLogSet, mtpLogEnabled

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
 * SUBSTITUTE GOODS OR SERVICES; LOSS, USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef LIBMTPROTO_H
#define LIBMTPROTO_H

#include <stddef.h>
#include <stdint.h>

#define MTP_OK			0
#define MTP_ERR_INVAL		-1
#define MTP_ERR_NOMEM		-2
#define MTP_ERR_NET		-3
#define MTP_ERR_PROTO		-4
#define MTP_ERR_CRYPTO		-5
#define MTP_ERR_AUTH		-6
#define MTP_ERR_RPC		-7
#define MTP_ERR_BUSY		-8
#define MTP_ERR_TIMEOUT		-9
#define MTP_ERR_STORE		-10
#define MTP_ERR_UNSUPPORTED	-11
#define MTP_ERR_FLOOD		-12
#define MTP_ERR_NOTREADY	-13

#define MTP_STATE_IDLE		0
#define MTP_STATE_CONNECT	1
#define MTP_STATE_HANDSHAKE	2
#define MTP_STATE_INIT		3
#define MTP_STATE_READY		4
#define MTP_STATE_FAILED	5

#define MTP_PEER_EMPTY		0
#define MTP_PEER_USER		1
#define MTP_PEER_CHAT		2
#define MTP_PEER_CHANNEL	3

#define MTP_MAX_NAME		128
#define MTP_MAX_DIALOGS	128
#define MTP_MAX_HISTORY	128
#define MTP_MAX_TEXT		4096
#define MTP_MAX_PHONE		32
#define MTP_MAX_ERROR		192

#define MTP_MAX_PASSWORD	128

#define MTP_LOG_NONE		0
#define MTP_LOG_ERROR		1
#define MTP_LOG_INFO		2
#define MTP_LOG_DEBUG		3
#define MTP_LOG_TRACE		4

typedef struct mtp_client mtp_client_t;

typedef struct mtp_config {
	int32_t		api_id;
	const char	*api_hash;
	const char	*device_model;
	const char	*system_version;
	const char	*app_version;
	const char	*lang_code;
	const char	*auth_path;
	int		dc_index;
} mtp_config_t;

typedef struct mtp_peer {
	int32_t		kind;
	int64_t		id;
	int64_t		access_hash;
} mtp_peer_t;

typedef struct mtp_dialog {
	mtp_peer_t	peer;
	char		title[MTP_MAX_NAME];
	char		preview[MTP_MAX_NAME];
	int32_t		top_message;
	int32_t		unread_count;
	int32_t		date;
	int		pinned;
} mtp_dialog_t;

typedef struct mtp_message {
	int32_t		id;
	int32_t		date;
	int		out;
	int		service;
	int64_t		from_id;
	char		author[MTP_MAX_NAME];
	char		text[MTP_MAX_TEXT];
} mtp_message_t;

int	mtpCreate(const mtp_config_t *cfg, int kq, mtp_client_t **out);
void	mtpDestroy(mtp_client_t *c);

int	mtpStep(mtp_client_t *c);

int	mtpTimeout(mtp_client_t *c);

int	mtpWatchFd(const mtp_client_t *c);
int	mtpWatchFilter(const mtp_client_t *c);

int		mtpState(const mtp_client_t *c);
const char	*mtpError(const mtp_client_t *c);
int		mtpIsAuthorized(const mtp_client_t *c);
int64_t		mtpSelfId(const mtp_client_t *c);

int	mtpPendingCount(const mtp_client_t *c);

int	mtpCodeHashPresent(const mtp_client_t *c);

int	mtpIsReady(const mtp_client_t *c);

int	mtpDcId(const mtp_client_t *c);
int		mtpFloodWait(const mtp_client_t *c);
const char	*mtpFloodRequest(const mtp_client_t *c);

void	mtpResetCode(mtp_client_t *c);

int	mtpReconnect(mtp_client_t *c);

int	mtpSendCode(mtp_client_t *c, const char *phone);
int	mtpSignIn(mtp_client_t *c, const char *phone, const char *code);
int	mtpCheckPassword(mtp_client_t *c, const char *password);
int	mtpPasswordNeeded(const mtp_client_t *c);
const char *mtpPasswordHint(const mtp_client_t *c);
int	mtpPasswordBusy(const mtp_client_t *c);
int	mtpPasswordReady(const mtp_client_t *c);
int	mtpLogOut(mtp_client_t *c);

int	mtpGetDialogs(mtp_client_t *c, int limit);
int	mtpGetHistory(mtp_client_t *c, const mtp_peer_t *peer, int limit,
	    int32_t offset_id);
int	mtpSendMessage(mtp_client_t *c, const mtp_peer_t *peer,
	    const char *text);
int	mtpReadHistory(mtp_client_t *c, const mtp_peer_t *peer, int32_t max_id);

int			mtpDialogCount(const mtp_client_t *c);
const mtp_dialog_t	*mtpDialogAt(const mtp_client_t *c, int index);
int			mtpHistoryCount(const mtp_client_t *c);
const mtp_message_t	*mtpHistoryAt(const mtp_client_t *c, int index);
const mtp_peer_t	*mtpHistoryPeer(const mtp_client_t *c);

const char	*mtpStrerror(int err);
const char	*mtpStateName(int state);

typedef void (*mtp_log_callback)(void *ctx, int level, const char *line);

void	mtpLogSet(int level, mtp_log_callback callback, void *ctx);
int	mtpLogEnabled(int level);

#endif
