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

$define %type api_net_addr as native IPv4 endpoint address
$define %type api_net_msg as native datagram descriptor
$define %type kevent as native event record
$define %func main as start with args int, char **, char **
$define %func print_ip as procedure with args unsigned int
$define %func bind_udp as function with args int
$define %func serve as function with args int

*/

/* !SPACE!

$space %internal print_ip, bind_udp, serve
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UDP_ECHO_DEFAULT_PORT	7
#define UDP_ECHO_BUF_SIZE	1472

static void
print_ip(uint32_t ip)
{
	printf("%u.%u.%u.%u",
	    (unsigned int)((ip >> 24) & 0xFF),
	    (unsigned int)((ip >> 16) & 0xFF),
	    (unsigned int)((ip >> 8) & 0xFF),
	    (unsigned int)(ip & 0xFF));
}

static int
bind_udp(int port)
{
	struct api_net_addr	addr;
	int			fd;

	fd = netOpen(API_NET_PROTO_UDP, API_NET_MODE_DGRAM,
	    API_NET_OPEN_NONBLOCK);
	if (fd < 0) {
		printf("udp_echo: netOpen failed: %d\n", errno);
		return (-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.port = (uint32_t)port;
	addr.ip = 0;
	addr.ifindex = 0;

	if (netBind(fd, &addr) != 0) {
		printf("udp_echo: netBind port %d failed: %d\n",
		    port, errno);
		return (-1);
	}
	return (fd);
}

static int
serve(int fd)
{
	struct api_net_addr	peer;
	struct api_net_msg	msg;
	struct kevent		change;
	struct kevent		event;
	unsigned char		buf[UDP_ECHO_BUF_SIZE];
	ssize_t			n, sent;
	int			kq, ret;

	kq = eventKqueue();
	if (kq < 0) {
		printf("udp_echo: eventKqueue failed: %d\n", errno);
		return (-1);
	}

	memset(&change, 0, sizeof(change));
	change.ident = (uint64_t)fd;
	change.filter = EVFILT_READ;
	change.flags = EV_ADD | EV_CLEAR;

	ret = eventWait(kq, &change, 1, NULL, 0, -1);
	if (ret < 0) {
		printf("udp_echo: EVFILT_READ add failed: %d\n", errno);
		return (-1);
	}

	for (;;) {
		ret = eventWait(kq, NULL, 0, &event, 1, -1);
		if (ret < 0) {
			printf("udp_echo: eventWait failed: %d\n", errno);
			return (-1);
		}
		if (ret == 0 || event.filter != EVFILT_READ) {
			continue;
		}

		for (;;) {
			memset(&peer, 0, sizeof(peer));
			memset(&msg, 0, sizeof(msg));
			msg.data = buf;
			msg.addr = &peer;
			msg.length = sizeof(buf);
			msg.flags = API_NET_MSG_NONBLOCK;

			n = netRecv(fd, &msg);
			if (n < 0) {
				if (errno == EAGAIN) {
					break;
				}
				printf("udp_echo: netRecv failed: %d\n",
				    errno);
				break;
			}

			msg.data = buf;
			msg.addr = &peer;
			msg.length = (uint32_t)n;
			msg.flags = 0;
			sent = netSend(fd, &msg);
			if (sent < 0) {
				printf("udp_echo: netSend failed: %d\n",
				    errno);
				continue;
			}

			printf("udp_echo: %d bytes from ", (int)n);
			print_ip(peer.ip);
			printf(":%u\n", (unsigned int)peer.port);
		}
	}
}

int
main(int argc, char **argv, char **envp)
{
	int	fd, port;

	(void)envp;
	personality(0);

	port = UDP_ECHO_DEFAULT_PORT;
	if (argc > 1) {
		port = atoi(argv[1]);
		if (port <= 0 || port > 65535) {
			printf("udp_echo: bad port '%s'\n", argv[1]);
			return (1);
		}
	}

	fd = bind_udp(port);
	if (fd < 0) {
		return (1);
	}

	printf("udp_echo: listening on 0.0.0.0:%d\n", port);
	return (serve(fd) == 0 ? 0 : 1);
}
