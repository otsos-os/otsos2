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
 * CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type send_target_t as parsed IPv4 endpoint
$define %type send_stats_t as HTTP request counters
$define %type api_net_addr as native IPv4 endpoint address
$define %type api_net_msg as native network message descriptor
$define %type api_timeinfo as native time data
$define %func usage as procedure with args void
$define %func parse_ipv4 as function with args const char *, uint32_t *
$define %func parse_target as function with args const char *, send_target_t *
$define %func now_ms as function with args void
$define %func connect_target as function with args const send_target_t *
$define %func send_all as function with args fd, buffer, size, stats
$define %func read_response as function with args int, send_stats_t *
$define %func run_http as function with args fd, target, stats
$define %func print_stats as procedure with args const send_stats_t *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal usage, parse_ipv4, parse_target, now_ms
$space %internal connect_target, send_all, read_response
$space %internal run_http, print_stats
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	SEND_BUF_SIZE		1024
#define	SEND_REQ_SIZE		256
#define	SEND_HOST_SIZE		16
#define	SEND_AUTH_SIZE		32

typedef struct {
	uint32_t	ip;
	uint32_t	port;
	char		host[SEND_HOST_SIZE];
	char		authority[SEND_AUTH_SIZE];
} send_target_t;

typedef struct {
	uint64_t	start_ms;
	uint64_t	connected_ms;
	uint64_t	end_ms;
	uint64_t	sent_bytes;
	uint64_t	recv_bytes;
	uint32_t	send_calls;
	uint32_t	recv_calls;
} send_stats_t;

static void
usage(void)
{
	printf("usage: send <ip:port>\n");
	printf("example: send 1.1.1.1:80\n");
}

static int
parse_ipv4(const char *text, uint32_t *out)
{
	const char	*p;
	uint32_t	ip, part;
	int		digits, i;

	if (!text || !out) {
		return (-1);
	}

	p = text;
	ip = 0;
	for (i = 0; i < 4; i++) {
		part = 0;
		digits = 0;
		while (*p >= '0' && *p <= '9') {
			part = part * 10 + (uint32_t)(*p - '0');
			if (part > 255) {
				return (-1);
			}
			p++;
			digits++;
		}
		if (digits == 0) {
			return (-1);
		}
		ip = (ip << 8) | part;
		if (i < 3) {
			if (*p != '.') {
				return (-1);
			}
			p++;
		}
	}
	if (*p != '\0') {
		return (-1);
	}
	*out = ip;
	return (0);
}

static int
parse_target(const char *arg, send_target_t *target)
{
	const char	*colon;
	char		*end;
	unsigned long	port;
	size_t		host_len, arg_len;

	if (!arg || !target) {
		return (-1);
	}

	memset(target, 0, sizeof(*target));
	colon = strchr(arg, ':');
	if (!colon || colon == arg || colon[1] == '\0') {
		return (-1);
	}

	host_len = (size_t)(colon - arg);
	arg_len = strlen(arg);
	if (host_len >= sizeof(target->host) ||
	    arg_len >= sizeof(target->authority)) {
		return (-1);
	}

	memcpy(target->host, arg, host_len);
	target->host[host_len] = '\0';
	memcpy(target->authority, arg, arg_len + 1);

	if (parse_ipv4(target->host, &target->ip) != 0) {
		return (-1);
	}
	port = strtoul(colon + 1, &end, 10);
	if (end == colon + 1 || *end != '\0' ||
	    port == 0 || port > 65535) {
		return (-1);
	}
	target->port = (uint32_t)port;
	return (0);
}

static uint64_t
now_ms(void)
{
	struct api_timeinfo	ti;

	if (sysTimeInfo(&ti) != 0) {
		return (0);
	}
	return (ti.uptime_sec * 1000ULL + ti.uptime_nsec / 1000000ULL);
}

static int
connect_target(const send_target_t *target)
{
	struct api_net_addr	addr;
	int			fd;

	fd = netOpen(API_NET_PROTO_TCP, API_NET_MODE_STREAM, 0);
	if (fd < 0) {
		printf("send: netOpen failed: %d\n", errno);
		return (-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.ip = target->ip;
	addr.port = target->port;
	addr.ifindex = 0;

	if (netConnect(fd, &addr) != 0) {
		printf("send: connect %s failed: %d\n",
		    target->authority, errno);
		dataClose(fd);
		return (-1);
	}
	return (fd);
}

static int
send_all(int fd, const unsigned char *buf, size_t len,
    send_stats_t *stats)
{
	struct api_net_msg	msg;
	size_t			off;
	ssize_t			n;

	off = 0;
	while (off < len) {
		memset(&msg, 0, sizeof(msg));
		msg.data = (void *)(buf + off);
		msg.length = (uint32_t)(len - off);

		n = netSend(fd, &msg);
		if (n <= 0 || (size_t)n > len - off) {
			return (-1);
		}
		off += (size_t)n;
		stats->sent_bytes += (uint64_t)n;
		stats->send_calls++;
	}
	return (0);
}

static int
read_response(int fd, send_stats_t *stats)
{
	struct api_net_msg	msg;
	unsigned char		buf[SEND_BUF_SIZE];
	ssize_t			n;

	for (;;) {
		memset(&msg, 0, sizeof(msg));
		msg.data = buf;
		msg.length = sizeof(buf);

		n = netRecv(fd, &msg);
		if (n < 0) {
			printf("\nsend: netRecv failed: %d\n", errno);
			return (-1);
		}
		if (n == 0) {
			break;
		}
		stats->recv_bytes += (uint64_t)n;
		stats->recv_calls++;
		fwrite(buf, 1, (size_t)n, stdout);
	}
	fflush(stdout);
	return (0);
}

static int
run_http(int fd, const send_target_t *target, send_stats_t *stats)
{
	char	request[SEND_REQ_SIZE];
	int	len;

	len = snprintf(request, sizeof(request),
	    "GET / HTTP/1.0\r\n"
	    "Host: %s\r\n"
	    "User-Agent: otsos-send/0\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    target->authority);
	if (len < 0 || len >= (int)sizeof(request)) {
		printf("send: request too large\n");
		return (-1);
	}

	printf("send: connected to %s\n", target->authority);
	printf("send: response follows\n\n");
	if (send_all(fd, (const unsigned char *)request,
	    (size_t)len, stats) != 0) {
		printf("send: netSend failed: %d\n", errno);
		return (-1);
	}
	return (read_response(fd, stats));
}

static void
print_stats(const send_stats_t *stats)
{
	uint64_t	connect_ms, total_ms;

	connect_ms = 0;
	total_ms = 0;
	if (stats->connected_ms >= stats->start_ms) {
		connect_ms = stats->connected_ms - stats->start_ms;
	}
	if (stats->end_ms >= stats->start_ms) {
		total_ms = stats->end_ms - stats->start_ms;
	}

	printf("\nsend: stats sent=%llu recv=%llu send_calls=%u "
	    "recv_calls=%u connect_ms=%llu total_ms=%llu\n",
	    (unsigned long long)stats->sent_bytes,
	    (unsigned long long)stats->recv_bytes,
	    (unsigned int)stats->send_calls,
	    (unsigned int)stats->recv_calls,
	    (unsigned long long)connect_ms,
	    (unsigned long long)total_ms);
}

int
main(int argc, char **argv, char **envp)
{
	send_target_t	target;
	send_stats_t	stats;
	int		fd, ret;

	(void)envp;
	personality(0);

	if (argc != 2) {
		usage();
		return (1);
	}
	if (parse_target(argv[1], &target) != 0) {
		printf("send: bad target '%s'\n", argv[1]);
		usage();
		return (1);
	}

	memset(&stats, 0, sizeof(stats));
	stats.start_ms = now_ms();

	fd = connect_target(&target);
	stats.connected_ms = now_ms();
	if (fd < 0) {
		stats.end_ms = now_ms();
		print_stats(&stats);
		return (1);
	}

	ret = run_http(fd, &target, &stats);
	dataClose(fd);

	stats.end_ms = now_ms();
	print_stats(&stats);
	return (ret == 0 ? 0 : 1);
}
