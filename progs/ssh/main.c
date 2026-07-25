/* !DEFINES!

$define %type ssh_options as parsed SSH command line options
$define %type ssh_run_state as channel loop exit state
$define %func ssh_usage as procedure with args void
$define %func ssh_error_name as function with args int
$define %func ssh_log_level_name as function with args int
$define %func ssh_log_callback as procedure with args void *, int, const char *
$define %func ssh_write_all as function with args const void *, size_t
$define %func ssh_copy_range as function with args char *, size_t, const char *, size_t
$define %func ssh_parse_port as function with args const char *, uint32_t *
$define %func ssh_parse_ipv4 as function with args const char *, uint32_t *
$define %func ssh_parse_target as function with args ssh_options *, const char *
$define %func ssh_parse_args as function with args int, char **, ssh_options *
$define %func ssh_build_command as function with args int, char **, int, char *, size_t
$define %func ssh_read_line as function with args char *, size_t, uint32_t
$define %func ssh_read_password as function with args const char *, char *, size_t
$define %func ssh_confirm_new_host as function with args const char *, const char *
$define %func ssh_check_known_host as function with args const ssh_options *, result
$define %func ssh_auth_password as function with args lssh_transport *, options
$define %func ssh_term_size as procedure with args uint32_t *, uint32_t *, uint32_t *, uint32_t *
$define %func ssh_channel_replenish as function with args transport, channel
$define %func ssh_handle_channel_event as function with args transport, channel, event, state
$define %func ssh_recv_channel_event as function with args transport, channel, buffer, state
$define %func ssh_send_channel_data as function with args transport, channel, data, len
$define %func ssh_register_events as function with args int, int
$define %func ssh_run_interactive as function with args transport, channel
$define %func ssh_run_exec_wait as function with args transport, channel
$define %func ssh_open_and_run as function with args transport, options
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal ssh_usage, ssh_error_name, ssh_log_level_name
$space %internal ssh_log_callback, ssh_write_all
$space %internal ssh_copy_range, ssh_parse_port, ssh_parse_ipv4
$space %internal ssh_parse_target, ssh_parse_args, ssh_build_command
$space %internal ssh_read_line, ssh_read_password, ssh_confirm_new_host
$space %internal ssh_check_known_host, ssh_auth_password, ssh_term_size
$space %internal ssh_channel_replenish, ssh_handle_channel_event
$space %internal ssh_recv_channel_event, ssh_send_channel_data
$space %internal ssh_register_events, ssh_run_interactive
$space %internal ssh_run_exec_wait, ssh_open_and_run
$space %export main

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <errno.h>
#include <libcrypto.h>
#include <libssh.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SSH_DEFAULT_PORT	22
#define SSH_KNOWN_HOSTS		"/known_hosts"
#define SSH_USER_MAX		64
#define SSH_HOST_MAX		64
#define SSH_COMMAND_MAX		1024
#define SSH_PASSWORD_MAX	256
#define SSH_PROMPT_MAX		192
#define SSH_AUTH_TRIES		3
#define SSH_STDIN_IDENT		0
#define SSH_STDIN_BUF		1024

typedef struct ssh_options {
	char	user[SSH_USER_MAX];
	char	host[SSH_HOST_MAX];
	char	command[SSH_COMMAND_MAX];
	uint32_t ip;
	uint32_t port;
	int	has_user;
	int	has_command;
	int	force_tty;
	int	no_tty;
	int	use_pty;
	int	verbose;
} ssh_options;

typedef struct ssh_run_state {
	int	done;
	int	exit_status;
} ssh_run_state;

static int	ssh_build_command(int argc, char **argv, int start,
		    char *out, size_t out_size);

static void
ssh_usage(void)
{
	printf("usage: ssh [-p port] [-l user] [-t|-T] [user@]ip[:port] ");
	printf("[command ...]\n");
	printf("       ssh [-v|-vv] ... enables LibSSH protocol logs.\n");
	printf("note: hostnames are not available yet; use numeric IPv4.\n");
}

static const char *
ssh_error_name(int err)
{
	switch (err) {
	case LSSH_OK:
		return ("ok");
	case LSSH_ERR_INVALID:
		return ("invalid argument");
	case LSSH_ERR_NO_MEMORY:
		return ("out of memory");
	case LSSH_ERR_RANGE:
		return ("value out of range");
	case LSSH_ERR_FORMAT:
		return ("protocol format error");
	case LSSH_ERR_UNSUPPORTED:
		return ("unsupported algorithm or message");
	case LSSH_ERR_CRYPTO:
		return ("crypto failure");
	case LSSH_ERR_STATE:
		return ("bad protocol state");
	case LSSH_ERR_IO:
		return ("io error");
	case LSSH_ERR_AUTH:
		return ("authentication failed");
	case LSSH_ERR_VERIFY:
		return ("cryptographic verification failed");
	case LSSH_ERR_AGAIN:
		return ("operation would block");
	case LSSH_ERR_NO_MATCH:
		return ("no matching SSH algorithm");
	default:
		return ("unknown error");
	}
}

static const char *
ssh_log_level_name(int level)
{
	switch (level) {
	case LSSH_LOG_ERROR:
		return ("error");
	case LSSH_LOG_INFO:
		return ("info");
	case LSSH_LOG_DEBUG:
		return ("debug");
	default:
		return ("log");
	}
}

static void
ssh_log_callback(void *ctx, int level, const char *message)
{
	(void)ctx;
	fprintf(stderr, "ssh: libssh %s: %s\n",
	    ssh_log_level_name(level), message ? message : "");
	fflush(stderr);
}

static int
ssh_write_all(const void *data, size_t len)
{
	const uint8_t	*p;
	size_t		off;
	ssize_t		n;

	if (!data && len != 0) {
		return (-1);
	}
	p = (const uint8_t *)data;
	off = 0;
	while (off < len) {
		n = termWrite(p + off, len - off);
		if (n <= 0) {
			return (-1);
		}
		off += (size_t)n;
	}
	return (0);
}

static int
ssh_copy_range(char *dst, size_t dst_size, const char *src, size_t len)
{
	if (!dst || dst_size == 0 || !src || len >= dst_size) {
		return (-1);
	}
	memcpy(dst, src, len);
	dst[len] = '\0';
	return (0);
}

static int
ssh_parse_port(const char *text, uint32_t *out)
{
	char		*end;
	unsigned long	port;

	if (!text || !out || text[0] == '\0') {
		return (-1);
	}
	port = strtoul(text, &end, 10);
	if (end == text || *end != '\0' || port == 0 || port > 65535) {
		return (-1);
	}
	*out = (uint32_t)port;
	return (0);
}

static int
ssh_parse_ipv4(const char *text, uint32_t *out)
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
ssh_parse_target(ssh_options *opts, const char *arg)
{
	const char	*host_start, *colon, *at;
	size_t		host_len, user_len;

	if (!opts || !arg || arg[0] == '\0') {
		return (-1);
	}

	host_start = arg;
	at = strchr(arg, '@');
	if (at) {
		user_len = (size_t)(at - arg);
		if (user_len == 0 ||
		    ssh_copy_range(opts->user, sizeof(opts->user),
		    arg, user_len) != 0) {
			return (-1);
		}
		opts->has_user = 1;
		host_start = at + 1;
	}

	colon = strchr(host_start, ':');
	if (colon) {
		host_len = (size_t)(colon - host_start);
		if (host_len == 0 ||
		    ssh_parse_port(colon + 1, &opts->port) != 0) {
			return (-1);
		}
	} else {
		host_len = strlen(host_start);
	}
	if (ssh_copy_range(opts->host, sizeof(opts->host),
	    host_start, host_len) != 0) {
		return (-1);
	}
	if (ssh_parse_ipv4(opts->host, &opts->ip) != 0) {
		return (-1);
	}
	return (0);
}

static int
ssh_parse_args(int argc, char **argv, ssh_options *opts)
{
	char	*env_user;
	int	i, j;

	if (!opts) {
		return (-1);
	}
	memset(opts, 0, sizeof(*opts));
	opts->port = SSH_DEFAULT_PORT;

	i = 1;
	while (i < argc) {
		if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			return (1);
		}
		if (argv[i][0] == '-' && argv[i][1] == 'v') {
			j = 1;
			while (argv[i][j] == 'v') {
				j++;
			}
			if (argv[i][j] == '\0') {
				opts->verbose += j - 1;
				i++;
				continue;
			}
		}
		if (strcmp(argv[i], "-p") == 0) {
			i++;
			if (i >= argc ||
			    ssh_parse_port(argv[i], &opts->port) != 0) {
				return (-1);
			}
			i++;
			continue;
		}
		if (strncmp(argv[i], "-p", 2) == 0 && argv[i][2] != '\0') {
			if (ssh_parse_port(argv[i] + 2, &opts->port) != 0) {
				return (-1);
			}
			i++;
			continue;
		}
		if (strcmp(argv[i], "-l") == 0) {
			i++;
			if (i >= argc ||
			    ssh_copy_range(opts->user, sizeof(opts->user),
			    argv[i], strlen(argv[i])) != 0) {
				return (-1);
			}
			opts->has_user = 1;
			i++;
			continue;
		}
		if (strncmp(argv[i], "-l", 2) == 0 && argv[i][2] != '\0') {
			if (ssh_copy_range(opts->user, sizeof(opts->user),
			    argv[i] + 2, strlen(argv[i] + 2)) != 0) {
				return (-1);
			}
			opts->has_user = 1;
			i++;
			continue;
		}
		if (strcmp(argv[i], "-t") == 0) {
			opts->force_tty = 1;
			i++;
			continue;
		}
		if (strcmp(argv[i], "-T") == 0) {
			opts->no_tty = 1;
			i++;
			continue;
		}
		if (argv[i][0] == '-' && argv[i][1] != '\0') {
			return (-1);
		}
		break;
	}

	if (i >= argc || ssh_parse_target(opts, argv[i]) != 0) {
		return (-1);
	}
	i++;
	if (!opts->has_user) {
		env_user = getenv("USER");
		if (!env_user || env_user[0] == '\0') {
			env_user = "user";
		}
		if (ssh_copy_range(opts->user, sizeof(opts->user),
		    env_user, strlen(env_user)) != 0) {
			return (-1);
		}
		opts->has_user = 1;
	}
	if (i < argc) {
		opts->has_command = 1;
		if (ssh_build_command(argc, argv, i, opts->command,
		    sizeof(opts->command)) != 0) {
			return (-1);
		}
	}
	opts->use_pty = (!opts->has_command && !opts->no_tty) ||
	    (opts->has_command && opts->force_tty && !opts->no_tty);
	return (0);
}

static int
ssh_build_command(int argc, char **argv, int start, char *out,
    size_t out_size)
{
	size_t	pos, len;
	int	i;

	if (!argv || !out || out_size == 0 || start >= argc) {
		return (-1);
	}
	pos = 0;
	for (i = start; i < argc; i++) {
		len = strlen(argv[i]);
		if (pos + len + (i == start ? 0 : 1) >= out_size) {
			return (-1);
		}
		if (i != start) {
			out[pos++] = ' ';
		}
		memcpy(out + pos, argv[i], len);
		pos += len;
	}
	out[pos] = '\0';
	return (0);
}

static int
ssh_read_line(char *buf, size_t size, uint32_t flags)
{
	size_t	pos;
	ssize_t	n;
	char	c;

	if (!buf || size == 0) {
		return (-1);
	}
	pos = 0;
	for (;;) {
		n = termReadFlags(&c, 1, flags | TERM_READ_IGNORE_SIGINT);
		if (n < 0) {
			return (-1);
		}
		if (n == 0) {
			continue;
		}
		if (c == '\r' || c == '\n') {
			buf[pos] = '\0';
			return ((int)pos);
		}
		if (c == 0x03) {
			buf[0] = '\0';
			return (-2);
		}
		if ((c == '\b' || c == 0x7f) && pos > 0) {
			pos--;
			continue;
		}
		if ((uint8_t)c >= 32 && pos + 1 < size) {
			buf[pos++] = c;
		}
	}
}

static int
ssh_read_password(const char *prompt, char *buf, size_t size)
{
	int	ret;

	if (!prompt || !buf || size == 0) {
		return (-1);
	}
	ssh_write_all(prompt, strlen(prompt));
	ret = ssh_read_line(buf, size, TERM_READ_NO_ECHO);
	ssh_write_all("\n", 1);
	return (ret);
}

static int
ssh_confirm_new_host(const char *host, const char *fingerprint)
{
	char	answer[16];
	int	ret;

	printf("The authenticity of host '%s' cannot be established.\n", host);
	printf("ED25519 key fingerprint is SHA256-hex:%s.\n", fingerprint);
	printf("Are you sure you want to continue connecting (yes/no)? ");
	fflush(stdout);
	ret = ssh_read_line(answer, sizeof(answer), 0);
	if (ret < 0) {
		printf("\n");
		return (-1);
	}
	printf("\n");
	if (strcmp(answer, "yes") == 0) {
		return (0);
	}
	return (-1);
}

static int
ssh_check_known_host(const ssh_options *opts,
    const lssh_client_handshake_result *result)
{
	lssh_knownhost_result	kh;
	lssh_slice		host_key;
	char			fp[LSSH_FINGERPRINT_HEX_MAX];
	int			ret;

	host_key.data = result->host_key_blob;
	host_key.len = result->host_key_blob_len;
	ret = lssh_knownhost_fingerprint_hex(host_key, fp, sizeof(fp));
	if (ret != LSSH_OK) {
		printf("ssh: host key fingerprint failed: %s\n",
		    ssh_error_name(ret));
		return (-1);
	}
	ret = lssh_knownhosts_check(SSH_KNOWN_HOSTS, opts->host,
	    opts->port, host_key, &kh);
	if (ret != LSSH_OK) {
		printf("ssh: known_hosts check failed: %s\n",
		    ssh_error_name(ret));
		return (-1);
	}
	if (kh.status == LSSH_KNOWNHOST_TRUSTED) {
		return (0);
	}
	if (kh.status == LSSH_KNOWNHOST_CHANGED) {
		printf("ssh: WARNING: REMOTE HOST IDENTIFICATION CHANGED!\n");
		printf("ssh: offending key for %s at %s:%u\n",
		    kh.host, SSH_KNOWN_HOSTS, (unsigned int)kh.line);
		printf("ssh: presented ED25519 fingerprint SHA256-hex:%s\n",
		    fp);
		return (-1);
	}
	if (ssh_confirm_new_host(kh.host, fp) != 0) {
		printf("ssh: host key not accepted\n");
		return (-1);
	}
	ret = lssh_knownhosts_add(SSH_KNOWN_HOSTS, opts->host,
	    opts->port, host_key);
	if (ret != LSSH_OK) {
		printf("ssh: failed to write %s: %s\n", SSH_KNOWN_HOSTS,
		    ssh_error_name(ret));
		return (-1);
	}
	printf("Warning: Permanently added '%s' to known hosts.\n", kh.host);
	return (0);
}

static int
ssh_auth_password(lssh_transport *transport, const ssh_options *opts)
{
	char	password[SSH_PASSWORD_MAX];
	char	prompt[SSH_PROMPT_MAX];
	int	attempt, ret, n;

	ret = LSSH_ERR_AUTH;
	for (attempt = 0; attempt < SSH_AUTH_TRIES; attempt++) {
		n = snprintf(prompt, sizeof(prompt), "%s@%s's password: ",
		    opts->user, opts->host);
		if (n < 0 || (size_t)n >= sizeof(prompt)) {
			return (LSSH_ERR_RANGE);
		}
		n = ssh_read_password(prompt, password, sizeof(password));
		if (n < 0) {
			lc_wipe(password, sizeof(password));
			return (LSSH_ERR_IO);
		}
		ret = lssh_client_auth_password(transport, opts->user,
		    password);
		lc_wipe(password, sizeof(password));
		if (ret == LSSH_OK) {
			return (LSSH_OK);
		}
		if (ret != LSSH_ERR_AUTH) {
			return (ret);
		}
		printf("Permission denied, try again.\n");
	}
	return (ret);
}

static void
ssh_term_size(uint32_t *cols, uint32_t *rows, uint32_t *xpixel,
    uint32_t *ypixel)
{
	struct api_term_info	info;

	*cols = 80;
	*rows = 25;
	*xpixel = 0;
	*ypixel = 0;
	if (termInfo(&info) != 0) {
		return;
	}
	if (info.cols != 0) {
		*cols = info.cols;
	}
	if (info.rows != 0) {
		*rows = info.rows;
	}
	*xpixel = info.xpixel;
	*ypixel = info.ypixel;
}

static int
ssh_channel_replenish(lssh_transport *transport, lssh_channel *channel)
{
	uint32_t	adjust;

	if (!channel->open ||
	    channel->local_window >= LSSH_CHANNEL_DEFAULT_WINDOW / 2) {
		return (LSSH_OK);
	}
	adjust = LSSH_CHANNEL_DEFAULT_WINDOW - channel->local_window;
	return (lssh_client_channel_adjust_window(transport, channel,
	    adjust));
}

static int
ssh_handle_channel_event(lssh_transport *transport, lssh_channel *channel,
    const lssh_channel_event *event, ssh_run_state *state)
{
	int	ret;

	ret = LSSH_OK;
	if (event->type == LSSH_MSG_CHANNEL_DATA) {
		if (ssh_write_all(event->data.data, event->data.len) != 0) {
			return (LSSH_ERR_IO);
		}
		ret = ssh_channel_replenish(transport, channel);
	} else if (event->type == LSSH_MSG_CHANNEL_EXTENDED_DATA) {
		if (ssh_write_all(event->data.data, event->data.len) != 0) {
			return (LSSH_ERR_IO);
		}
		ret = ssh_channel_replenish(transport, channel);
	} else if (event->type == LSSH_MSG_CHANNEL_REQUEST &&
	    event->has_exit_status) {
		state->exit_status = (int)event->exit_status;
	} else if (event->type == LSSH_MSG_CHANNEL_CLOSE) {
		state->done = 1;
		if (!channel->close_sent) {
			(void)lssh_client_channel_close(transport, channel);
		}
	} else if (event->type == LSSH_MSG_CHANNEL_EOF) {
		channel->eof_received = 1;
	}
	return (ret);
}

static int
ssh_recv_channel_event(lssh_transport *transport, lssh_channel *channel,
    lssh_buf *buf, ssh_run_state *state)
{
	lssh_channel_event	event;
	int			ret;

	ret = lssh_client_channel_recv_event(transport, channel, buf,
	    &event);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (ssh_handle_channel_event(transport, channel, &event,
	    state));
}

static int
ssh_send_channel_data(lssh_transport *transport, lssh_channel *channel,
    const void *data, size_t len)
{
	const uint8_t	*p;
	size_t		off, chunk, max_chunk;
	int		ret;

	if (!data && len != 0) {
		return (LSSH_ERR_INVALID);
	}
	p = (const uint8_t *)data;
	off = 0;
	while (off < len) {
		max_chunk = channel->remote_max_packet;
		if (max_chunk > channel->remote_window) {
			max_chunk = channel->remote_window;
		}
		if (max_chunk == 0) {
			return (LSSH_ERR_AGAIN);
		}
		chunk = len - off;
		if (chunk > max_chunk) {
			chunk = max_chunk;
		}
		ret = lssh_client_channel_send_data(transport, channel,
		    p + off, chunk);
		if (ret != LSSH_OK) {
			return (ret);
		}
		off += chunk;
	}
	return (LSSH_OK);
}

static int
ssh_register_events(int kq, int net_handle)
{
	struct kevent	changes[2];
	int		ret;

	memset(changes, 0, sizeof(changes));
	changes[0].ident = SSH_STDIN_IDENT;
	changes[0].filter = EVFILT_READ;
	changes[0].flags = EV_ADD | EV_CLEAR;
	changes[1].ident = (uint64_t)net_handle;
	changes[1].filter = EVFILT_READ;
	changes[1].flags = EV_ADD | EV_CLEAR;
	ret = eventWait(kq, changes, 2, NULL, 0, -1);
	if (ret < 0) {
		return (-1);
	}
	return (0);
}

static int
ssh_run_interactive(lssh_transport *transport, lssh_channel *channel)
{
	struct api_term_mode	saved_mode;
	struct kevent		event;
	lssh_buf		buf;
	ssh_run_state		state;
	char			input[SSH_STDIN_BUF];
	size_t			want;
	ssize_t			n;
	int			kq, pending, ret, raw;

	memset(&state, 0, sizeof(state));
	state.exit_status = 0;
	raw = 0;
	kq = -1;
	ret = lssh_buf_init(&buf, 4096);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (termEnterRaw(&saved_mode) != 0) {
		lssh_buf_free(&buf);
		return (LSSH_ERR_IO);
	}
	raw = 1;
	kq = eventKqueue();
	if (kq < 0) {
		ret = LSSH_ERR_IO;
		goto out;
	}
	if (ssh_register_events(kq, transport->handle) != 0) {
		ret = LSSH_ERR_IO;
		goto out;
	}

	while (!state.done) {
		pending = lssh_transport_packet_pending(transport);
		if (pending < 0) {
			ret = pending;
			break;
		}
		if (pending > 0) {
			ret = ssh_recv_channel_event(transport, channel,
			    &buf, &state);
			if (ret != LSSH_OK) {
				break;
			}
			continue;
		}

		ret = eventWait(kq, NULL, 0, &event, 1, -1);
		if (ret < 0) {
			ret = LSSH_ERR_IO;
			break;
		}
		if (ret == 0 || event.filter != EVFILT_READ) {
			continue;
		}
		if (event.ident == SSH_STDIN_IDENT) {
			want = sizeof(input);
			if (event.data > 0 &&
			    (uint64_t)event.data < (uint64_t)want) {
				want = (size_t)event.data;
			}
			n = termReadFlags(input, want,
			    TERM_READ_IGNORE_SIGINT);
			if (n < 0) {
				ret = LSSH_ERR_IO;
				break;
			}
			if (n == 0) {
				ret = lssh_client_channel_send_eof(transport,
				    channel);
				if (ret != LSSH_OK) {
					break;
				}
				continue;
			}
			ret = ssh_send_channel_data(transport, channel,
			    input, (size_t)n);
			if (ret != LSSH_OK) {
				break;
			}
		} else if (event.ident == (uint64_t)transport->handle) {
			ret = ssh_recv_channel_event(transport, channel,
			    &buf, &state);
			if (ret != LSSH_OK) {
				break;
			}
		}
	}

out:
	if (kq >= 0) {
		eventClose(kq);
	}
	if (raw) {
		termRestoreMode(&saved_mode);
	}
	lssh_buf_free(&buf);
	if (ret == LSSH_OK) {
		return (state.exit_status);
	}
	return (ret);
}

static int
ssh_run_exec_wait(lssh_transport *transport, lssh_channel *channel)
{
	lssh_buf	buf;
	ssh_run_state	state;
	int		ret;

	memset(&state, 0, sizeof(state));
	state.exit_status = 0;
	ret = lssh_buf_init(&buf, 4096);
	if (ret != LSSH_OK) {
		return (ret);
	}
	while (!state.done) {
		ret = ssh_recv_channel_event(transport, channel, &buf,
		    &state);
		if (ret != LSSH_OK) {
			break;
		}
	}
	lssh_buf_free(&buf);
	if (ret == LSSH_OK) {
		return (state.exit_status);
	}
	return (ret);
}

static int
ssh_open_and_run(lssh_transport *transport, const ssh_options *opts)
{
	lssh_channel	channel;
	uint32_t	cols, rows, xpixel, ypixel;
	int		ret;

	ret = lssh_client_open_session(transport, &channel, 0);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (opts->use_pty) {
		ssh_term_size(&cols, &rows, &xpixel, &ypixel);
		ret = lssh_client_channel_request_pty(transport, &channel,
		    "xterm", cols, rows, xpixel, ypixel);
		if (ret != LSSH_OK) {
			printf("ssh: warning: pty request failed: %s\n",
			    ssh_error_name(ret));
		}
	}
	if (opts->has_command) {
		ret = lssh_client_channel_request_exec(transport, &channel,
		    opts->command);
		if (ret != LSSH_OK) {
			return (ret);
		}
		if (opts->use_pty) {
			return (ssh_run_interactive(transport, &channel));
		}
		return (ssh_run_exec_wait(transport, &channel));
	}
	ret = lssh_client_channel_request_shell(transport, &channel);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (ssh_run_interactive(transport, &channel));
}

int
main(int argc, char **argv, char **envp)
{
	lssh_client_handshake_result	handshake;
	lssh_transport			transport;
	ssh_options			opts;
	int				ret, parse_ret;

	(void)envp;
	personality(0);

	parse_ret = ssh_parse_args(argc, argv, &opts);
	if (parse_ret != 0) {
		ssh_usage();
		return (parse_ret > 0 ? 0 : 1);
	}
	if (opts.verbose > 0) {
		lssh_log_set(opts.verbose > 1 ? LSSH_LOG_DEBUG :
		    LSSH_LOG_INFO, ssh_log_callback, NULL);
	}

	ret = lssh_transport_init(&transport);
	if (ret != LSSH_OK) {
		printf("ssh: transport init failed: %s\n",
		    ssh_error_name(ret));
		return (1);
	}
	printf("ssh: connecting to %s:%u\n", opts.host,
	    (unsigned int)opts.port);
	ret = lssh_transport_connect_ipv4(&transport, opts.ip, opts.port);
	if (ret != LSSH_OK) {
		printf("ssh: connect failed: %s\n", ssh_error_name(ret));
		lssh_transport_free(&transport);
		return (1);
	}
	ret = lssh_client_handshake(&transport, &handshake, NULL, NULL);
	if (ret != LSSH_OK) {
		printf("ssh: handshake failed: %s\n", ssh_error_name(ret));
		lssh_transport_free(&transport);
		return (1);
	}
	if (ssh_check_known_host(&opts, &handshake) != 0) {
		lssh_transport_free(&transport);
		return (1);
	}
	ret = ssh_auth_password(&transport, &opts);
	if (ret != LSSH_OK) {
		printf("ssh: authentication failed: %s\n",
		    ssh_error_name(ret));
		lssh_transport_free(&transport);
		return (1);
	}
	ret = ssh_open_and_run(&transport, &opts);
	lssh_transport_free(&transport);
	if (ret == LSSH_OK) {
		return (0);
	}
	if (ret > 0) {
		return (ret > 255 ? 255 : ret);
	}
	printf("ssh: session failed: %s\n", ssh_error_name(ret));
	return (1);
}
