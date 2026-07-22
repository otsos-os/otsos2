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

$define %type api_ipc_call as native IPC call descriptor
$define %type dhcpd_request as DHCP client daemon IPC request
$define %type dhcpd_status as DHCP client daemon IPC status
$define %func sleep_ms as procedure with args int
$define %func connect_dhcpd as function with args void
$define %func start_dhcpd as function with args void
$define %func command_from_args as function with args int, char **
$define %func print_status as procedure with args const dhcpd_status *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal sleep_ms, connect_dhcpd, start_dhcpd
$space %internal command_from_args, print_status
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dhcp_ipc.h"

#define	DHCPC_CONNECT_TRIES	40
#define	DHCPC_CONNECT_MS	50
#define	DHCPD_PATH		"/bin/dhcpd"

static void
sleep_ms(int ms)
{
	struct kevent	change;
	struct kevent	event;
	int		kq;

	kq = eventKqueue();
	if (kq < 0) {
		return;
	}
	memset(&change, 0, sizeof(change));
	change.ident = 1;
	change.filter = EVFILT_TIMER;
	change.flags = EV_ADD | EV_ONESHOT;
	change.data = ms;
	(void)eventWait(kq, &change, 1, &event, 1, ms + 50);
	eventClose(kq);
}

static int
connect_dhcpd(void)
{
	int	handle;

	handle = ipcConnect(DHCPD_IPC_SERVICE, 0);
	return (handle);
}

static int
start_dhcpd(void)
{
	char	*argv[2];
	int	status;
	int	pid;

	argv[0] = "dhcpd";
	argv[1] = NULL;
	pid = procSpawnNative(DHCPD_PATH, argv, NULL);
	if (pid < 0) {
		printf("dhcpc: spawn %s failed: %d\n", DHCPD_PATH, errno);
		return (-1);
	}
	(void)procWait(&status);
	return (0);
}

static int
command_from_args(int argc, char **argv)
{
	if (argc < 2 || strcmp(argv[1], "acquire") == 0) {
		return (DHCPD_CMD_ACQUIRE);
	}
	if (strcmp(argv[1], "status") == 0) {
		return (DHCPD_CMD_STATUS);
	}
	if (strcmp(argv[1], "renew") == 0) {
		return (DHCPD_CMD_RENEW);
	}
	if (strcmp(argv[1], "release") == 0) {
		return (DHCPD_CMD_RELEASE);
	}
	if (strcmp(argv[1], "stop") == 0) {
		return (DHCPD_CMD_STOP);
	}
	return (-1);
}

static void
print_status(const struct dhcpd_status *status)
{
	printf("dhcpc: state=%u iface=%s ifindex=%u error=%d\n",
	    (unsigned int)status->state, status->ifname,
	    (unsigned int)status->ifindex, (int)status->error);
	if (status->state == DHCPD_STATE_BOUND) {
		printf("dhcpc: lease %u.%u.%u.%u lease=%u renew=%u\n",
		    (unsigned int)((status->address >> 24) & 0xFF),
		    (unsigned int)((status->address >> 16) & 0xFF),
		    (unsigned int)((status->address >> 8) & 0xFF),
		    (unsigned int)(status->address & 0xFF),
		    (unsigned int)status->lease_seconds,
		    (unsigned int)status->renew_seconds);
	}
}

int
main(int argc, char **argv, char **envp)
{
	struct dhcpd_request	request;
	struct dhcpd_status	status;
	struct api_ipc_call	call;
	int			command;
	int			handle;
	int			i;
	ssize_t			ret;

	(void)envp;
	personality(0);
	command = command_from_args(argc, argv);
	if (command < 0) {
		printf("usage: dhcpc [acquire|status|renew|release|stop]\n");
		return (1);
	}
	handle = connect_dhcpd();
	if (handle < 0) {
		if (command != DHCPD_CMD_ACQUIRE) {
			printf("dhcpc: dhcpd is not running\n");
			return (1);
		}
		printf("dhcpc: starting dhcpd\n");
		if (start_dhcpd() != 0) {
			return (1);
		}
		for (i = 0; i < DHCPC_CONNECT_TRIES; i++) {
			handle = connect_dhcpd();
			if (handle >= 0) {
				break;
			}
			sleep_ms(DHCPC_CONNECT_MS);
		}
	}
	if (handle < 0) {
		printf("dhcpc: dhcpd IPC service did not become ready\n");
		return (1);
	}
	memset(&request, 0, sizeof(request));
	request.version = DHCPD_IPC_VERSION;
	request.command = (uint32_t)command;
	memset(&status, 0, sizeof(status));
	memset(&call, 0, sizeof(call));
	call.request.opcode = (uint32_t)command;
	call.request.flags = IPC_MSG_REQUEST;
	call.request.length = sizeof(request);
	call.request.data = &request;
	call.reply.flags = IPC_MSG_REPLY;
	call.reply.capacity = sizeof(status);
	call.reply.data = &status;
	call.timeout_ms = -1;
	ret = ipcCall(handle, &call);
	dataClose(handle);
	if (ret < 0) {
		printf("dhcpc: IPC request failed: %d\n", errno);
		return (1);
	}
	if ((size_t)ret != sizeof(status) ||
	    status.version != DHCPD_IPC_VERSION) {
		printf("dhcpc: invalid dhcpd reply\n");
		return (1);
	}
	print_status(&status);
	return (status.error == 0 ? 0 : 1);
}
