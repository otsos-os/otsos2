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
$define %type process_t as process control block
$define %type ipc_waiter_t as timed IPC call waiter
$define %func ipc_name_valid as function with args const char *
$define %func ipc_service_find as function with args const char *
$define %func ipc_endpoint_alloc as function with args int
$define %func ipc_endpoint_free as procedure with args ipc_endpoint_t *
$define %func ipc_service_access as function with args ipc_service_t *, process_t *
$define %func ipc_service_client as function with args ipc_service_t *, u64
$define %func ipc_queue_push as function with args ipc_endpoint_t *, const ipc_message_t *, u32
$define %func ipc_queue_pop as function with args endpoint, message, reply, flags
$define %func ipc_queue_unpop as procedure with args endpoint, message
$define %func ipc_message_release_handles as procedure with args ipc_message_t *
$define %func ipc_message_build as function with args ipc_endpoint_t *, ipc_message_t *, const api_ipc_message *, const void *
$define %func ipc_message_copyout as function with args const ipc_message_t *, api_ipc_message *, void *
$define %func ipc_endpoint_target as function with args ipc_endpoint_t *, const ipc_message_t *
$define %func ipc_waiter_alloc as function with args ipc_endpoint_t *, u64
$define %func ipc_waiter_release as procedure with args ipc_waiter_t *
$define %func ipc_cancel_add as procedure with args ipc_endpoint_t *, u64
$define %func ipc_cancel_take as function with args ipc_endpoint_t *, u64
$define %func ipc_init as procedure with args void
$define %func ipc_service_create as function with args const char *, u32, u32, int *
$define %func ipc_service_connect as function with args const char *, u32, int *
$define %func ipc_endpoint_retain as procedure with args ipc_endpoint_t *
$define %func ipc_endpoint_release as procedure with args ipc_endpoint_t *
$define %func ipc_endpoint_send as function with args ipc_endpoint_t *, const api_ipc_message *, const void *
$define %func ipc_endpoint_recv as function with args ipc_endpoint_t *, api_ipc_message *, void *, u32
$define %func ipc_endpoint_call as function with args ipc_endpoint_t *, const api_ipc_message *, const void *, api_ipc_message *, void *, s64
$define %func ipc_endpoint_ctl as function with args ipc_endpoint_t *, u32, void *
$define %func ipc_endpoint_readable as function with args ipc_endpoint_t *
$define %func ipc_endpoint_writable as function with args ipc_endpoint_t *
$define %func ipc_endpoint_hup as function with args ipc_endpoint_t *
$define %func ipc_endpoint_pending as function with args ipc_endpoint_t *
$define %func ipc_timer_tick as procedure with args void

*/

/* !SPACE!

$space %internal ipc_name_valid, ipc_service_find, ipc_endpoint_alloc
$space %internal ipc_endpoint_free, ipc_service_access
$space %internal ipc_service_client, ipc_queue_push, ipc_queue_pop
$space %internal ipc_queue_unpop
$space %internal ipc_message_release_handles
$space %internal ipc_message_build, ipc_message_copyout
$space %internal ipc_endpoint_target, ipc_waiter_alloc
$space %internal ipc_waiter_release, ipc_cancel_add, ipc_cancel_take
$space %export ipc_init, ipc_service_create, ipc_service_connect
$space %export ipc_endpoint_retain, ipc_endpoint_release
$space %export ipc_endpoint_send, ipc_endpoint_recv, ipc_endpoint_call
$space %export ipc_endpoint_ctl, ipc_endpoint_readable
$space %export ipc_endpoint_writable, ipc_endpoint_hup
$space %export ipc_endpoint_pending, ipc_timer_tick

*/

#include <kernel/ipc/ipc.h>
#include <kernel/event/event.h>
#include <kernel/drivers/timer.h>
#include <kernel/process.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

typedef struct ipc_waiter {
	int			used;
	int			expired;
	ipc_endpoint_t		*endpoint;
	u64			deadline;
} ipc_waiter_t;

static ipc_service_t	ipc_services[IPC_MAX_SERVICES];
static ipc_waiter_t	ipc_waiters[IPC_MAX_WAITERS];

static void	ipc_message_release_handles(ipc_message_t *message);

static int
ipc_name_valid(const char *name)
{
	u32	i;
	char	ch;

	if (!name || name[0] == '\0') {
		return (0);
	}
	for (i = 0; i < IPC_NAME_MAX; i++) {
		ch = name[i];
		if (ch == '\0') {
			return (i > 0);
		}
		if ((ch >= 'a' && ch <= 'z') ||
		    (ch >= 'A' && ch <= 'Z') ||
		    (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' ||
		    ch == '-' || ch == '/') {
			continue;
		}
		return (0);
	}
	return (0);
}

static ipc_service_t *
ipc_service_find(const char *name)
{
	int	i;

	for (i = 0; i < IPC_MAX_SERVICES; i++) {
		if (ipc_services[i].used &&
		    strcmp(ipc_services[i].name, name) == 0) {
			return (&ipc_services[i]);
		}
	}
	return (NULL);
}

static ipc_endpoint_t *
ipc_endpoint_alloc(int role)
{
	ipc_endpoint_t	*endpoint;

	endpoint = kmem_alloc(sizeof(*endpoint));
	if (!endpoint) {
		return (NULL);
	}
	memset(endpoint, 0, sizeof(*endpoint));
	endpoint->used = 1;
	endpoint->refcount = 1;
	endpoint->role = role;
	endpoint->next_id = 1;
	return (endpoint);
}

static void
ipc_endpoint_free(ipc_endpoint_t *endpoint)
{
	u32	i;

	if (!endpoint) {
		return;
	}
	for (i = 0; i < endpoint->queue_count; i++) {
		u32	index;

		index = (endpoint->queue_head + i) % IPC_QUEUE_MESSAGES;
		ipc_message_release_handles(&endpoint->queue[index]);
	}
	memset(endpoint, 0, sizeof(*endpoint));
	kmem_free(endpoint);
}

static int
ipc_service_access(ipc_service_t *service, process_t *proc)
{
	u32	bits;

	if (!service || !proc) {
		return (0);
	}
	if (proc_has_privilege(proc) || proc->euid == 0) {
		return (1);
	}
	if (proc->euid == service->owner_uid) {
		bits = (service->mode >> 6) & 7;
	} else if (proc->egid == service->owner_gid) {
		bits = (service->mode >> 3) & 7;
	} else {
		bits = service->mode & 7;
	}
	return ((bits & 6) == 6);
}

static ipc_endpoint_t *
ipc_service_client(ipc_service_t *service, u64 peer)
{
	int	i;

	if (!service || peer == 0) {
		return (NULL);
	}
	for (i = 0; i < IPC_MAX_SESSIONS; i++) {
		if (service->clients[i] &&
		    service->clients[i]->peer_id == peer &&
		    !service->clients[i]->closed) {
			return (service->clients[i]);
		}
	}
	return (NULL);
}

static int
ipc_queue_push(ipc_endpoint_t *endpoint, const ipc_message_t *message,
    u32 flags)
{
	int	nonblock;

	nonblock = (flags & IPC_MSG_NONBLOCK) != 0 ||
	    (endpoint->flags & IPC_OPEN_NONBLOCK) != 0;
	while (endpoint->queue_count >= IPC_QUEUE_MESSAGES) {
		if (endpoint->closed) {
			return (-API_ERR_PIPE_CLOSED);
		}
		if (nonblock) {
			return (-API_ERR_RETRY);
		}
		proc_sleep(endpoint);
	}
	endpoint->queue[endpoint->queue_tail] = *message;
	endpoint->queue_tail = (endpoint->queue_tail + 1) %
	    IPC_QUEUE_MESSAGES;
	endpoint->queue_count++;
	endpoint->queue_bytes += message->length;
	proc_wakeup(endpoint);
	event_notify_ipc_change(endpoint);
	return (0);
}

static int
ipc_queue_pop(ipc_endpoint_t *endpoint, ipc_message_t *message,
    u64 reply_to, u32 flags)
{
	u32	count, i, slot, next;
	int	found, nonblock;

	nonblock = (flags & IPC_MSG_NONBLOCK) != 0 ||
	    (endpoint->flags & IPC_OPEN_NONBLOCK) != 0;
	for (;;) {
		found = 0;
		count = endpoint->queue_count;
		for (i = 0; i < count; i++) {
			slot = (endpoint->queue_head + i) % IPC_QUEUE_MESSAGES;
			if (reply_to != 0 &&
			    endpoint->queue[slot].reply_to != reply_to) {
				continue;
			}
			*message = endpoint->queue[slot];
			found = 1;
			break;
		}
		if (found) {
			if (i == 0) {
				endpoint->queue_head =
				    (endpoint->queue_head + 1) %
				    IPC_QUEUE_MESSAGES;
			} else {
				for (; i + 1 < count; i++) {
					slot = (endpoint->queue_head + i) %
					    IPC_QUEUE_MESSAGES;
					next = (slot + 1) %
					    IPC_QUEUE_MESSAGES;
					endpoint->queue[slot] =
					    endpoint->queue[next];
				}
				endpoint->queue_tail = (endpoint->queue_tail +
				    IPC_QUEUE_MESSAGES - 1) %
				    IPC_QUEUE_MESSAGES;
			}
			endpoint->queue_count--;
			if (endpoint->queue_bytes >= message->length) {
				endpoint->queue_bytes -= message->length;
			} else {
				endpoint->queue_bytes = 0;
			}
			proc_wakeup(endpoint);
			event_notify_ipc_change(endpoint);
			return (0);
		}
		if (endpoint->closed) {
			return (-API_ERR_PIPE_CLOSED);
		}
		if (nonblock) {
			return (-API_ERR_RETRY);
		}
		proc_sleep(endpoint);
	}
}

static void
ipc_queue_unpop(ipc_endpoint_t *endpoint, const ipc_message_t *message)
{
	endpoint->queue_head = (endpoint->queue_head +
	    IPC_QUEUE_MESSAGES - 1) % IPC_QUEUE_MESSAGES;
	endpoint->queue[endpoint->queue_head] = *message;
	endpoint->queue_count++;
	endpoint->queue_bytes += message->length;
	event_notify_ipc_change(endpoint);
}

static void
ipc_message_release_handles(ipc_message_t *message)
{
	u32	i;

	if (!message) {
		return;
	}
	for (i = 0; i < message->handle_count; i++) {
		if (message->handles[i].id != 0) {
			entity_release(message->handles[i].id);
			message->handles[i].id = 0;
		}
	}
	message->handle_count = 0;
}

static int
ipc_message_build(ipc_endpoint_t *endpoint, ipc_message_t *out,
    const struct api_ipc_message *message, const void *payload)
{
	process_t	*proc;
	entity_id_t	id;
	u32		access, i;
	int		ret;

	if (!endpoint || !out || !message ||
	    message->length > IPC_MAX_PAYLOAD ||
	    message->handle_count > IPC_MAX_HANDLES) {
		return (message != NULL &&
		    message->handle_count > IPC_MAX_HANDLES ?
		    -API_ERR_TOO_BIG : -API_ERR_INVAL);
	}
	if (message->length > 0 && !payload) {
		return (-API_ERR_BAD_ADDR);
	}
	memset(out, 0, sizeof(*out));
	out->id = message->id != 0 ? message->id : endpoint->next_id++;
	if (endpoint->next_id == 0) {
		endpoint->next_id = 1;
	}
	out->reply_to = message->reply_to;
	out->peer = endpoint->role == IPC_ENDPOINT_CLIENT ?
	    endpoint->peer_id : message->peer;
	out->opcode = message->opcode;
	out->flags = message->flags;
	out->length = message->length;
	proc = process_current();
	if (proc) {
		out->cred.pid = proc->pid;
		out->cred.uid = proc_has_privilege(proc) ? 0 : proc->euid;
		out->cred.gid = proc_has_privilege(proc) ? 0 : proc->egid;
	}
	if (out->length > 0) {
		memcpy(out->data, payload, out->length);
	}
	for (i = 0; i < message->handle_count; i++) {
		if (!proc) {
			ipc_message_release_handles(out);
			return (-API_ERR_BAD_HANDLE);
		}
		ret = entity_handle_lookup(proc, message->handles[i], &id,
		    &access);
		if (ret != 0) {
			ipc_message_release_handles(out);
			return (ret);
		}
		out->handles[i].id = id;
		out->handles[i].access = access;
		entity_retain(id);
		out->handle_count++;
	}
	return (0);
}

static int
ipc_message_copyout(const ipc_message_t *source,
    struct api_ipc_message *message, void *payload)
{
	process_t	*proc;
	u32	copy_len;
	u32	i;
	int	installed[IPC_MAX_HANDLES];
	int	ret;

	if (!source || !message) {
		return (-API_ERR_INVAL);
	}
	copy_len = source->length;
	if (copy_len > message->capacity) {
		if ((message->flags & IPC_MSG_TRUNC) == 0) {
			return (-API_ERR_TOO_BIG);
		}
		copy_len = message->capacity;
	}
	if (copy_len > 0 && !payload) {
		return (-API_ERR_BAD_ADDR);
	}
	if (source->handle_count > message->handle_capacity) {
		return (-API_ERR_TOO_BIG);
	}
	if (source->handle_count > 0 && !process_current()) {
		return (-API_ERR_BAD_HANDLE);
	}
	memset(installed, 0, sizeof(installed));
	proc = process_current();
	for (i = 0; i < source->handle_count; i++) {
		ret = entity_handle_alloc(proc, source->handles[i].id,
		    source->handles[i].access);
		if (ret < 0) {
			while (i > 0) {
				i--;
				if (installed[i] != 0) {
					(void)entity_handle_free(proc, installed[i]);
				}
			}
			return (ret);
		}
		installed[i] = ret;
	}
	message->id = source->id;
	message->reply_to = source->reply_to;
	message->peer = source->peer;
	message->opcode = source->opcode;
	message->flags = source->flags;
	message->length = source->length;
	message->cred = source->cred;
	message->handle_count = source->handle_count;
	for (i = 0; i < source->handle_count; i++) {
		message->handles[i] = installed[i];
	}
	if (copy_len > 0) {
		memcpy(payload, source->data, copy_len);
	}
	return ((int)copy_len);
}

static ipc_endpoint_t *
ipc_endpoint_target(ipc_endpoint_t *endpoint, const ipc_message_t *message)
{
	if (endpoint->role == IPC_ENDPOINT_CLIENT) {
		return (endpoint->service->server);
	}
	return (ipc_service_client(endpoint->service, message->peer));
}

static ipc_waiter_t *
ipc_waiter_alloc(ipc_endpoint_t *endpoint, u64 deadline)
{
	int	i;

	for (i = 0; i < IPC_MAX_WAITERS; i++) {
		if (!ipc_waiters[i].used) {
			memset(&ipc_waiters[i], 0, sizeof(ipc_waiters[i]));
			ipc_waiters[i].used = 1;
			ipc_waiters[i].endpoint = endpoint;
			ipc_waiters[i].deadline = deadline;
			return (&ipc_waiters[i]);
		}
	}
	return (NULL);
}

static void
ipc_waiter_release(ipc_waiter_t *waiter)
{
	if (waiter) {
		memset(waiter, 0, sizeof(*waiter));
	}
}

static void
ipc_cancel_add(ipc_endpoint_t *endpoint, u64 id)
{
	if (!endpoint || id == 0) {
		return;
	}
	endpoint->canceled[endpoint->cancel_pos] = id;
	endpoint->cancel_pos = (endpoint->cancel_pos + 1) % IPC_CANCEL_SLOTS;
}

static int
ipc_cancel_take(ipc_endpoint_t *endpoint, u64 id)
{
	int	i;

	if (!endpoint || id == 0) {
		return (0);
	}
	for (i = 0; i < IPC_CANCEL_SLOTS; i++) {
		if (endpoint->canceled[i] == id) {
			endpoint->canceled[i] = 0;
			return (1);
		}
	}
	return (0);
}

void
ipc_init(void)
{
	memset(ipc_services, 0, sizeof(ipc_services));
	memset(ipc_waiters, 0, sizeof(ipc_waiters));
	printk("[IPC] initialized: %d services, %d sessions/service\n",
	    IPC_MAX_SERVICES, IPC_MAX_SESSIONS);
}

ipc_endpoint_t *
ipc_service_create(const char *name, u32 flags, u32 mode, int *error)
{
	ipc_endpoint_t	*endpoint;
	ipc_service_t	*service;
	process_t	*proc;
	int		i;

	if (error) {
		*error = API_ERR_INVAL;
	}
	if (!ipc_name_valid(name)) {
		return (NULL);
	}
	if (ipc_service_find(name)) {
		if (error) {
			*error = API_ERR_EXISTS;
		}
		return (NULL);
	}
	service = NULL;
	for (i = 0; i < IPC_MAX_SERVICES; i++) {
		if (!ipc_services[i].used) {
			service = &ipc_services[i];
			break;
		}
	}
	if (!service) {
		if (error) {
			*error = API_ERR_NO_SPACE;
		}
		return (NULL);
	}
	endpoint = ipc_endpoint_alloc(IPC_ENDPOINT_SERVER);
	if (!endpoint) {
		if (error) {
			*error = API_ERR_NO_MEMORY;
		}
		return (NULL);
	}
	proc = process_current();
	memset(service, 0, sizeof(*service));
	service->used = 1;
	strncpy(service->name, name, sizeof(service->name) - 1);
	service->owner_pid = proc ? proc->pid : 0;
	service->owner_uid = proc ? proc->euid : 0;
	service->owner_gid = proc ? proc->egid : 0;
	service->mode = mode & 0777;
	service->flags = flags;
	service->next_peer_id = 1;
	service->server = endpoint;
	endpoint->service = service;
	endpoint->flags = flags;
	if (error) {
		*error = 0;
	}
	return (endpoint);
}

ipc_endpoint_t *
ipc_service_connect(const char *name, u32 flags, int *error)
{
	ipc_endpoint_t	*endpoint;
	ipc_service_t	*service;
	process_t	*proc;
	int		i;

	if (error) {
		*error = API_ERR_NOT_FOUND;
	}
	if (!ipc_name_valid(name)) {
		if (error) {
			*error = API_ERR_INVAL;
		}
		return (NULL);
	}
	service = ipc_service_find(name);
	if (!service || !service->server || service->server->closed) {
		return (NULL);
	}
	proc = process_current();
	if (!ipc_service_access(service, proc)) {
		if (error) {
			*error = API_ERR_ACCESS;
		}
		return (NULL);
	}
	for (i = 0; i < IPC_MAX_SESSIONS; i++) {
		if (!service->clients[i]) {
			break;
		}
	}
	if (i == IPC_MAX_SESSIONS) {
		if (error) {
			*error = API_ERR_BUSY;
		}
		return (NULL);
	}
	endpoint = ipc_endpoint_alloc(IPC_ENDPOINT_CLIENT);
	if (!endpoint) {
		if (error) {
			*error = API_ERR_NO_MEMORY;
		}
		return (NULL);
	}
	endpoint->service = service;
	endpoint->flags = flags;
	endpoint->peer_id = service->next_peer_id++;
	if (service->next_peer_id == 0) {
		service->next_peer_id = 1;
	}
	service->clients[i] = endpoint;
	service->client_count++;
	service->server->peer_generation++;
	event_notify_ipc_change(service->server);
	if (error) {
		*error = 0;
	}
	return (endpoint);
}

void
ipc_endpoint_retain(ipc_endpoint_t *endpoint)
{
	if (endpoint && endpoint->used) {
		endpoint->refcount++;
	}
}

void
ipc_endpoint_release(ipc_endpoint_t *endpoint)
{
	ipc_service_t	*service;
	int		i;

	if (!endpoint || !endpoint->used) {
		return;
	}
	endpoint->refcount--;
	if (endpoint->refcount > 0) {
		return;
	}
	service = endpoint->service;
	endpoint->closed = 1;
	proc_wakeup(endpoint);
	event_notify_ipc_change(endpoint);
	if (service && endpoint->role == IPC_ENDPOINT_SERVER) {
		service->server = NULL;
		service->used = 0;
		for (i = 0; i < IPC_MAX_SESSIONS; i++) {
			if (!service->clients[i]) {
				continue;
			}
			service->clients[i]->closed = 1;
			service->clients[i]->service = NULL;
			proc_wakeup(service->clients[i]);
			event_notify_ipc_change(service->clients[i]);
			service->clients[i] = NULL;
		}
		service->client_count = 0;
	} else if (service) {
		for (i = 0; i < IPC_MAX_SESSIONS; i++) {
			if (service->clients[i] == endpoint) {
				service->clients[i] = NULL;
				if (service->client_count > 0) {
					service->client_count--;
				}
				break;
			}
		}
		if (service->server) {
			service->server->peer_generation++;
			event_notify_ipc_change(service->server);
		}
	}
	ipc_endpoint_free(endpoint);
}

int
ipc_endpoint_send(ipc_endpoint_t *endpoint,
    const struct api_ipc_message *message, const void *payload)
{
	ipc_endpoint_t	*target;
	ipc_message_t	queued;
	int		ret;

	if (!endpoint || endpoint->closed || !endpoint->service) {
		return (-API_ERR_PIPE_CLOSED);
	}
	ret = ipc_message_build(endpoint, &queued, message, payload);
	if (ret != 0) {
		return (ret);
	}
	target = ipc_endpoint_target(endpoint, &queued);
	if (!target || target->closed) {
		ipc_message_release_handles(&queued);
		return (-API_ERR_NOT_FOUND);
	}
	if (queued.reply_to != 0 &&
	    ipc_cancel_take(target, queued.reply_to)) {
		ipc_message_release_handles(&queued);
		return (0);
	}
	ret = ipc_queue_push(target, &queued, message->flags);
	if (ret != 0) {
		ipc_message_release_handles(&queued);
	}
	return (ret);
}

int
ipc_endpoint_recv(ipc_endpoint_t *endpoint,
    struct api_ipc_message *message, void *payload, u32 flags)
{
	ipc_message_t	queued;
	int		ret;

	if (!endpoint || !message) {
		return (-API_ERR_INVAL);
	}
	ret = ipc_queue_pop(endpoint, &queued, 0, flags | message->flags);
	if (ret != 0) {
		return (ret);
	}
	ret = ipc_message_copyout(&queued, message, payload);
	if (ret == -API_ERR_TOO_BIG) {
		ipc_queue_unpop(endpoint, &queued);
	} else {
		ipc_message_release_handles(&queued);
	}
	return (ret);
}

int
ipc_endpoint_call(ipc_endpoint_t *endpoint,
    const struct api_ipc_message *request, const void *request_payload,
    struct api_ipc_message *reply, void *reply_payload, s64 timeout_ms)
{
	ipc_message_t	queued;
	ipc_message_t	out;
	ipc_waiter_t	*waiter;
	u64		deadline, request_id, ticks;
	int		ret;

	if (!endpoint || endpoint->role != IPC_ENDPOINT_CLIENT ||
	    endpoint->closed || endpoint->service == NULL ||
	    endpoint->service->server == NULL || !request || !reply) {
		return (-API_ERR_INVAL);
	}
	ret = ipc_message_build(endpoint, &out, request, request_payload);
	if (ret != 0) {
		return (ret);
	}
	out.flags |= IPC_MSG_REQUEST;
	request_id = out.id;
	ret = ipc_queue_push(endpoint->service->server, &out, request->flags);
	if (ret != 0) {
		ipc_message_release_handles(&out);
		return (ret);
	}
	if (timeout_ms < 0) {
		ret = ipc_queue_pop(endpoint, &queued, request_id,
		    reply->flags & ~IPC_MSG_NONBLOCK);
		if (ret != 0) {
			return (ret);
		}
		ret = ipc_message_copyout(&queued, reply, reply_payload);
		if (ret == -API_ERR_TOO_BIG) {
			ipc_queue_unpop(endpoint, &queued);
		} else {
			ipc_message_release_handles(&queued);
		}
		return (ret);
	}
	if (timeout_ms == 0) {
		ret = ipc_queue_pop(endpoint, &queued, request_id,
		    reply->flags | IPC_MSG_NONBLOCK);
		if (ret != 0) {
			if (ret == -API_ERR_RETRY) {
				ipc_cancel_add(endpoint, request_id);
			}
			return (ret);
		}
		ret = ipc_message_copyout(&queued, reply, reply_payload);
		if (ret == -API_ERR_TOO_BIG) {
			ipc_queue_unpop(endpoint, &queued);
		} else {
			ipc_message_release_handles(&queued);
		}
		return (ret);
	}
	ticks = (u64)timeout_ms * timer_get_frequency() / 1000;
	if (ticks == 0) {
		ticks = 1;
	}
	deadline = timer_get_ticks() + ticks;
	waiter = ipc_waiter_alloc(endpoint, deadline);
	if (!waiter) {
		ipc_cancel_add(endpoint, request_id);
		return (-API_ERR_BUSY);
	}
	for (;;) {
		ret = ipc_queue_pop(endpoint, &queued, request_id,
		    reply->flags | IPC_MSG_NONBLOCK);
		if (ret == 0) {
			ipc_waiter_release(waiter);
			ret = ipc_message_copyout(&queued, reply,
			    reply_payload);
			if (ret == -API_ERR_TOO_BIG) {
				ipc_queue_unpop(endpoint, &queued);
			} else {
				ipc_message_release_handles(&queued);
			}
			return (ret);
		}
		if (ret != -API_ERR_RETRY) {
			ipc_waiter_release(waiter);
			return (ret);
		}
		if (waiter->expired || timer_get_ticks() >= deadline) {
			ipc_cancel_add(endpoint, request_id);
			ipc_waiter_release(waiter);
			return (-API_ERR_TIMED_OUT);
		}
		proc_sleep(endpoint);
	}
}

int
ipc_endpoint_ctl(ipc_endpoint_t *endpoint, u32 op, void *arg)
{
	struct api_ipc_info	*info;
	ipc_service_t		*service;
	process_t		*proc;

	if (!endpoint || !endpoint->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	service = endpoint->service;
	switch (op) {
	case IPC_CTL_GET_INFO:
		info = (struct api_ipc_info *)arg;
		if (!info) {
			return (-API_ERR_BAD_ADDR);
		}
		memset(info, 0, sizeof(*info));
		info->state = ipc_endpoint_hup(endpoint) ? IPC_STATE_HUP : 0;
		if (ipc_endpoint_readable(endpoint)) {
			info->state |= IPC_STATE_READABLE;
		}
		if (ipc_endpoint_writable(endpoint)) {
			info->state |= IPC_STATE_WRITABLE;
		}
		info->state |= endpoint->role == IPC_ENDPOINT_SERVER ?
		    IPC_STATE_SERVER : IPC_STATE_CLIENT;
		info->pending_messages = endpoint->queue_count;
		info->pending_bytes = endpoint->queue_bytes;
		info->peer = endpoint->peer_id;
		if (service) {
			info->mode = service->mode;
			info->owner_pid = service->owner_pid;
			info->owner_uid = service->owner_uid;
			info->owner_gid = service->owner_gid;
			info->peer_count = service->client_count;
			strncpy(info->name, service->name,
			    sizeof(info->name) - 1);
		}
		return (0);
	case IPC_CTL_SET_MODE:
		proc = process_current();
		if (!service || endpoint->role != IPC_ENDPOINT_SERVER ||
		    (!proc_has_privilege(proc) &&
		    proc->euid != service->owner_uid)) {
			return (-API_ERR_PERM);
		}
		service->mode = *(u32 *)arg & 0777;
		return (0);
	case IPC_CTL_DISCONNECT:
		endpoint->closed = 1;
		proc_wakeup(endpoint);
		event_notify_ipc_change(endpoint);
		return (0);
	default:
		return (-API_ERR_NOT_SUPPORTED);
	}
}

int
ipc_endpoint_readable(ipc_endpoint_t *endpoint)
{
	return (endpoint && endpoint->queue_count > 0);
}

int
ipc_endpoint_writable(ipc_endpoint_t *endpoint)
{
	ipc_endpoint_t	*target;

	if (!endpoint || endpoint->closed || !endpoint->service) {
		return (0);
	}
	if (endpoint->role == IPC_ENDPOINT_CLIENT) {
		target = endpoint->service->server;
		return (target && !target->closed &&
		    target->queue_count < IPC_QUEUE_MESSAGES);
	}
	return (endpoint->service->client_count > 0);
}

int
ipc_endpoint_hup(ipc_endpoint_t *endpoint)
{
	return (!endpoint || endpoint->closed || !endpoint->service ||
	    (endpoint->role == IPC_ENDPOINT_CLIENT &&
	    !endpoint->service->server));
}

u32
ipc_endpoint_pending(ipc_endpoint_t *endpoint)
{
	return (endpoint ? endpoint->queue_count : 0);
}

void
ipc_timer_tick(void)
{
	u64	now;
	int	i;

	now = timer_get_ticks();
	for (i = 0; i < IPC_MAX_WAITERS; i++) {
		if (!ipc_waiters[i].used || ipc_waiters[i].expired ||
		    now < ipc_waiters[i].deadline) {
			continue;
		}
		ipc_waiters[i].expired = 1;
		proc_wakeup(ipc_waiters[i].endpoint);
	}
}
