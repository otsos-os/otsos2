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

$define %type dhcpd_request as DHCP client daemon IPC request
$define %type dhcpd_status as DHCP client daemon IPC status

*/

/* !SPACE!

$space %export dhcpd_request, dhcpd_status

*/

#ifndef PORTS_DHCP_IPC_H
#define PORTS_DHCP_IPC_H

#include <stdint.h>

#define	DHCPD_IPC_SERVICE	"system.network.dhcpd"
#define	DHCPD_IPC_VERSION	1
#define	DHCPD_CMD_ACQUIRE	1
#define	DHCPD_CMD_STATUS	2
#define	DHCPD_CMD_RENEW	3
#define	DHCPD_CMD_RELEASE	4
#define	DHCPD_CMD_STOP		5
#define	DHCPD_STATE_IDLE	0
#define	DHCPD_STATE_ACQUIRING	1
#define	DHCPD_STATE_BOUND	2
#define	DHCPD_STATE_RENEWING	3
#define	DHCPD_STATE_FAILED	4
#define	DHCPD_STATE_STOPPING	5

struct dhcpd_request {
	uint32_t	version;
	uint32_t	command;
	uint32_t	flags;
	uint32_t	reserved;
};

struct dhcpd_status {
	uint32_t	version;
	uint32_t	state;
	int32_t		error;
	uint32_t	ifindex;
	uint32_t	address;
	uint32_t	netmask;
	uint32_t	gateway;
	uint32_t	server;
	uint32_t	lease_seconds;
	uint32_t	renew_seconds;
	uint32_t	rebind_seconds;
	uint32_t	dns_count;
	uint32_t	dns[4];
	char		ifname[16];
};

#endif
