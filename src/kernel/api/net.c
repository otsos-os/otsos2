/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type api_handle_t as struct with process handle state
$define %type api_object_t as struct with global API object state
$define %type api_net_addr as native userspace network address
$define %type api_net_msg as native userspace network message descriptor
$define %type net_endpoint_t as native network endpoint state
$define %type net_endpoint_addr_t as endpoint IPv4 address tuple

$define %func api_net_find_free_handle as function with args void
$define %func api_net_get_endpoint as function with args int, net_endpoint_t **
$define %func api_net_install_endpoint as function with args net_endpoint_t *
$define %func api_net_from_user_addr as function with args const api_net_addr *, net_endpoint_addr_t *
$define %func api_net_to_user_addr as function with args api_net_addr *, const net_endpoint_addr_t *
$define %func api_net_ctl_privileged as function with args int
$define %func api_net_open as function with args int, int, u32
$define %func api_net_bind as function with args int, const api_net_addr *
$define %func api_net_connect as function with args int, const api_net_addr *
$define %func api_net_listen as function with args int, int
$define %func api_net_accept as function with args int, api_net_addr *, u32
$define %func api_net_send as function with args int, const api_net_msg *
$define %func api_net_recv as function with args int, api_net_msg *
$define %func api_net_ctl as function with args int, int, void *

*/

/* !SPACE!

$space %internal api_net_find_free_handle, api_net_get_endpoint
$space %internal api_net_install_endpoint
$space %internal api_net_from_user_addr, api_net_to_user_addr
$space %internal api_net_ctl_privileged
$space %export api_net_open, api_net_bind, api_net_connect
$space %export api_net_listen, api_net_accept
$space %export api_net_send, api_net_recv, api_net_ctl

*/

#include <kernel/api/api.h>
#include <kernel/net/endpoint.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

static int
api_net_find_free_handle(void)
{
	api_handle_t	*handles;
	int		i;

	handles = api_get_handle_table();
	for (i = 3; i < MAX_HANDLES; i++) {
		if (!handles[i].used) {
			return (i);
		}
	}
	return (-API_ERR_HANDLES_FULL);
}

static int
api_net_get_endpoint(int handle, net_endpoint_t **out_ep)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	api_object_t	*obj;
	int		object_index;

	if (!out_ep) {
		return (-API_ERR_BAD_ADDR);
	}
	*out_ep = NULL;

	if (handle < 0 || handle >= MAX_HANDLES) {
		return (-API_ERR_BAD_HANDLE);
	}

	handles = api_get_handle_table();
	if (!handles[handle].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	objects = api_get_object_table();
	object_index = handles[handle].object_index;
	if (object_index < 0 || object_index >= MAX_DATA_OBJECTS ||
	    !objects[object_index].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	obj = &objects[object_index];
	if (obj->type != API_OBJECT_NET || !obj->net) {
		return (-API_ERR_BAD_HANDLE);
	}

	*out_ep = (net_endpoint_t *)obj->net;
	return (0);
}

static int
api_net_install_endpoint(net_endpoint_t *ep)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	int		handle, object_index;

	if (!ep) {
		return (-API_ERR_BAD_HANDLE);
	}

	handle = api_net_find_free_handle();
	if (handle < 0) {
		return (handle);
	}

	object_index = api_alloc_object();
	if (object_index < 0) {
		return (object_index);
	}

	handles = api_get_handle_table();
	objects = api_get_object_table();

	objects[object_index].type = API_OBJECT_NET;
	objects[object_index].flags = API_OPEN_RW;
	objects[object_index].net = ep;

	handles[handle].used = 1;
	handles[handle].flags = API_OPEN_RW;
	handles[handle].object_index = object_index;
	return (handle);
}

static int
api_net_from_user_addr(const struct api_net_addr *uaddr,
    net_endpoint_addr_t *addr)
{
	struct api_net_addr	tmp;

	if (!uaddr || !addr) {
		return (-API_ERR_BAD_ADDR);
	}
	if (!is_user_address(uaddr, sizeof(*uaddr)) ||
	    !user_range_fault_in(uaddr, sizeof(*uaddr), 0)) {
		return (-API_ERR_BAD_ADDR);
	}

	memcpy(&tmp, uaddr, sizeof(tmp));
	if (tmp.family != API_NET_ADDR_IP4) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (tmp.port > 65535) {
		return (-API_ERR_BAD_VALUE);
	}
	if (tmp.ifindex > 0x7FFFFFFFu) {
		return (-API_ERR_BAD_VALUE);
	}

	addr->ip = tmp.ip;
	addr->port = (u16)tmp.port;
	addr->family = NET_ENDPOINT_ADDR_IP4;
	addr->ifindex = tmp.ifindex == 0 ?
	    NET_ENDPOINT_IF_AUTO : (int)tmp.ifindex - 1;
	return (0);
}

static int
api_net_to_user_addr(struct api_net_addr *uaddr,
    const net_endpoint_addr_t *addr)
{
	struct api_net_addr	tmp;

	if (!uaddr || !addr) {
		return (-API_ERR_BAD_ADDR);
	}
	if (!is_user_address(uaddr, sizeof(*uaddr)) ||
	    !user_range_fault_in(uaddr, sizeof(*uaddr), 1)) {
		return (-API_ERR_BAD_ADDR);
	}

	tmp.family = API_NET_ADDR_IP4;
	tmp.port = addr->port;
	tmp.ip = addr->ip;
	tmp.ifindex = addr->ifindex == NET_ENDPOINT_IF_AUTO ?
	    0 : (u32)(addr->ifindex + 1);
	memcpy(uaddr, &tmp, sizeof(tmp));
	return (0);
}

static int
api_net_ctl_privileged(int op)
{
	return (op >= API_NET_CTL_PRIV_BASE);
}

int
api_net_open(int proto, int mode, u32 flags)
{
	net_endpoint_t	*ep;
	int		handle, kproto, kmode;

	if (!((proto == API_NET_PROTO_UDP &&
	    mode == API_NET_MODE_DGRAM) ||
	    (proto == API_NET_PROTO_TCP &&
	    mode == API_NET_MODE_STREAM))) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (flags & ~API_NET_OPEN_NONBLOCK) {
		return (-API_ERR_BAD_VALUE);
	}

	kproto = NET_ENDPOINT_PROTO_UDP;
	kmode = NET_ENDPOINT_MODE_DGRAM;
	if (proto == API_NET_PROTO_TCP) {
		kproto = NET_ENDPOINT_PROTO_TCP;
		kmode = NET_ENDPOINT_MODE_STREAM;
	}

	ep = net_endpoint_open(kproto, kmode, flags);
	if (!ep) {
		return (-API_ERR_NO_MEMORY);
	}

	handle = api_net_install_endpoint(ep);
	if (handle < 0) {
		net_endpoint_close(ep);
		return (handle);
	}
	return (handle);
}

int
api_net_bind(int handle, const struct api_net_addr *uaddr)
{
	net_endpoint_t		*ep;
	net_endpoint_addr_t	addr;
	int			ret;

	ret = api_net_get_endpoint(handle, &ep);
	if (ret != 0) {
		return (ret);
	}
	ret = api_net_from_user_addr(uaddr, &addr);
	if (ret != 0) {
		return (ret);
	}
	return (net_endpoint_bind(ep, &addr));
}

int
api_net_connect(int handle, const struct api_net_addr *uaddr)
{
	net_endpoint_t		*ep;
	net_endpoint_addr_t	addr;
	int			ret;

	ret = api_net_get_endpoint(handle, &ep);
	if (ret != 0) {
		return (ret);
	}
	ret = api_net_from_user_addr(uaddr, &addr);
	if (ret != 0) {
		return (ret);
	}
	return (net_endpoint_connect(ep, &addr));
}

int
api_net_listen(int handle, int backlog)
{
	net_endpoint_t	*ep;
	int		ret;

	ret = api_net_get_endpoint(handle, &ep);
	if (ret != 0) {
		return (ret);
	}
	return (net_endpoint_listen(ep, backlog));
}

int
api_net_accept(int handle, struct api_net_addr *uaddr, u32 flags)
{
	net_endpoint_t		*ep;
	net_endpoint_t		*child;
	net_endpoint_addr_t	addr;
	int			ret, child_handle;

	ret = api_net_get_endpoint(handle, &ep);
	if (ret != 0) {
		return (ret);
	}
	if (flags & ~API_NET_MSG_NONBLOCK) {
		return (-API_ERR_BAD_VALUE);
	}
	if (uaddr && (!is_user_address(uaddr, sizeof(*uaddr)) ||
	    !user_range_fault_in(uaddr, sizeof(*uaddr), 1))) {
		return (-API_ERR_BAD_ADDR);
	}

	child = NULL;
	ret = net_endpoint_accept(ep, &child, uaddr ? &addr : NULL,
	    flags);
	if (ret != 0) {
		return (ret);
	}

	child_handle = api_net_install_endpoint(child);
	if (child_handle < 0) {
		net_endpoint_close(child);
		return (child_handle);
	}
	if (uaddr) {
		ret = api_net_to_user_addr(uaddr, &addr);
		if (ret != 0) {
			api_data_close(child_handle);
			return (ret);
		}
	}
	return (child_handle);
}

int
api_net_send(int handle, const struct api_net_msg *umsg)
{
	net_endpoint_t		*ep;
	net_endpoint_addr_t	addr;
	struct api_net_msg	msg;
	const net_endpoint_addr_t *addrp;
	int			ret;

	ret = api_net_get_endpoint(handle, &ep);
	if (ret != 0) {
		return (ret);
	}
	if (!umsg || !is_user_address(umsg, sizeof(*umsg)) ||
	    !user_range_fault_in(umsg, sizeof(*umsg), 0)) {
		return (-API_ERR_BAD_ADDR);
	}
	memcpy(&msg, umsg, sizeof(msg));

	if (msg.length != 0 &&
	    (!is_user_address(msg.data, msg.length) ||
	    !user_range_fault_in(msg.data, msg.length, 0))) {
		return (-API_ERR_BAD_ADDR);
	}

	addrp = NULL;
	if (msg.addr) {
		ret = api_net_from_user_addr(msg.addr, &addr);
		if (ret != 0) {
			return (ret);
		}
		addrp = &addr;
	}

	return (net_endpoint_send(ep, (const u8 *)msg.data,
	    msg.length, addrp, msg.flags));
}

int
api_net_recv(int handle, struct api_net_msg *umsg)
{
	net_endpoint_t		*ep;
	net_endpoint_addr_t	addr;
	struct api_net_msg	msg;
	u32			out_flags;
	int			ret;

	ret = api_net_get_endpoint(handle, &ep);
	if (ret != 0) {
		return (ret);
	}
	if (!umsg || !is_user_address(umsg, sizeof(*umsg)) ||
	    !user_range_fault_in(umsg, sizeof(*umsg), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	memcpy(&msg, umsg, sizeof(msg));

	if (msg.length != 0 &&
	    (!is_user_address(msg.data, msg.length) ||
	    !user_range_fault_in(msg.data, msg.length, 1))) {
		return (-API_ERR_BAD_ADDR);
	}
	if (msg.addr &&
	    (!is_user_address(msg.addr, sizeof(*msg.addr)) ||
	    !user_range_fault_in(msg.addr, sizeof(*msg.addr), 1))) {
		return (-API_ERR_BAD_ADDR);
	}

	out_flags = 0;
	ret = net_endpoint_recv(ep, (u8 *)msg.data, msg.length,
	    msg.addr ? &addr : NULL, msg.flags, &out_flags);
	if (ret < 0) {
		return (ret);
	}
	if (msg.addr) {
		api_net_to_user_addr(msg.addr, &addr);
	}
	msg.flags = (msg.flags & API_NET_MSG_NONBLOCK) | out_flags;
	umsg->flags = msg.flags;
	return (ret);
}

int
api_net_ctl(int handle, int op, void *arg)
{
	net_endpoint_t		*ep;
	net_endpoint_addr_t	addr;
	int			ret;

	if (api_net_ctl_privileged(op) &&
	    !proc_has_privilege(process_current())) {
		return (-API_ERR_PERM);
	}

	ret = api_net_get_endpoint(handle, &ep);
	if (ret != 0) {
		return (ret);
	}

	switch (op) {
	case API_NET_CTL_GET_LOCAL:
		net_endpoint_get_local(ep, &addr);
		return (api_net_to_user_addr(
		    (struct api_net_addr *)arg, &addr));
	case API_NET_CTL_GET_PEER:
		ret = net_endpoint_get_peer(ep, &addr);
		if (ret != 0) {
			return (ret);
		}
		return (api_net_to_user_addr(
		    (struct api_net_addr *)arg, &addr));
	default:
		return (-API_ERR_NOT_SUPPORTED);
	}
}
