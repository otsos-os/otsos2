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
$define %type api_ipc_message as native IPC message descriptor
$define %type api_ipc_call as native IPC call descriptor
$define %func ipc_copy_name as function with args const char *, char *
$define %func ipc_handle_alloc as function with args ipc_endpoint_t *, u32
$define %func ipc_handle_get as function with args int
$define %func api_ipc_create as function with args const char *, u32, u32
$define %func api_ipc_connect as function with args const char *, u32
$define %func api_ipc_send as function with args int, const api_ipc_message *
$define %func api_ipc_recv as function with args int, api_ipc_message *, u32
$define %func api_ipc_call as function with args int, api_ipc_call *
$define %func api_ipc_ctl as function with args int, u32, void *

*/

/* !SPACE!

$space %internal ipc_copy_name, ipc_handle_alloc, ipc_handle_get
$space %export api_ipc_create, api_ipc_connect, api_ipc_send
$space %export api_ipc_recv, api_ipc_call, api_ipc_ctl

*/

#include <kernel/api/api.h>
#include <kernel/ipc/ipc.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

static int
ipc_copy_name(const char *user, char *name)
{
	int	i;

	if (!user || !name || !is_user_address(user, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	for (i = 0; i < IPC_NAME_MAX - 1; i++) {
		if (!is_user_address(user + i, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
		name[i] = user[i];
		if (name[i] == '\0') {
			return (0);
		}
	}
	name[0] = '\0';
	return (-API_ERR_TOO_BIG);
}

static int
ipc_handle_alloc(ipc_endpoint_t *endpoint, u32 flags)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	int		object_index;
	int		handle;

	handles = api_get_handle_table();
	objects = api_get_object_table();
	handle = -1;
	for (object_index = 0; object_index < MAX_HANDLES; object_index++) {
		if (!handles[object_index].used) {
			handle = object_index;
			break;
		}
	}
	if (handle < 0) {
		return (-API_ERR_HANDLES_FULL);
	}
	object_index = api_alloc_object();
	if (object_index < 0) {
		return (object_index);
	}
	objects[object_index].type = API_OBJECT_IPC;
	objects[object_index].ipc = endpoint;
	objects[object_index].flags = API_OPEN_RW;
	handles[handle].used = 1;
	handles[handle].flags = API_OPEN_RW | (int)flags;
	handles[handle].object_index = object_index;
	return (handle);
}

static ipc_endpoint_t *
ipc_handle_get(int handle)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	int		object_index;

	handles = api_get_handle_table();
	objects = api_get_object_table();
	if (handle < 0 || handle >= MAX_HANDLES || !handles[handle].used) {
		return (NULL);
	}
	object_index = handles[handle].object_index;
	if (object_index < 0 || object_index >= MAX_DATA_OBJECTS ||
	    !objects[object_index].used ||
	    objects[object_index].type != API_OBJECT_IPC) {
		return (NULL);
	}
	ipc_endpoint_retain((ipc_endpoint_t *)objects[object_index].ipc);
	return ((ipc_endpoint_t *)objects[object_index].ipc);
}

int
api_ipc_create(const char *uname, u32 flags, u32 mode)
{
	ipc_endpoint_t	*endpoint;
	char		name[IPC_NAME_MAX];
	int		error, handle;

	if (!uname || !is_user_address(uname, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	memset(name, 0, sizeof(name));
	if (ipc_copy_name(uname, name) != 0) {
		return (-API_ERR_BAD_ADDR);
	}
	endpoint = ipc_service_create(name, flags, mode, &error);
	if (!endpoint) {
		return (-error);
	}
	handle = ipc_handle_alloc(endpoint, flags);
	if (handle < 0) {
		ipc_endpoint_release(endpoint);
	}
	return (handle);
}

int
api_ipc_connect(const char *uname, u32 flags)
{
	ipc_endpoint_t	*endpoint;
	char		name[IPC_NAME_MAX];
	int		error, handle;

	if (!uname || !is_user_address(uname, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	memset(name, 0, sizeof(name));
	if (ipc_copy_name(uname, name) != 0) {
		return (-API_ERR_BAD_ADDR);
	}
	endpoint = ipc_service_connect(name, flags, &error);
	if (!endpoint) {
		return (-error);
	}
	handle = ipc_handle_alloc(endpoint, flags);
	if (handle < 0) {
		ipc_endpoint_release(endpoint);
	}
	return (handle);
}

int
api_ipc_send(int handle, const struct api_ipc_message *umessage)
{
	struct api_ipc_message	message;
	ipc_endpoint_t		*endpoint;
	int			ret;

	endpoint = ipc_handle_get(handle);
	if (!endpoint) {
		return (-API_ERR_BAD_HANDLE);
	}
	ret = -API_ERR_BAD_ADDR;
	if (!umessage || !is_user_address(umessage, sizeof(message)) ||
	    !user_range_fault_in((void *)umessage, sizeof(message), 0)) {
		goto out;
	}
	memcpy(&message, umessage, sizeof(message));
	if (message.length > 0 &&
	    (!is_user_address(message.data, message.length) ||
	    !user_range_fault_in(message.data, message.length, 0))) {
		goto out;
	}
	ret = ipc_endpoint_send(endpoint, &message, message.data);
out:
	ipc_endpoint_release(endpoint);
	return (ret);
}

int
api_ipc_recv(int handle, struct api_ipc_message *umessage, u32 flags)
{
	struct api_ipc_message	message;
	ipc_endpoint_t		*endpoint;
	int			ret;

	endpoint = ipc_handle_get(handle);
	if (!endpoint) {
		return (-API_ERR_BAD_HANDLE);
	}
	ret = -API_ERR_BAD_ADDR;
	if (!umessage || !is_user_address(umessage, sizeof(message)) ||
	    !user_range_fault_in(umessage, sizeof(message), 1)) {
		goto out;
	}
	memcpy(&message, umessage, sizeof(message));
	if (message.capacity > IPC_MAX_PAYLOAD) {
		message.capacity = IPC_MAX_PAYLOAD;
	}
	if (message.capacity > 0 &&
	    (!is_user_address(message.data, message.capacity) ||
	    !user_range_fault_in(message.data, message.capacity, 1))) {
		goto out;
	}
	ret = ipc_endpoint_recv(endpoint, &message, message.data, flags);
	if (ret >= 0) {
		memcpy(umessage, &message, sizeof(message));
	}
out:
	ipc_endpoint_release(endpoint);
	return (ret);
}

int
api_ipc_call(int handle, struct api_ipc_call *ucall)
{
	struct api_ipc_call	call;
	ipc_endpoint_t		*endpoint;
	int			ret;

	endpoint = ipc_handle_get(handle);
	if (!endpoint) {
		return (-API_ERR_BAD_HANDLE);
	}
	ret = -API_ERR_BAD_ADDR;
	if (!ucall || !is_user_address(ucall, sizeof(call)) ||
	    !user_range_fault_in(ucall, sizeof(call), 1)) {
		goto out;
	}
	memcpy(&call, ucall, sizeof(call));
	if (call.request.length > IPC_MAX_PAYLOAD ||
	    call.reply.capacity > IPC_MAX_PAYLOAD) {
		ret = -API_ERR_TOO_BIG;
		goto out;
	}
	if (call.request.length > 0 &&
	    (!is_user_address(call.request.data, call.request.length) ||
	    !user_range_fault_in(call.request.data,
	    call.request.length, 0))) {
		goto out;
	}
	if (call.reply.capacity > 0 &&
	    (!is_user_address(call.reply.data, call.reply.capacity) ||
	    !user_range_fault_in(call.reply.data,
	    call.reply.capacity, 1))) {
		goto out;
	}
	ret = ipc_endpoint_call(endpoint, &call.request,
	    call.request.data, &call.reply, call.reply.data,
	    call.timeout_ms);
	if (ret >= 0) {
		memcpy(ucall, &call, sizeof(call));
	}
out:
	ipc_endpoint_release(endpoint);
	return (ret);
}

int
api_ipc_ctl(int handle, u32 op, void *uarg)
{
	struct api_ipc_info	info;
	ipc_endpoint_t		*endpoint;
	u32			mode;
	int			ret;

	endpoint = ipc_handle_get(handle);
	if (!endpoint) {
		return (-API_ERR_BAD_HANDLE);
	}
	ret = -API_ERR_NOT_SUPPORTED;
	switch (op) {
	case IPC_CTL_GET_INFO:
		if (!uarg || !is_user_address(uarg, sizeof(info)) ||
		    !user_range_fault_in(uarg, sizeof(info), 1)) {
			ret = -API_ERR_BAD_ADDR;
			break;
		}
		ret = ipc_endpoint_ctl(endpoint, op, &info);
		if (ret == 0) {
			memcpy(uarg, &info, sizeof(info));
		}
		break;
	case IPC_CTL_SET_MODE:
		if (!uarg || !is_user_address(uarg, sizeof(mode)) ||
		    !user_range_fault_in(uarg, sizeof(mode), 0)) {
			ret = -API_ERR_BAD_ADDR;
			break;
		}
		memcpy(&mode, uarg, sizeof(mode));
		ret = ipc_endpoint_ctl(endpoint, op, &mode);
		break;
	case IPC_CTL_DISCONNECT:
		ret = ipc_endpoint_ctl(endpoint, op, NULL);
		break;
	default:
		break;
	}
	ipc_endpoint_release(endpoint);
	return (ret);
}
