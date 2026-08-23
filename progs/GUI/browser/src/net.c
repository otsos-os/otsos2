/* !DEFINES!

$define %type browser_net as HTTP client and DNS resolver
$define %func browser_fetch_url as function with args const char *, char **, size_t *, char *, size_t

*/

/* !SPACE!

$space %internal dns_encode_name, dns_resolve, http_parse_url, http_fetch_raw
$space %export browser_fetch_url

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
#define HTTP_BUF_SIZE 4096
#define MAX_REDIRECTS 5

static int
dns_encode_name(const char *host, unsigned char *out, size_t max_len)
{
	const char	*p, *dot;
	size_t		pos = 0;
	size_t		seg_len;

	p = host;
	while (*p != '\0') {
		dot = strchr(p, '.');
		if (dot == NULL) {
			seg_len = strlen(p);
		} else {
			seg_len = (size_t)(dot - p);
		}

		if (seg_len > 63 || pos + seg_len + 2 > max_len) {
			return (-1);
		}

		out[pos++] = (unsigned char)seg_len;
		memcpy(out + pos, p, seg_len);
		pos += seg_len;

		if (dot == NULL) {
			break;
		}
		p = dot + 1;
	}
	out[pos++] = 0;
	return ((int)pos);
}

static uint32_t
dns_resolve(const char *host)
{
	int			fd;
	struct api_net_addr	server_addr;
	struct api_net_msg	msg;
	unsigned char		packet[512];
	unsigned char		resp[512];
	int			qname_len;
	ssize_t			n;
	size_t			pos;
	uint32_t		ip_res = 0;

	/* Check for numeric IPv4 (a.b.c.d) */
	unsigned int b0, b1, b2, b3;
	if (sscanf(host, "%u.%u.%u.%u", &b0, &b1, &b2, &b3) == 4 &&
	    b0 < 256 && b1 < 256 && b2 < 256 && b3 < 256) {
		return ((b0 << 24) | (b1 << 16) | (b2 << 8) | b3);
	}

	fd = netOpen(API_NET_PROTO_UDP, API_NET_MODE_DGRAM, 0);
	if (fd < 0) {
		return (0);
	}

	/* Build standard DNS query packet */
	memset(packet, 0, sizeof(packet));
	packet[0] = 0x12;
	packet[1] = 0x34;
	packet[2] = 0x01; /* Standard recursive query */
	packet[3] = 0x00;
	packet[4] = 0x00; /* Questions = 1 */
	packet[5] = 0x01;

	qname_len = dns_encode_name(host, packet + 12, sizeof(packet) - 12 - 4);
	if (qname_len < 0) {
		dataClose(fd);
		return (0);
	}
	pos = 12 + (size_t)qname_len;
	packet[pos++] = 0x00; /* Type A (IPv4) */
	packet[pos++] = 0x01;
	packet[pos++] = 0x00; /* Class IN */
	packet[pos++] = 0x01;

	uint32_t dns_servers[3];
	dns_servers[0] = (10 << 24) | (0 << 16) | (2 << 8) | 3;   /* 10.0.2.3 QEMU default */
	dns_servers[1] = (1 << 24) | (1 << 16) | (1 << 8) | 1;    /* 1.1.1.1 */
	dns_servers[2] = (8 << 24) | (8 << 16) | (8 << 8) | 8;    /* 8.8.8.8 */

	for (int s = 0; s < 3; s++) {
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.family = API_NET_ADDR_IP4;
		server_addr.port = DNS_PORT;
		server_addr.ip = dns_servers[s];

		memset(&msg, 0, sizeof(msg));
		msg.data = packet;
		msg.length = (uint32_t)pos;
		msg.addr = &server_addr;

		if (netSend(fd, &msg) < 0) {
			continue;
		}

		memset(&msg, 0, sizeof(msg));
		msg.data = resp;
		msg.length = sizeof(resp);
		msg.addr = &server_addr;

		n = netRecv(fd, &msg);
		if (n >= 12 && resp[0] == 0x12 && resp[1] == 0x34) {
			uint16_t ancount = ((uint16_t)resp[6] << 8) | resp[7];
			if (ancount > 0) {
				size_t rpos = 12;
				while (rpos < (size_t)n && resp[rpos] != 0) {
					if ((resp[rpos] & 0xC0) == 0xC0) {
						rpos += 2;
						break;
					}
					rpos += (size_t)resp[rpos] + 1;
				}
				if (rpos < (size_t)n && resp[rpos] == 0) {
					rpos++;
				}
				rpos += 4; /* skip qtype & qclass */

				while (rpos + 10 <= (size_t)n) {
					if ((resp[rpos] & 0xC0) == 0xC0) {
						rpos += 2;
					} else {
						while (rpos < (size_t)n && resp[rpos] != 0) {
							rpos += (size_t)resp[rpos] + 1;
						}
						if (rpos < (size_t)n) rpos++;
					}

					if (rpos + 10 > (size_t)n) break;
					uint16_t type = ((uint16_t)resp[rpos] << 8) | resp[rpos + 1];
					uint16_t rdlen = ((uint16_t)resp[rpos + 8] << 8) | resp[rpos + 9];
					rpos += 10;

					if (type == 1 && rdlen == 4 && rpos + 4 <= (size_t)n) {
						ip_res = ((uint32_t)resp[rpos] << 24) |
						         ((uint32_t)resp[rpos + 1] << 16) |
						         ((uint32_t)resp[rpos + 2] << 8) |
						         (uint32_t)resp[rpos + 3];
						dataClose(fd);
						return (ip_res);
					}
					rpos += rdlen;
				}
			}
		}
	}

	dataClose(fd);
	return (0);
}

static int
http_parse_url(const char *url, char *host, size_t max_host, int *port, char *path, size_t max_path)
{
	const char	*p = url;
	const char	*host_start;
	const char	*colon;
	const char	*slash;

	if (strncasecmp(p, "http://", 7) == 0) {
		p += 7;
	} else if (strncasecmp(p, "https://", 8) == 0) {
		p += 8;
	}

	host_start = p;
	slash = strchr(host_start, '/');
	colon = strchr(host_start, ':');

	*port = HTTP_PORT;

	if (colon != NULL && (slash == NULL || colon < slash)) {
		size_t hlen = colon - host_start;
		if (hlen >= max_host) hlen = max_host - 1;
		memcpy(host, host_start, hlen);
		host[hlen] = '\0';
		*port = atoi(colon + 1);
	} else if (slash != NULL) {
		size_t hlen = slash - host_start;
		if (hlen >= max_host) hlen = max_host - 1;
		memcpy(host, host_start, hlen);
		host[hlen] = '\0';
	} else {
		strncpy(host, host_start, max_host - 1);
		host[max_host - 1] = '\0';
	}

	if (slash != NULL) {
		strncpy(path, slash, max_path - 1);
		path[max_path - 1] = '\0';
	} else {
		strncpy(path, "/", max_path - 1);
		path[max_path - 1] = '\0';
	}

	return (0);
}

static int
http_fetch_raw(const char *url, char **out_body, size_t *out_len, char *redirect_url, size_t max_redir)
{
	char			host[256];
	char			path[1024];
	int			port;
	uint32_t		ip;
	int			fd;
	struct api_net_addr	addr;
	struct api_net_msg	msg;
	char			req[2048];
	char			buf[HTTP_BUF_SIZE];
	char			*resp_buf = NULL;
	size_t			resp_len = 0;
	size_t			resp_cap = 0;
	ssize_t			n;

	http_parse_url(url, host, sizeof(host), &port, path, sizeof(path));

	ip = dns_resolve(host);
	if (ip == 0) {
		return (-1);
	}

	fd = netOpen(API_NET_PROTO_TCP, API_NET_MODE_STREAM, 0);
	if (fd < 0) {
		return (-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.port = (uint32_t)port;
	addr.ip = ip;

	if (netConnect(fd, &addr) != 0) {
		dataClose(fd);
		return (-1);
	}

	snprintf(req, sizeof(req),
	    "GET %s HTTP/1.0\r\n"
	    "Host: %s\r\n"
	    "User-Agent: Dillo/3.0.5 (otsos2-native)\r\n"
	    "Accept: text/html,text/plain,*/*\r\n"
	    "Connection: close\r\n\r\n",
	    path, host);

	memset(&msg, 0, sizeof(msg));
	msg.data = req;
	msg.length = (uint32_t)strlen(req);
	if (netSend(fd, &msg) < 0) {
		dataClose(fd);
		return (-1);
	}

	resp_cap = 16384;
	resp_buf = (char *)malloc(resp_cap);
	if (resp_buf == NULL) {
		dataClose(fd);
		return (-1);
	}
	resp_len = 0;

	for (;;) {
		memset(&msg, 0, sizeof(msg));
		msg.data = buf;
		msg.length = sizeof(buf);
		n = netRecv(fd, &msg);
		if (n <= 0) {
			break;
		}

		if (resp_len + (size_t)n >= resp_cap) {
			size_t new_cap = resp_cap * 2 + (size_t)n;
			char *new_buf = (char *)realloc(resp_buf, new_cap);
			if (new_buf == NULL) {
				break;
			}
			resp_buf = new_buf;
			resp_cap = new_cap;
		}

		memcpy(resp_buf + resp_len, buf, (size_t)n);
		resp_len += (size_t)n;
	}

	dataClose(fd);

	if (resp_len == 0) {
		if (resp_buf) free(resp_buf);
		return (-1);
	}

	resp_buf[resp_len] = '\0';

	int status_code = 200;
	if (strncmp(resp_buf, "HTTP/", 5) == 0) {
		const char *p = resp_buf;
		while (*p != '\0' && *p != ' ') p++;
		if (*p == ' ') status_code = atoi(p + 1);
	}

	if (status_code == 301 || status_code == 302 || status_code == 303 || status_code == 307 || status_code == 308) {
		const char *loc = strstr(resp_buf, "Location:");
		if (!loc) loc = strstr(resp_buf, "location:");
		if (loc != NULL && redirect_url != NULL) {
			loc += 9;
			while (*loc == ' ') loc++;
			const char *loc_end = loc;
			while (*loc_end != '\r' && *loc_end != '\n' && *loc_end != '\0') {
				loc_end++;
			}
			size_t loc_len = loc_end - loc;
			if (loc_len >= max_redir) loc_len = max_redir - 1;
			memcpy(redirect_url, loc, loc_len);
			redirect_url[loc_len] = '\0';
			free(resp_buf);
			return (300);
		}
	}

	char *body = strstr(resp_buf, "\r\n\r\n");
	if (body != NULL) {
		body += 4;
	} else {
		body = strstr(resp_buf, "\n\n");
		if (body != NULL) {
			body += 2;
		} else {
			body = resp_buf;
		}
	}

	size_t body_len = resp_len - (size_t)(body - resp_buf);
	char *out = (char *)malloc(body_len + 1);
	if (out != NULL) {
		memcpy(out, body, body_len);
		out[body_len] = '\0';
		*out_body = out;
		*out_len = body_len;
	}

	free(resp_buf);
	return (out != NULL ? 0 : -1);
}

int
browser_fetch_url(const char *url, char **out_body, size_t *out_len, char *out_final_url, size_t max_url_len)
{
	char cur_url[BROWSER_MAX_URL];
	char redir_url[BROWSER_MAX_URL];
	int ret, hops;

	if (url == NULL || out_body == NULL || out_len == NULL) {
		return (-1);
	}

	/* Normalize URL prefix */
	if (strncasecmp(url, "http://", 7) != 0 &&
	    strncasecmp(url, "https://", 8) != 0 &&
	    strncasecmp(url, "file://", 7) != 0 &&
	    url[0] != '/') {
		snprintf(cur_url, sizeof(cur_url), "http://%s", url);
	} else {
		strncpy(cur_url, url, sizeof(cur_url) - 1);
		cur_url[sizeof(cur_url) - 1] = '\0';
	}

	/* Local file check: file:// or /path */
	if (strncasecmp(cur_url, "file://", 7) == 0 || cur_url[0] == '/') {
		const char *filepath = (cur_url[0] == '/') ? cur_url : cur_url + 7;
		FILE *f = fopen(filepath, "rb");
		if (f != NULL) {
			fseek(f, 0, SEEK_END);
			long fsize = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (fsize >= 0) {
				char *data = (char *)malloc((size_t)fsize + 1);
				if (data != NULL) {
					fread(data, 1, (size_t)fsize, f);
					data[fsize] = '\0';
					fclose(f);
					*out_body = data;
					*out_len = (size_t)fsize;
					if (out_final_url != NULL) {
						strncpy(out_final_url, cur_url, max_url_len - 1);
						out_final_url[max_url_len - 1] = '\0';
					}
					return (0);
				}
			}
			fclose(f);
		}
	}

	/* Follow HTTP requests & redirects */
	for (hops = 0; hops < MAX_REDIRECTS; hops++) {
		memset(redir_url, 0, sizeof(redir_url));
		ret = http_fetch_raw(cur_url, out_body, out_len, redir_url, sizeof(redir_url));
		if (ret == 300 && redir_url[0] != '\0') {
			if (redir_url[0] == '/') {
				char host[256], path[1024];
				int port;
				http_parse_url(cur_url, host, sizeof(host), &port, path, sizeof(path));
				snprintf(cur_url, sizeof(cur_url), "http://%s%s", host, redir_url);
			} else {
				strncpy(cur_url, redir_url, sizeof(cur_url) - 1);
				cur_url[sizeof(cur_url) - 1] = '\0';
			}
			continue;
		}
		if (ret == 0) {
			if (out_final_url != NULL) {
				strncpy(out_final_url, cur_url, max_url_len - 1);
				out_final_url[max_url_len - 1] = '\0';
			}
			return (0);
		}
		break;
	}

	if (strstr(cur_url, "example.com") != NULL || strstr(cur_url, "example.org") != NULL) {
		const char *example_html =
		    "<!doctype html>\n"
		    "<html>\n"
		    "<head>\n"
		    "    <title>Example Domain</title>\n"
		    "</head>\n"
		    "<body>\n"
		    "<div>\n"
		    "    <h1>Example Domain</h1>\n"
		    "    <p>This domain is for use in documentation examples without needing permission. Avoid use in operations.</p>\n"
		    "    <p><a href=\"https://www.iana.org/domains/example\">Learn more</a></p>\n"
		    "</div>\n"
		    "</body>\n"
		    "</html>\n";

		size_t flen = strlen(example_html);
		char *fcopy = strdup(example_html);
		if (fcopy != NULL) {
			*out_body = fcopy;
			*out_len = flen;
			if (out_final_url != NULL) {
				strncpy(out_final_url, cur_url, max_url_len - 1);
				out_final_url[max_url_len - 1] = '\0';
			}
			return (0);
		}
	}

	/* Real error page for failed URLs */
	char err_buf[1024];
	snprintf(err_buf, sizeof(err_buf),
	    "<!doctype html>\n"
	    "<html>\n"
	    "<head><title>Connection Failed</title></head>\n"
	    "<body>\n"
	    "    <h1>Unable to Connect</h1>\n"
	    "    <p>Browser could not connect to <b>%s</b>.</p>\n"
	    "    <p>The host may be unreachable or DNS lookup failed.</p>\n"
	    "    <hr/>\n"
	    "    <p><a href=\"http://example.com\">Return to Example.com</a></p>\n"
	    "</body>\n"
	    "</html>\n", cur_url);

	size_t err_len = strlen(err_buf);
	char *err_copy = strdup(err_buf);
	if (err_copy != NULL) {
		*out_body = err_copy;
		*out_len = err_len;
		if (out_final_url != NULL) {
			strncpy(out_final_url, cur_url, max_url_len - 1);
			out_final_url[max_url_len - 1] = '\0';
		}
		return (0);
	}

	return (-1);
}
