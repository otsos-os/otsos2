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
$define %type api_net_msg as native network message descriptor
$define %func main as start with args int, char **, char **
$define %func print_ip as procedure with args unsigned int
$define %func bind_tcp as function with args int
$define %func send_all as function with args int, const unsigned char *, int
$define %func serve_client as function with args int, const api_net_addr *
$define %func serve as function with args int

*/

/* !SPACE!

$space %internal print_ip, bind_tcp, send_all
$space %internal serve_client, serve
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	TCP_ECHO_DEFAULT_PORT	7
#define	TCP_ECHO_BACKLOG	4
#define	TCP_ECHO_BUF_SIZE	1024

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
bind_tcp(int port)
{
	struct api_net_addr	addr;
	int			fd;

	fd = netOpen(API_NET_PROTO_TCP, API_NET_MODE_STREAM, 0);
	if (fd < 0) {
		printf("tcp_echo: netOpen failed: %d\n", errno);
		return (-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.port = (uint32_t)port;
	addr.ip = 0;
	addr.ifindex = 0;

	if (netBind(fd, &addr) != 0) {
		printf("tcp_echo: netBind port %d failed: %d\n",
		    port, errno);
		dataClose(fd);
		return (-1);
	}
	if (netListen(fd, TCP_ECHO_BACKLOG) != 0) {
		printf("tcp_echo: netListen failed: %d\n", errno);
		dataClose(fd);
		return (-1);
	}
	return (fd);
}

static int
send_all(int fd, const unsigned char *buf, int len)
{
	struct api_net_msg	msg;
	int			off;
	ssize_t			n;

	off = 0;
	while (off < len) {
		memset(&msg, 0, sizeof(msg));
		msg.data = (void *)(buf + off);
		msg.length = (uint32_t)(len - off);
		n = netSend(fd, &msg);
		if (n < 0) {
			return (-1);
		}
		off += (int)n;
	}
	return (0);
}

static int
serve_client(int fd, const struct api_net_addr *peer)
{
	struct api_net_msg	msg;
	unsigned char		buf[TCP_ECHO_BUF_SIZE];
	ssize_t			n;

	printf("tcp_echo: client ");
	print_ip(peer->ip);
	printf(":%u\n", (unsigned int)peer->port);

	for (;;) {
		memset(&msg, 0, sizeof(msg));
		msg.data = buf;
		msg.length = sizeof(buf);
		n = netRecv(fd, &msg);
		if (n < 0) {
			printf("tcp_echo: netRecv failed: %d\n", errno);
			return (-1);
		}
		if (n == 0) {
			return (0);
		}
		if (send_all(fd, buf, (int)n) != 0) {
			printf("tcp_echo: netSend failed: %d\n", errno);
			return (-1);
		}
	}
}

static int
serve(int fd)
{
	struct api_net_addr	peer;
	int			client;

	for (;;) {
		memset(&peer, 0, sizeof(peer));
		client = netAccept(fd, &peer, 0);
		if (client < 0) {
			printf("tcp_echo: netAccept failed: %d\n", errno);
			return (-1);
		}
		serve_client(client, &peer);
		dataClose(client);
	}
}

int
main(int argc, char **argv, char **envp)
{
	int	fd, port;

	(void)envp;
	personality(0);

	port = TCP_ECHO_DEFAULT_PORT;
	if (argc > 1) {
		port = atoi(argv[1]);
		if (port <= 0 || port > 65535) {
			printf("tcp_echo: bad port '%s'\n", argv[1]);
			return (1);
		}
	}

	fd = bind_tcp(port);
	if (fd < 0) {
		return (1);
	}

	printf("tcp_echo: listening on 0.0.0.0:%d\n", port);
	return (serve(fd) == 0 ? 0 : 1);
}
