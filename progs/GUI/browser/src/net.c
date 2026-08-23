/* !DEFINES!

$define %type browser_net as URL, DNS and HTTP message helpers
$define %func browser_url_normalize as procedure with args const char *, char *, size_t
$define %func browser_url_parse as function with args const char *, char *, size_t, int *, char *, size_t, int *
$define %func browser_url_resolve as procedure with args const char *, const char *, char *, size_t
$define %func browser_now_ms as function with args void
$define %func browser_dns_build as function with args const char *, uint16_t, unsigned char *, size_t
$define %func browser_dns_parse as function with args const unsigned char *, size_t, uint16_t
$define %func browser_dns_server as function with args int
$define %func browser_ip_literal as function with args const char *
$define %func browser_http_request as function with args char *, size_t, const char *, int, const char *
$define %func browser_http_status as function with args const char *, size_t
$define %func browser_http_location as function with args const char *, size_t, char *, size_t
$define %func browser_http_body_offset as function with args const char *, size_t

*/

/* !SPACE!

$space %internal dns_encode_name, dns_skip_name, http_header_len
$space %export browser_url_normalize, browser_url_parse, browser_url_resolve
$space %export browser_now_ms, browser_dns_build, browser_dns_parse
$space %export browser_dns_server, browser_ip_literal
$space %export browser_http_request, browser_http_status
$space %export browser_http_location, browser_http_body_offset

*/

#include <browser.h>
#include <ctype.h>
#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DNS_PORT 53
#define HTTP_PORT 80
#define HTTPS_PORT 443
#define DNS_HEADER_LEN 12
#define DNS_LABEL_MAX 63
#define DNS_TYPE_A 1
#define DNS_CLASS_IN 1
/* A compressed name is two bytes with the top two bits set (RFC 1035 4.1.4). */
#define DNS_PTR_MASK 0xC0
/* Bound on label hops so a self-referential compression pointer cannot spin. */
#define DNS_MAX_HOPS 128

/*
 * Milliseconds since boot.  sysTimeInfo can fail, and a clock that returns a
 * constant would freeze every deadline in the loader - so on failure the last
 * value is advanced by one tick instead.  Timeouts then still expire, just
 * more slowly, rather than never.
 */
uint64_t
browser_now_ms(void)
{
	static uint64_t		last;
	struct api_timeinfo	ti;

	memset(&ti, 0, sizeof(ti));
	if (sysTimeInfo(&ti) != 0) {
		last++;
		return (last);
	}
	last = ti.uptime_sec * 1000u + ti.uptime_nsec / 1000000u;
	return (last);
}

void
browser_url_normalize(const char *url, char *out, size_t max_out)
{
	const char	*p;

	if (out == NULL || max_out == 0) {
		return;
	}
	out[0] = '\0';
	if (url == NULL) {
		return;
	}

	p = url;
	while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '\0') {
		return;
	}

	if (strncasecmp(p, "http://", 7) == 0 ||
	    strncasecmp(p, "https://", 8) == 0 ||
	    strncasecmp(p, "file://", 7) == 0 || *p == '/') {
		snprintf(out, max_out, "%s", p);
		return;
	}
	snprintf(out, max_out, "http://%s", p);
}

int
browser_url_parse(const char *url, char *host, size_t max_host, int *port,
    char *path, size_t max_path, int *is_https)
{
	const char	*p, *colon, *slash, *at, *hash;
	size_t		hlen;

	if (url == NULL || host == NULL || port == NULL || path == NULL ||
	    max_host == 0 || max_path == 0) {
		return (-1);
	}

	host[0] = '\0';
	snprintf(path, max_path, "/");
	*port = HTTP_PORT;
	if (is_https != NULL) {
		*is_https = 0;
	}

	p = url;
	if (strncasecmp(p, "http://", 7) == 0) {
		p += 7;
	} else if (strncasecmp(p, "https://", 8) == 0) {
		p += 8;
		*port = HTTPS_PORT;
		if (is_https != NULL) {
			*is_https = 1;
		}
	}

	slash = strchr(p, '/');

	/* userinfo@host is legal; drop it, we never authenticate. */
	at = strchr(p, '@');
	if (at != NULL && (slash == NULL || at < slash)) {
		p = at + 1;
		slash = strchr(p, '/');
	}

	colon = strchr(p, ':');
	if (colon != NULL && (slash == NULL || colon < slash)) {
		hlen = (size_t)(colon - p);
		*port = atoi(colon + 1);
		if (*port <= 0 || *port > 65535) {
			return (-1);
		}
	} else if (slash != NULL) {
		hlen = (size_t)(slash - p);
	} else {
		hlen = strlen(p);
	}

	if (hlen == 0 || hlen >= max_host) {
		return (-1);
	}
	memcpy(host, p, hlen);
	host[hlen] = '\0';

	if (slash != NULL) {
		snprintf(path, max_path, "%s", slash);
		/* A fragment is client-side only and must not go on the wire. */
		hash = strchr(path, '#');
		if (hash != NULL) {
			path[hash - path] = '\0';
		}
		if (path[0] == '\0') {
			snprintf(path, max_path, "/");
		}
	}
	return (0);
}

/*
 * Enough of RFC 3986 5.2 to follow real redirects: absolute references pass
 * through, "//host/x" inherits the scheme, "/x" inherits the authority, and
 * anything else is taken relative to the base's directory.
 */
void
browser_url_resolve(const char *base, const char *ref, char *out,
    size_t max_out)
{
	char		host[BROWSER_MAX_HOST];
	char		path[BROWSER_MAX_PATH];
	const char	*scheme;
	char		*last;
	int		port, https;

	if (out == NULL || max_out == 0) {
		return;
	}
	out[0] = '\0';
	if (ref == NULL || ref[0] == '\0') {
		return;
	}

	if (strncasecmp(ref, "http://", 7) == 0 ||
	    strncasecmp(ref, "https://", 8) == 0) {
		snprintf(out, max_out, "%s", ref);
		return;
	}

	if (base == NULL || browser_url_parse(base, host, sizeof(host), &port,
	    path, sizeof(path), &https) != 0) {
		browser_url_normalize(ref, out, max_out);
		return;
	}
	scheme = https ? "https" : "http";

	if (ref[0] == '/' && ref[1] == '/') {
		snprintf(out, max_out, "%s:%s", scheme, ref);
		return;
	}

	if (ref[0] == '/') {
		if (port == HTTP_PORT || port == HTTPS_PORT) {
			snprintf(out, max_out, "%s://%s%s", scheme, host, ref);
		} else {
			snprintf(out, max_out, "%s://%s:%d%s", scheme, host,
			    port, ref);
		}
		return;
	}

	/* Relative: replace the last path segment. */
	last = strrchr(path, '/');
	if (last != NULL) {
		last[1] = '\0';
	} else {
		snprintf(path, sizeof(path), "/");
	}
	if (port == HTTP_PORT || port == HTTPS_PORT) {
		snprintf(out, max_out, "%s://%s%s%s", scheme, host, path, ref);
	} else {
		snprintf(out, max_out, "%s://%s:%d%s%s", scheme, host, port,
		    path, ref);
	}
}

uint32_t
browser_dns_server(int index)
{
	/*
	 * 10.0.2.3 is QEMU's built-in user-mode resolver and answers first in
	 * the common case; the public resolvers are the fallback for a bridged
	 * or tap setup.  Change these if the host network differs.
	 */
	static const uint32_t servers[BROWSER_DNS_SERVERS] = {
		(10u << 24) | (0u << 16) | (2u << 8) | 3u,
		(1u << 24) | (1u << 16) | (1u << 8) | 1u,
		(8u << 24) | (8u << 16) | (8u << 8) | 8u
	};

	if (index < 0 || index >= BROWSER_DNS_SERVERS) {
		return (0);
	}
	return (servers[index]);
}

/* Returns the address for a dotted-quad literal, or 0 when it is not one. */
uint32_t
browser_ip_literal(const char *host)
{
	uint32_t	ip;
	unsigned int	octet;
	int		part, digits;
	const char	*p;

	if (host == NULL) {
		return (0);
	}

	ip = 0;
	p = host;
	for (part = 0; part < 4; part++) {
		octet = 0;
		digits = 0;
		while (*p >= '0' && *p <= '9') {
			octet = octet * 10u + (unsigned int)(*p - '0');
			if (octet > 255u) {
				return (0);
			}
			digits++;
			p++;
		}
		if (digits == 0 || digits > 3) {
			return (0);
		}
		ip = (ip << 8) | octet;
		if (part < 3) {
			if (*p != '.') {
				return (0);
			}
			p++;
		}
	}
	if (*p != '\0') {
		return (0);
	}
	/* 0.0.0.0 is indistinguishable from "not a literal" here; reject it. */
	return (ip);
}

static int
dns_encode_name(const char *host, unsigned char *out, size_t max_len)
{
	const char	*p, *dot;
	size_t		pos, seg;

	pos = 0;
	p = host;
	while (*p != '\0') {
		dot = strchr(p, '.');
		seg = (dot == NULL) ? strlen(p) : (size_t)(dot - p);
		if (seg == 0 || seg > DNS_LABEL_MAX ||
		    pos + seg + 2 > max_len) {
			return (-1);
		}
		out[pos++] = (unsigned char)seg;
		memcpy(out + pos, p, seg);
		pos += seg;
		if (dot == NULL) {
			break;
		}
		p = dot + 1;
	}
	if (pos + 1 > max_len) {
		return (-1);
	}
	out[pos++] = 0;
	return ((int)pos);
}

int
browser_dns_build(const char *host, uint16_t id, unsigned char *out,
    size_t max_out)
{
	int	qlen;
	size_t	pos;

	if (host == NULL || out == NULL || max_out < DNS_HEADER_LEN + 6) {
		return (-1);
	}

	memset(out, 0, DNS_HEADER_LEN);
	out[0] = (unsigned char)(id >> 8);
	out[1] = (unsigned char)(id & 0xFF);
	out[2] = 0x01;		/* QR=0, standard query, RD=1 */
	out[5] = 0x01;		/* QDCOUNT = 1 */

	qlen = dns_encode_name(host, out + DNS_HEADER_LEN,
	    max_out - DNS_HEADER_LEN - 4);
	if (qlen < 0) {
		return (-1);
	}
	pos = DNS_HEADER_LEN + (size_t)qlen;
	out[pos++] = 0;
	out[pos++] = DNS_TYPE_A;
	out[pos++] = 0;
	out[pos++] = DNS_CLASS_IN;
	return ((int)pos);
}

/*
 * Walks past one name.  Bounded twice over: by the buffer and by a hop count,
 * because a compression pointer can legally point backwards and a crafted
 * response can point at itself.
 */
static size_t
dns_skip_name(const unsigned char *resp, size_t len, size_t pos, int *ok)
{
	int	hops;

	*ok = 0;
	for (hops = 0; hops < DNS_MAX_HOPS; hops++) {
		if (pos >= len) {
			return (pos);
		}
		if ((resp[pos] & DNS_PTR_MASK) == DNS_PTR_MASK) {
			if (pos + 2 > len) {
				return (pos);
			}
			*ok = 1;
			return (pos + 2);
		}
		if (resp[pos] == 0) {
			*ok = 1;
			return (pos + 1);
		}
		pos += (size_t)resp[pos] + 1;
	}
	return (pos);
}

uint32_t
browser_dns_parse(const unsigned char *resp, size_t len, uint16_t id)
{
	uint16_t	ancount, qdcount, type, rdlen;
	size_t		pos;
	int		i, ok;

	if (resp == NULL || len < DNS_HEADER_LEN) {
		return (0);
	}
	if (resp[0] != (unsigned char)(id >> 8) ||
	    resp[1] != (unsigned char)(id & 0xFF)) {
		return (0);
	}
	if ((resp[2] & 0x80) == 0) {
		return (0);	/* not a response */
	}
	if ((resp[3] & 0x0F) != 0) {
		return (0);	/* RCODE says the query failed */
	}

	qdcount = (uint16_t)((resp[4] << 8) | resp[5]);
	ancount = (uint16_t)((resp[6] << 8) | resp[7]);
	if (ancount == 0) {
		return (0);
	}

	pos = DNS_HEADER_LEN;
	for (i = 0; i < (int)qdcount; i++) {
		pos = dns_skip_name(resp, len, pos, &ok);
		if (!ok || pos + 4 > len) {
			return (0);
		}
		pos += 4;
	}

	for (i = 0; i < (int)ancount; i++) {
		pos = dns_skip_name(resp, len, pos, &ok);
		if (!ok || pos + 10 > len) {
			return (0);
		}
		type = (uint16_t)((resp[pos] << 8) | resp[pos + 1]);
		rdlen = (uint16_t)((resp[pos + 8] << 8) | resp[pos + 9]);
		pos += 10;
		if (pos + rdlen > len) {
			return (0);
		}
		if (type == DNS_TYPE_A && rdlen == 4) {
			return (((uint32_t)resp[pos] << 24) |
			    ((uint32_t)resp[pos + 1] << 16) |
			    ((uint32_t)resp[pos + 2] << 8) |
			    (uint32_t)resp[pos + 3]);
		}
		pos += rdlen;
	}
	return (0);
}

int
browser_http_request(char *out, size_t max_out, const char *host, int port,
    const char *path)
{
	int	n;

	if (out == NULL || max_out == 0 || host == NULL || path == NULL) {
		return (-1);
	}

	/*
	 * HTTP/1.0 with Connection: close.  The loader reads to EOF, so it
	 * needs the server to close rather than keep-alive; asking for 1.1
	 * would invite chunked encoding, which nothing here decodes.
	 */
	if (port == HTTP_PORT) {
		n = snprintf(out, max_out,
		    "GET %s HTTP/1.0\r\n"
		    "Host: %s\r\n"
		    "User-Agent: otsos2-browser/" BROWSER_VERSION "\r\n"
		    "Accept: text/html,text/plain,*/*\r\n"
		    "Accept-Encoding: identity\r\n"
		    "Connection: close\r\n\r\n", path, host);
	} else {
		n = snprintf(out, max_out,
		    "GET %s HTTP/1.0\r\n"
		    "Host: %s:%d\r\n"
		    "User-Agent: otsos2-browser/" BROWSER_VERSION "\r\n"
		    "Accept: text/html,text/plain,*/*\r\n"
		    "Accept-Encoding: identity\r\n"
		    "Connection: close\r\n\r\n", path, host, port);
	}

	if (n < 0 || (size_t)n >= max_out) {
		return (-1);
	}
	return (n);
}

/*
 * Length of the HTTP header block.  Header scans are clamped to this so a body
 * containing the text "Location:" cannot be mistaken for a redirect.
 */
static size_t
http_header_len(const char *resp, size_t len)
{
	size_t	i;

	for (i = 0; i + 1 < len; i++) {
		if (resp[i] == '\n' && resp[i + 1] == '\n') {
			return (i + 1);
		}
		if (i + 3 < len && resp[i] == '\r' && resp[i + 1] == '\n' &&
		    resp[i + 2] == '\r' && resp[i + 3] == '\n') {
			return (i + 2);
		}
	}
	return (len);
}

int
browser_http_status(const char *resp, size_t len)
{
	size_t	i;

	if (resp == NULL || len < 12 || strncmp(resp, "HTTP/", 5) != 0) {
		return (-1);
	}
	for (i = 5; i < len && resp[i] != ' '; i++) {
		if (resp[i] == '\r' || resp[i] == '\n') {
			return (-1);
		}
	}
	if (i + 3 >= len) {
		return (-1);
	}
	i++;
	if (!isdigit((unsigned char)resp[i])) {
		return (-1);
	}
	return ((resp[i] - '0') * 100 + (resp[i + 1] - '0') * 10 +
	    (resp[i + 2] - '0'));
}

int
browser_http_location(const char *resp, size_t len, char *out, size_t max_out)
{
	size_t	hdr, i, start, n;

	if (resp == NULL || out == NULL || max_out == 0) {
		return (-1);
	}
	out[0] = '\0';
	hdr = http_header_len(resp, len);

	for (i = 0; i + 9 < hdr; i++) {
		/* Header names are case-insensitive; only match line starts. */
		if (i != 0 && resp[i - 1] != '\n') {
			continue;
		}
		if (strncasecmp(resp + i, "Location:", 9) != 0) {
			continue;
		}
		start = i + 9;
		while (start < hdr && (resp[start] == ' ' ||
		    resp[start] == '\t')) {
			start++;
		}
		n = 0;
		while (start + n < hdr && resp[start + n] != '\r' &&
		    resp[start + n] != '\n' && n + 1 < max_out) {
			n++;
		}
		if (n == 0) {
			return (-1);
		}
		memcpy(out, resp + start, n);
		out[n] = '\0';
		return (0);
	}
	return (-1);
}

size_t
browser_http_body_offset(const char *resp, size_t len)
{
	size_t	i;

	if (resp == NULL) {
		return (0);
	}
	for (i = 0; i + 3 < len; i++) {
		if (resp[i] == '\r' && resp[i + 1] == '\n' &&
		    resp[i + 2] == '\r' && resp[i + 3] == '\n') {
			return (i + 4);
		}
	}
	for (i = 0; i + 1 < len; i++) {
		if (resp[i] == '\n' && resp[i + 1] == '\n') {
			return (i + 2);
		}
	}
	/*
	 * No header terminator: a bare body, or a truncated response.  Treating
	 * the whole buffer as body beats showing raw headers as page text.
	 */
	return (0);
}
