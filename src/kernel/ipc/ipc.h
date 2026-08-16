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

/* !DEFINES!

$define %type ipc_endpoint_t as IPC endpoint state
$define %type ipc_service_t as named IPC service state
$define %type ipc_message_t as queued IPC message
$define %type api_ipc_message as native IPC message descriptor
$define %func ipc_init as procedure with args void
$define %func ipc_service_create as function with args const char *, u32, u32, int *
$define %func ipc_service_connect as function with args const char *, u32, int *
$define %func ipc_endpoint_retain as procedure with args ipc_endpoint_t *
$define %func ipc_endpoint_release as procedure with args ipc_endpoint_t *
$define %func ipc_endpoint_send as function with args ipc_endpoint_t *, const api_ipc_message *, const void *
$define %func ipc_endpoint_recv as function with args ipc_endpoint_t *, api_ipc_message *, void *, u32
$define %func ipc_endpoint_call as function with args ipc_endpoint_t *, const api_ipc_message *, const void *, api_ipc_message *, void *, u32
$define %func ipc_endpoint_ctl as function with args ipc_endpoint_t *, u32, void *
$define %func ipc_endpoint_readable as function with args ipc_endpoint_t *
$define %func ipc_endpoint_writable as function with args ipc_endpoint_t *
$define %func ipc_endpoint_hup as function with args ipc_endpoint_t *
$define %func ipc_endpoint_pending as function with args ipc_endpoint_t *
$define %func ipc_timer_tick as procedure with args void

*/

/* !SPACE!

$space %export ipc_init, ipc_service_create, ipc_service_connect
$space %export ipc_endpoint_retain, ipc_endpoint_release
$space %export ipc_endpoint_send, ipc_endpoint_recv, ipc_endpoint_call
$space %export ipc_endpoint_ctl, ipc_endpoint_readable
$space %export ipc_endpoint_writable, ipc_endpoint_hup
$space %export ipc_endpoint_pending, ipc_timer_tick

*/

#ifndef KERNEL_IPC_IPC_H
#define KERNEL_IPC_IPC_H

#include <kernel/api/api.h>
#include <kernel/entity/entity.h>
#include <mlibc/mlibc.h>

#define	IPC_NAME_MAX		48
#define	IPC_MAX_SERVICES	32
#define	IPC_MAX_SESSIONS	64
#define	IPC_QUEUE_MESSAGES	32
#define	IPC_MAX_PAYLOAD		1024
#define	IPC_MAX_HANDLES		8
#define	IPC_MAX_WAITERS	64
#define	IPC_CANCEL_SLOTS	16
#define	IPC_ENDPOINT_SERVER	1
#define	IPC_ENDPOINT_CLIENT	2
#define	IPC_MSG_REQUEST		0x00000001
#define	IPC_MSG_REPLY		0x00000002
#define	IPC_MSG_EVENT		0x00000004
#define	IPC_MSG_NONBLOCK	0x00000008
#define	IPC_MSG_TRUNC		0x00000010
#define	IPC_OPEN_NONBLOCK	0x00000001
#define	IPC_OPEN_EXCLUSIVE	0x00000002
#define	IPC_CTL_GET_INFO	1
#define	IPC_CTL_SET_MODE	2
#define	IPC_CTL_DISCONNECT	3
#define	IPC_STATE_READABLE	0x00000001
#define	IPC_STATE_WRITABLE	0x00000002
#define	IPC_STATE_HUP		0x00000004
#define	IPC_STATE_SERVER	0x00000008
#define	IPC_STATE_CLIENT	0x00000010

struct api_ipc_cred {
	u32	pid;
	u32	uid;
	u32	gid;
	u32	reserved;
};

struct api_ipc_message {
	u64	id;
	u64	reply_to;
	u64	peer;
	u32	opcode;
	u32	flags;
	u32	length;
	u32	capacity;
	void	*data;
	struct api_ipc_cred cred;
	u32	handle_count;
	u32	handle_capacity;
	int	handles[IPC_MAX_HANDLES];
};

struct api_ipc_call {
	struct api_ipc_message	request;
	struct api_ipc_message	reply;
	s64			timeout_ms;
};

struct api_ipc_info {
	u32	state;
	u32	mode;
	u32	pending_messages;
	u32	pending_bytes;
	u32	owner_pid;
	u32	owner_uid;
	u32	owner_gid;
	u32	peer_count;
	u64	peer;
	char	name[IPC_NAME_MAX];
};

typedef struct ipc_message {
	u64	id;
	u64	reply_to;
	u64	peer;
	u32	opcode;
	u32	flags;
	u32	length;
	struct api_ipc_cred cred;
	u32	handle_count;
	struct {
		entity_id_t	id;
		u32		access;
	} handles[IPC_MAX_HANDLES];
	u8	data[IPC_MAX_PAYLOAD];
} ipc_message_t;

struct ipc_service;

typedef struct ipc_endpoint {
	int			used;
	int			refcount;
	int			role;
	int			closed;
	u32			flags;
	u64			peer_id;
	u64			next_id;
	u64			canceled[IPC_CANCEL_SLOTS];
	u32			cancel_pos;
	u32			peer_generation;
	struct ipc_service	*service;
	ipc_message_t		queue[IPC_QUEUE_MESSAGES];
	u32			queue_head;
	u32			queue_tail;
	u32			queue_count;
	u32			queue_bytes;
} ipc_endpoint_t;

typedef struct ipc_service {
	int			used;
	char			name[IPC_NAME_MAX];
	u32			owner_pid;
	u32			owner_uid;
	u32			owner_gid;
	u32			mode;
	u32			flags;
	u64			next_peer_id;
	ipc_endpoint_t		*server;
	ipc_endpoint_t		*clients[IPC_MAX_SESSIONS];
	u32			client_count;
} ipc_service_t;

void		ipc_init(void);
ipc_endpoint_t *ipc_service_create(const char *name, u32 flags,
		    u32 mode, int *error);
ipc_endpoint_t *ipc_service_connect(const char *name, u32 flags,
		    int *error);
void		ipc_endpoint_retain(ipc_endpoint_t *endpoint);
void		ipc_endpoint_release(ipc_endpoint_t *endpoint);
int		ipc_endpoint_send(ipc_endpoint_t *endpoint,
		    const struct api_ipc_message *message,
		    const void *payload);
int		ipc_endpoint_recv(ipc_endpoint_t *endpoint,
		    struct api_ipc_message *message, void *payload,
		    u32 flags);
int		ipc_endpoint_call(ipc_endpoint_t *endpoint,
		    const struct api_ipc_message *request,
		    const void *request_payload,
		    struct api_ipc_message *reply, void *reply_payload,
		    s64 timeout_ms);
int		ipc_endpoint_ctl(ipc_endpoint_t *endpoint, u32 op,
		    void *arg);
int		ipc_endpoint_readable(ipc_endpoint_t *endpoint);
int		ipc_endpoint_writable(ipc_endpoint_t *endpoint);
int		ipc_endpoint_hup(ipc_endpoint_t *endpoint);
u32		ipc_endpoint_pending(ipc_endpoint_t *endpoint);
void		ipc_timer_tick(void);

#endif
