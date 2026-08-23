/* !DEFINES!

$define %type browser_loader as non-blocking HTTP GET state machine
$define %func browser_loader_init as procedure with args browser_loader *, int
$define %func browser_loader_start as function with args browser_loader *, const char *
$define %func browser_loader_step as function with args browser_loader *
$define %func browser_loader_abort as procedure with args browser_loader *
$define %func browser_loader_reset as procedure with args browser_loader *
$define %func browser_loader_timeout as function with args const browser_loader *

*/

/* !SPACE!

$space %internal loader_fail, loader_wait, loader_unwatch
$space %internal loader_close_sock, loader_close_dns
$space %internal loader_begin_dns, loader_step_dns, loader_dns_send
$space %internal loader_begin_connect, loader_step_connect
$space %internal loader_step_send, loader_step_recv, loader_finish
$space %internal loader_grow, loader_follow, loader_drop_response
$space %export browser_loader_init, browser_loader_start, browser_loader_step
$space %export browser_loader_abort, browser_loader_reset
$space %export browser_loader_timeout

*/

#include <browser.h>
#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initial response buffer; grows geometrically up to BROWSER_MAX_RESPONSE. */
#define LOADER_RESP_INIT 16384
#define LOADER_DNS_PORT 53
/* Bytes drained per step, so a fast server cannot starve the UI. */
#define LOADER_RECV_ROUNDS 16

/*
 * Forward-declared because resolving an address chains straight into opening
 * the socket: entering CONNECT with no descriptor registered would park the
 * caller in eventWait with nothing able to wake it.
 */
static int	loader_begin_connect(browser_loader_t *ld);

/*
 * Closing a handle does NOT drop its knote - the kernel only detaches on
 * EV_DELETE or kqueue teardown.  Every descriptor must therefore be
 * unregistered before it is closed, or each navigation leaks a knote and the
 * pool runs dry after a few dozen page loads.
 */
static void
loader_unwatch(browser_loader_t *ld)
{
	struct kevent	change;

	if (ld->wait_fd < 0 || ld->kq < 0) {
		ld->wait_fd = -1;
		ld->wait_filter = 0;
		return;
	}

	memset(&change, 0, sizeof(change));
	change.ident = (uint64_t)ld->wait_fd;
	change.filter = ld->wait_filter;
	change.flags = EV_DELETE;
	/* Not-found is fine: the caller may be tearing down after a failure. */
	(void)eventWait(ld->kq, &change, 1, NULL, 0, 0);

	ld->wait_fd = -1;
	ld->wait_filter = 0;
}

/*
 * Points the kqueue at exactly one (fd, filter) pair.  Only one is ever armed:
 * leaving EVFILT_WRITE registered on an established socket would report ready
 * on every pass and spin the event loop while the body is still arriving.
 */
static int
loader_wait(browser_loader_t *ld, int fd, int16_t filter)
{
	struct kevent	change;

	if (ld->wait_fd == fd && ld->wait_filter == filter) {
		return (0);
	}
	loader_unwatch(ld);
	if (fd < 0 || ld->kq < 0) {
		return (0);
	}

	memset(&change, 0, sizeof(change));
	change.ident = (uint64_t)fd;
	change.filter = filter;
	change.flags = EV_ADD | EV_CLEAR;
	if (eventWait(ld->kq, &change, 1, NULL, 0, 0) < 0) {
		return (-1);
	}
	ld->wait_fd = fd;
	ld->wait_filter = filter;
	return (0);
}

static void
loader_close_sock(browser_loader_t *ld)
{
	if (ld->sock < 0) {
		return;
	}
	if (ld->wait_fd == ld->sock) {
		loader_unwatch(ld);
	}
	dataClose(ld->sock);
	ld->sock = -1;
}

static void
loader_close_dns(browser_loader_t *ld)
{
	if (ld->dns_fd < 0) {
		return;
	}
	if (ld->wait_fd == ld->dns_fd) {
		loader_unwatch(ld);
	}
	dataClose(ld->dns_fd);
	ld->dns_fd = -1;
}

static void
loader_drop_response(browser_loader_t *ld)
{
	free(ld->response);
	ld->response = NULL;
	ld->response_len = 0;
	ld->response_cap = 0;
}

/*
 * Terminal failure.  Every descriptor is dropped and unregistered here, so no
 * path out of the state machine can leave a socket open or a knote armed.
 */
static int
loader_fail(browser_loader_t *ld, const char *msg)
{
	loader_close_dns(ld);
	loader_close_sock(ld);
	loader_unwatch(ld);
	snprintf(ld->error, sizeof(ld->error), "%s", msg);
	ld->state = BROWSER_LOAD_ERROR;
	return (-1);
}

void
browser_loader_init(browser_loader_t *ld, int kq)
{
	if (ld == NULL) {
		return;
	}
	memset(ld, 0, sizeof(*ld));
	ld->sock = -1;
	ld->dns_fd = -1;
	ld->wait_fd = -1;
	ld->kq = kq;
	ld->state = BROWSER_LOAD_IDLE;
}

void
browser_loader_reset(browser_loader_t *ld)
{
	if (ld == NULL) {
		return;
	}
	loader_close_dns(ld);
	loader_close_sock(ld);
	loader_unwatch(ld);
	loader_drop_response(ld);
	ld->request_len = 0;
	ld->request_sent = 0;
	ld->redirects = 0;
	ld->ip = 0;
	ld->error[0] = '\0';
	ld->state = BROWSER_LOAD_IDLE;
}

void
browser_loader_abort(browser_loader_t *ld)
{
	if (ld == NULL || ld->state == BROWSER_LOAD_IDLE) {
		return;
	}
	browser_loader_reset(ld);
	snprintf(ld->error, sizeof(ld->error), "Stopped");
}

/*
 * How long the caller may block in eventWait.  Bounded by whichever deadline is
 * live, because a peer that simply goes quiet produces no event at all - the
 * expiry is noticed by the next step, not by the kqueue.
 */
int
browser_loader_timeout(const browser_loader_t *ld)
{
	uint64_t	now, target;

	if (ld == NULL) {
		return (-1);
	}
	switch (ld->state) {
	case BROWSER_LOAD_DNS:
		target = ld->dns_deadline;
		break;
	case BROWSER_LOAD_CONNECT:
	case BROWSER_LOAD_SEND:
	case BROWSER_LOAD_RECV:
		target = ld->deadline;
		break;
	default:
		return (-1);
	}

	now = browser_now_ms();
	if (target <= now) {
		return (0);
	}
	if (target - now > 1000u) {
		/* Capped so a stalled load is still stepped about once a sec. */
		return (1000);
	}
	return ((int)(target - now));
}

static int
loader_dns_send(browser_loader_t *ld)
{
	struct api_net_addr	addr;
	struct api_net_msg	msg;
	unsigned char		packet[512];
	uint32_t		server;
	int			len;

	server = browser_dns_server(ld->dns_server);
	if (server == 0) {
		return (loader_fail(ld, "DNS: no resolver answered"));
	}

	len = browser_dns_build(ld->host, ld->dns_id, packet, sizeof(packet));
	if (len < 0) {
		return (loader_fail(ld, "DNS: hostname too long"));
	}

	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.port = LOADER_DNS_PORT;
	addr.ip = server;

	memset(&msg, 0, sizeof(msg));
	msg.data = packet;
	msg.length = (uint32_t)len;
	msg.addr = &addr;

	if (netSend(ld->dns_fd, &msg) < 0) {
		/*
		 * No route yet, or the link is down.  Counted as this attempt
		 * failing rather than the whole resolve: the next server may be
		 * reachable, and an expired deadline drives the retry.
		 */
		ld->dns_deadline = browser_now_ms();
		return (0);
	}
	ld->dns_deadline = browser_now_ms() + BROWSER_DNS_TIMEOUT_MS;
	return (0);
}

static int
loader_begin_dns(browser_loader_t *ld)
{
	ld->ip = browser_ip_literal(ld->host);
	if (ld->ip != 0) {
		/* Dotted quad: no query needed, go straight to the socket. */
		return (loader_begin_connect(ld));
	}

	loader_close_dns(ld);
	ld->dns_fd = netOpen(API_NET_PROTO_UDP, API_NET_MODE_DGRAM,
	    API_NET_OPEN_NONBLOCK);
	if (ld->dns_fd < 0) {
		return (loader_fail(ld, "DNS: cannot open UDP socket"));
	}

	ld->dns_server = 0;
	ld->dns_try = 0;
	/* Low bits of the clock are enough to reject a stale answer. */
	ld->dns_id = (uint16_t)(browser_now_ms() & 0xFFFFu);
	ld->state = BROWSER_LOAD_DNS;
	if (loader_wait(ld, ld->dns_fd, EVFILT_READ) != 0) {
		return (loader_fail(ld, "DNS: cannot watch socket"));
	}
	return (loader_dns_send(ld));
}

static int
loader_step_dns(browser_loader_t *ld)
{
	struct api_net_addr	from;
	struct api_net_msg	msg;
	unsigned char		resp[1024];
	ssize_t			n;
	uint32_t		ip;

	for (;;) {
		memset(&from, 0, sizeof(from));
		memset(&msg, 0, sizeof(msg));
		msg.data = resp;
		msg.length = sizeof(resp);
		msg.addr = &from;

		n = netRecv(ld->dns_fd, &msg);
		if (n < 0) {
			if (errno != EAGAIN) {
				return (loader_fail(ld, "DNS: socket error"));
			}
			break;
		}
		if (n == 0) {
			break;
		}
		ip = browser_dns_parse(resp, (size_t)n, ld->dns_id);
		if (ip != 0) {
			ld->ip = ip;
			loader_close_dns(ld);
			return (loader_begin_connect(ld));
		}
		/* Wrong id, or no A record: keep draining, keep waiting. */
	}

	if (browser_now_ms() < ld->dns_deadline) {
		return (0);
	}

	ld->dns_try++;
	if (ld->dns_try >= BROWSER_DNS_TRIES) {
		ld->dns_try = 0;
		ld->dns_server++;
		if (ld->dns_server >= BROWSER_DNS_SERVERS) {
			return (loader_fail(ld, "DNS lookup failed"));
		}
	}
	return (loader_dns_send(ld));
}

static int
loader_begin_connect(browser_loader_t *ld)
{
	struct api_net_addr	addr;
	int			ret;

	loader_close_sock(ld);
	ld->sock = netOpen(API_NET_PROTO_TCP, API_NET_MODE_STREAM,
	    API_NET_OPEN_NONBLOCK);
	if (ld->sock < 0) {
		return (loader_fail(ld, "cannot open TCP socket"));
	}

	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.port = (uint32_t)ld->port;
	addr.ip = ld->ip;

	ld->deadline = browser_now_ms() + BROWSER_CONNECT_TIMEOUT_MS;
	ld->request_sent = 0;

	ret = netConnect(ld->sock, &addr);
	if (ret != 0 && errno != EAGAIN) {
		return (loader_fail(ld, "connect failed"));
	}

	/*
	 * Either the handshake finished inline (loopback) or the SYN is out.
	 * Both are reported as writability by the kernel, so the same wait
	 * covers them; the CONNECT step reads the real state.
	 */
	ld->state = (ret == 0) ? BROWSER_LOAD_SEND : BROWSER_LOAD_CONNECT;
	if (loader_wait(ld, ld->sock, EVFILT_WRITE) != 0) {
		return (loader_fail(ld, "cannot watch socket"));
	}
	return (0);
}

static int
loader_step_connect(browser_loader_t *ld)
{
	struct api_net_state	st;

	/*
	 * API_NET_CTL_GET_STATE is the readiness check.  A zero-length send
	 * cannot serve here - the kernel short-circuits len == 0 before it
	 * looks at the connection - and a real send returns EPIPE both while
	 * the handshake is outstanding and after a refusal, so the two are
	 * indistinguishable from send alone.
	 */
	memset(&st, 0, sizeof(st));
	if (netCtl(ld->sock, API_NET_CTL_GET_STATE, &st) != 0) {
		return (loader_fail(ld, "cannot read socket state"));
	}

	if (st.error != 0) {
		return (loader_fail(ld, "connection refused"));
	}

	if (st.state == API_NET_STATE_CONNECTED ||
	    st.state == API_NET_STATE_PEER_CLOSED) {
		ld->state = BROWSER_LOAD_SEND;
		ld->deadline = browser_now_ms() + BROWSER_RECV_TIMEOUT_MS;
		return (0);
	}

	if (st.state != API_NET_STATE_CONNECTING) {
		/* CLOSED with no latched error: the peer sent a bare RST. */
		return (loader_fail(ld, "connection failed"));
	}

	if (browser_now_ms() >= ld->deadline) {
		return (loader_fail(ld, "connection timed out"));
	}
	return (0);
}

static int
loader_step_send(browser_loader_t *ld)
{
	struct api_net_msg	msg;
	ssize_t			n;

	while (ld->request_sent < ld->request_len) {
		memset(&msg, 0, sizeof(msg));
		msg.data = ld->request + ld->request_sent;
		msg.length = (uint32_t)(ld->request_len - ld->request_sent);
		n = netSend(ld->sock, &msg);
		if (n < 0) {
			if (errno != EAGAIN) {
				return (loader_fail(ld, "send failed"));
			}
			if (browser_now_ms() >= ld->deadline) {
				return (loader_fail(ld, "send timed out"));
			}
			return (loader_wait(ld, ld->sock, EVFILT_WRITE) == 0 ?
			    0 : loader_fail(ld, "cannot watch socket"));
		}
		if (n == 0) {
			/* No progress, no error: wait rather than spin. */
			return (loader_wait(ld, ld->sock, EVFILT_WRITE) == 0 ?
			    0 : loader_fail(ld, "cannot watch socket"));
		}
		/*
		 * A short write is expected: the stack sends one segment per
		 * ACK, so a request can take several passes to drain.
		 */
		ld->request_sent += (size_t)n;
		ld->deadline = browser_now_ms() + BROWSER_RECV_TIMEOUT_MS;
	}

	ld->state = BROWSER_LOAD_RECV;
	ld->deadline = browser_now_ms() + BROWSER_RECV_TIMEOUT_MS;
	if (loader_wait(ld, ld->sock, EVFILT_READ) != 0) {
		return (loader_fail(ld, "cannot watch socket"));
	}
	return (0);
}

/* Ensures room for `need` more bytes plus the trailing NUL. */
static int
loader_grow(browser_loader_t *ld, size_t need)
{
	size_t	cap;
	char	*buf;

	if (ld->response_len + need + 1 <= ld->response_cap) {
		return (0);
	}
	if (ld->response_len + need > BROWSER_MAX_RESPONSE) {
		return (-1);
	}

	cap = (ld->response_cap != 0) ? ld->response_cap : LOADER_RESP_INIT;
	while (cap < ld->response_len + need + 1) {
		cap *= 2;
		if (cap > BROWSER_MAX_RESPONSE + 1) {
			cap = BROWSER_MAX_RESPONSE + 1;
			break;
		}
	}
	if (cap < ld->response_len + need + 1) {
		return (-1);
	}

	buf = (char *)realloc(ld->response, cap);
	if (buf == NULL) {
		return (-1);
	}
	ld->response = buf;
	ld->response_cap = cap;
	return (0);
}

/*
 * Re-points the loader at a redirect target and restarts from DNS.  The
 * response is dropped first: keeping it would prepend the 302's own body to
 * the real page.
 */
static int
loader_follow(browser_loader_t *ld, const char *location)
{
	char	next[BROWSER_MAX_URL];

	if (ld->redirects >= BROWSER_MAX_REDIRECTS) {
		return (loader_fail(ld, "too many redirects"));
	}
	browser_url_resolve(ld->url, location, next, sizeof(next));
	if (next[0] == '\0') {
		return (loader_fail(ld, "bad redirect target"));
	}

	ld->redirects++;
	return (browser_loader_start(ld, next));
}

static int
loader_finish(browser_loader_t *ld)
{
	char	location[BROWSER_MAX_URL];
	int	status;

	loader_close_sock(ld);
	loader_unwatch(ld);

	if (ld->response == NULL || ld->response_len == 0) {
		return (loader_fail(ld, "empty response"));
	}
	ld->response[ld->response_len] = '\0';

	status = browser_http_status(ld->response, ld->response_len);
	if (status == 301 || status == 302 || status == 303 ||
	    status == 307 || status == 308) {
		if (browser_http_location(ld->response, ld->response_len,
		    location, sizeof(location)) == 0) {
			return (loader_follow(ld, location));
		}
		/* Redirect with no usable Location: show what the server sent. */
	}

	ld->state = BROWSER_LOAD_DONE;
	return (0);
}

static int
loader_step_recv(browser_loader_t *ld)
{
	struct api_net_msg	msg;
	ssize_t			n;
	int			rounds;

	for (rounds = 0; rounds < LOADER_RECV_ROUNDS; rounds++) {
		if (loader_grow(ld, BROWSER_CHUNK_SIZE) != 0) {
			return (loader_fail(ld, "page too large"));
		}
		memset(&msg, 0, sizeof(msg));
		msg.data = ld->response + ld->response_len;
		msg.length = BROWSER_CHUNK_SIZE;
		n = netRecv(ld->sock, &msg);
		if (n < 0) {
			if (errno != EAGAIN) {
				/*
				 * A reset after a complete-looking body is
				 * common on HTTP/1.0 closes; keep what arrived
				 * instead of discarding the page.
				 */
				if (ld->response_len != 0) {
					return (loader_finish(ld));
				}
				return (loader_fail(ld, "receive failed"));
			}
			if (browser_now_ms() >= ld->deadline) {
				if (ld->response_len != 0) {
					return (loader_finish(ld));
				}
				return (loader_fail(ld, "receive timed out"));
			}
			return (0);
		}
		if (n == 0) {
			/* Orderly close: with HTTP/1.0 that ends the body. */
			return (loader_finish(ld));
		}
		ld->response_len += (size_t)n;
		ld->deadline = browser_now_ms() + BROWSER_RECV_TIMEOUT_MS;
	}
	return (0);
}

int
browser_loader_start(browser_loader_t *ld, const char *url)
{
	char	normalized[BROWSER_MAX_URL];
	int	is_https, len;

	if (ld == NULL || url == NULL || url[0] == '\0') {
		return (-1);
	}

	loader_close_dns(ld);
	loader_close_sock(ld);
	loader_unwatch(ld);
	loader_drop_response(ld);
	ld->request_sent = 0;
	ld->error[0] = '\0';
	ld->ip = 0;

	browser_url_normalize(url, normalized, sizeof(normalized));
	if (normalized[0] == '\0') {
		return (loader_fail(ld, "empty URL"));
	}
	snprintf(ld->url, sizeof(ld->url), "%s", normalized);

	if (browser_url_parse(ld->url, ld->host, sizeof(ld->host), &ld->port,
	    ld->path, sizeof(ld->path), &is_https) != 0) {
		return (loader_fail(ld, "malformed URL"));
	}

	/*
	 * There is no TLS in the tree, so https cannot be fetched.  Failing
	 * here with a clear message beats connecting to port 443 in the clear
	 * and rendering handshake bytes as a page - and it is why a plain
	 * http://google.com ends up unreachable: it redirects to https.
	 */
	if (is_https) {
		return (loader_fail(ld, "https not supported (no TLS)"));
	}

	len = browser_http_request(ld->request, sizeof(ld->request), ld->host,
	    ld->port, ld->path);
	if (len < 0) {
		return (loader_fail(ld, "request too long"));
	}
	ld->request_len = (size_t)len;

	return (loader_begin_dns(ld));
}

int
browser_loader_step(browser_loader_t *ld)
{
	if (ld == NULL) {
		return (-1);
	}

	switch (ld->state) {
	case BROWSER_LOAD_DNS:
		return (loader_step_dns(ld));
	case BROWSER_LOAD_CONNECT:
		return (loader_step_connect(ld));
	case BROWSER_LOAD_SEND:
		return (loader_step_send(ld));
	case BROWSER_LOAD_RECV:
		return (loader_step_recv(ld));
	case BROWSER_LOAD_IDLE:
	case BROWSER_LOAD_DONE:
	case BROWSER_LOAD_ERROR:
	default:
		return (0);
	}
}
