/* !DEFINES!

$define %type api_net_addr as native IPv4 endpoint address
$define %type api_net_iface as native interface snapshot
$define %type api_net_msg as native network message descriptor
$define %type api_timeinfo as native time data
$define %type dhcp_packet as DHCP/BOOTP packet
$define %type dhcp_config as daemon runtime configuration
$define %type dhcp_lease as parsed DHCP lease data
$define %func load_be32 as function with args const uint8_t *
$define %func store_be16 as procedure with args uint8_t *, uint16_t
$define %func store_be32 as procedure with args uint8_t *, uint32_t
$define %func now_ms as function with args void
$define %func ip_text as procedure with args char *, size_t, uint32_t
$define %func random_xid as function with args void
$define %func dhcp_put as function with args uint8_t *, uint32_t *, uint32_t, uint8_t, const void *, uint8_t
$define %func dhcp_put_u8 as function with args uint8_t *, uint32_t *, uint32_t, uint8_t, uint8_t
$define %func dhcp_put_u16 as function with args uint8_t *, uint32_t *, uint32_t, uint8_t, uint16_t
$define %func dhcp_put_u32 as function with args uint8_t *, uint32_t *, uint32_t, uint8_t, uint32_t
$define %func dhcp_put_common as function with args uint8_t *, uint32_t *, uint32_t, const api_net_iface *
$define %func dhcp_base as procedure with args dhcp_packet *, const api_net_iface *, uint32_t, uint32_t, int
$define %func dhcp_finish as function with args dhcp_packet *, uint32_t
$define %func dhcp_build_discover as function with args uint8_t *, const api_net_iface *, uint32_t
$define %func dhcp_build_request as function with args uint8_t *, const api_net_iface *, uint32_t, const dhcp_lease *, int
$define %func dhcp_parse as function with args const uint8_t *, size_t, uint32_t, const api_net_iface *, dhcp_lease *
$define %func dhcp_merge as procedure with args dhcp_lease *, const dhcp_lease *
$define %func dhcp_read_config as procedure with args dhcp_config *
$define %func dhcp_get_iface as function with args api_net_iface *
$define %func dhcp_open_socket as function with args const api_net_iface *
$define %func dhcp_attach_read as function with args int, int
$define %func dhcp_send as function with args int, const api_net_iface *, uint32_t, const uint8_t *, size_t
$define %func dhcp_wait as function with args int, int, const api_net_iface *, uint32_t, int, int, uint32_t, dhcp_lease *
$define %func dhcp_drain as procedure with args int
$define %func dhcp_sleep as procedure with args int, int, uint32_t
$define %func dhcp_set_dns as function with args const dhcp_lease *
$define %func dhcp_write_registry as function with args const api_net_iface *, const dhcp_lease *
$define %func dhcp_discover as function with args int, int, const api_net_iface *, const dhcp_config *, dhcp_lease *
$define %func dhcp_renew as function with args int, int, const api_net_iface *, const dhcp_config *, const dhcp_lease *, dhcp_lease *
$define %func daemonize as function with args void
$define %func has_arg as function with args int, char **, const char *
$define %func serve as function with args void
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal load_be32, store_be16, store_be32
$space %internal now_ms, ip_text, random_xid
$space %internal dhcp_put, dhcp_put_u8, dhcp_put_u16, dhcp_put_u32
$space %internal dhcp_put_common, dhcp_base, dhcp_finish
$space %internal dhcp_build_discover, dhcp_build_request, dhcp_parse
$space %internal dhcp_merge, dhcp_read_config, dhcp_get_iface
$space %internal dhcp_open_socket, dhcp_attach_read, dhcp_send
$space %internal dhcp_wait, dhcp_drain, dhcp_sleep, dhcp_set_dns
$space %internal dhcp_write_registry, dhcp_discover, dhcp_renew
$space %internal daemonize, has_arg, serve
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define	DHCP_CLIENT_PORT	68
#define	DHCP_SERVER_PORT	67
#define	DHCP_BROADCAST		0xFFFFFFFFu
#define	DHCP_FIXED_LEN		236
#define	DHCP_MIN_PACKET		300
#define	DHCP_OPTIONS_LEN	312
#define	DHCP_PACKET_LEN		(DHCP_FIXED_LEN + DHCP_OPTIONS_LEN)
#define	DHCP_MAGIC0		99
#define	DHCP_MAGIC1		130
#define	DHCP_MAGIC2		83
#define	DHCP_MAGIC3		99
#define	DHCP_OP_REQUEST		1
#define	DHCP_OP_REPLY		2
#define	DHCP_HTYPE_ETH		1
#define	DHCP_HLEN_ETH		6
#define	DHCP_FLAG_BROADCAST	0x8000
#define	DHCP_OPT_PAD		0
#define	DHCP_OPT_NETMASK	1
#define	DHCP_OPT_ROUTER		3
#define	DHCP_OPT_DNS		6
#define	DHCP_OPT_HOSTNAME	12
#define	DHCP_OPT_REQ_IP		50
#define	DHCP_OPT_LEASE		51
#define	DHCP_OPT_MSG_TYPE	53
#define	DHCP_OPT_SERVER_ID	54
#define	DHCP_OPT_PARAM_REQ	55
#define	DHCP_OPT_MAX_SIZE	57
#define	DHCP_OPT_RENEW		58
#define	DHCP_OPT_REBIND		59
#define	DHCP_OPT_CLIENT_ID	61
#define	DHCP_OPT_END		255
#define	DHCP_MSG_DISCOVER	1
#define	DHCP_MSG_OFFER		2
#define	DHCP_MSG_REQUEST	3
#define	DHCP_MSG_ACK		5
#define	DHCP_MSG_NAK		6
#define	DHCP_DEFAULT_TIMEOUT	4000
#define	DHCP_DEFAULT_RETRIES	3
#define	DHCP_DEFAULT_LEASE	3600
#define	DHCP_BACKOFF_MS		10000
#define	DHCP_MAX_DNS		4

typedef struct dhcp_packet {
	uint8_t	op;
	uint8_t	htype;
	uint8_t	hlen;
	uint8_t	hops;
	uint8_t	xid[4];
	uint8_t	secs[2];
	uint8_t	flags[2];
	uint8_t	ciaddr[4];
	uint8_t	yiaddr[4];
	uint8_t	siaddr[4];
	uint8_t	giaddr[4];
	uint8_t	chaddr[16];
	uint8_t	sname[64];
	uint8_t	file[128];
	uint8_t	options[DHCP_OPTIONS_LEN];
} dhcp_packet_t;

typedef struct dhcp_config {
	uint32_t	timeout_ms;
	uint32_t	retries;
} dhcp_config_t;

typedef struct dhcp_lease {
	uint32_t	address;
	uint32_t	server;
	uint32_t	netmask;
	uint32_t	router;
	uint32_t	lease_seconds;
	uint32_t	t1_seconds;
	uint32_t	t2_seconds;
	uint32_t	dns[DHCP_MAX_DNS];
	uint32_t	dns_count;
	uint8_t		message_type;
} dhcp_lease_t;

static uint32_t
load_be32(const uint8_t *p)
{
	return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static void
store_be16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)((v >> 8) & 0xFF);
	p[1] = (uint8_t)(v & 0xFF);
}

static void
store_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)((v >> 24) & 0xFF);
	p[1] = (uint8_t)((v >> 16) & 0xFF);
	p[2] = (uint8_t)((v >> 8) & 0xFF);
	p[3] = (uint8_t)(v & 0xFF);
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

static void
ip_text(char *buf, size_t size, uint32_t ip)
{
	snprintf(buf, size, "%u.%u.%u.%u",
	    (unsigned int)((ip >> 24) & 0xFF),
	    (unsigned int)((ip >> 16) & 0xFF),
	    (unsigned int)((ip >> 8) & 0xFF),
	    (unsigned int)(ip & 0xFF));
}

static uint32_t
random_xid(void)
{
	struct api_timeinfo	ti;
	uint32_t		xid;

	xid = 0;
	if (sysRandom(&xid, sizeof(xid)) == 0 && xid != 0) {
		return (xid);
	}
	if (sysTimeInfo(&ti) == 0) {
		xid = (uint32_t)ti.ticks ^ (uint32_t)ti.uptime_nsec;
		xid ^= (uint32_t)procGetpid() << 16;
	}
	if (xid == 0) {
		xid = 0x4F545344u;
	}
	return (xid);
}

static int
dhcp_put(uint8_t *opts, uint32_t *pos, uint32_t cap, uint8_t code,
    const void *data, uint8_t len)
{
	if (!opts || !pos || !data || *pos + 2u + len > cap) {
		return (-1);
	}

	opts[*pos] = code;
	opts[*pos + 1] = len;
	memcpy(opts + *pos + 2, data, len);
	*pos = *pos + 2u + len;
	return (0);
}

static int
dhcp_put_u8(uint8_t *opts, uint32_t *pos, uint32_t cap, uint8_t code,
    uint8_t value)
{
	uint8_t	data[1];

	data[0] = value;
	return (dhcp_put(opts, pos, cap, code, data, sizeof(data)));
}

static int
dhcp_put_u16(uint8_t *opts, uint32_t *pos, uint32_t cap, uint8_t code,
    uint16_t value)
{
	uint8_t	data[2];

	store_be16(data, value);
	return (dhcp_put(opts, pos, cap, code, data, sizeof(data)));
}

static int
dhcp_put_u32(uint8_t *opts, uint32_t *pos, uint32_t cap, uint8_t code,
    uint32_t value)
{
	uint8_t	data[4];

	store_be32(data, value);
	return (dhcp_put(opts, pos, cap, code, data, sizeof(data)));
}

static int
dhcp_put_common(uint8_t *opts, uint32_t *pos, uint32_t cap,
    const struct api_net_iface *iface)
{
	uint8_t	params[7];
	uint8_t	client[7];

	params[0] = DHCP_OPT_NETMASK;
	params[1] = DHCP_OPT_ROUTER;
	params[2] = DHCP_OPT_DNS;
	params[3] = DHCP_OPT_LEASE;
	params[4] = DHCP_OPT_RENEW;
	params[5] = DHCP_OPT_REBIND;
	params[6] = DHCP_OPT_HOSTNAME;

	client[0] = DHCP_HTYPE_ETH;
	memcpy(client + 1, iface->mac, 6);

	if (dhcp_put(opts, pos, cap, DHCP_OPT_PARAM_REQ,
	    params, sizeof(params)) != 0) {
		return (-1);
	}
	if (dhcp_put(opts, pos, cap, DHCP_OPT_CLIENT_ID,
	    client, sizeof(client)) != 0) {
		return (-1);
	}
	if (dhcp_put(opts, pos, cap, DHCP_OPT_HOSTNAME,
	    "otsos2", 6) != 0) {
		return (-1);
	}
	return (dhcp_put_u16(opts, pos, cap, DHCP_OPT_MAX_SIZE,
	    DHCP_PACKET_LEN));
}

static void
dhcp_base(dhcp_packet_t *pkt, const struct api_net_iface *iface,
    uint32_t xid, uint32_t ciaddr, int broadcast)
{
	memset(pkt, 0, sizeof(*pkt));
	pkt->op = DHCP_OP_REQUEST;
	pkt->htype = DHCP_HTYPE_ETH;
	pkt->hlen = DHCP_HLEN_ETH;
	store_be32(pkt->xid, xid);
	if (broadcast) {
		store_be16(pkt->flags, DHCP_FLAG_BROADCAST);
	}
	if (ciaddr != 0) {
		store_be32(pkt->ciaddr, ciaddr);
	}
	memcpy(pkt->chaddr, iface->mac, 6);
	pkt->options[0] = DHCP_MAGIC0;
	pkt->options[1] = DHCP_MAGIC1;
	pkt->options[2] = DHCP_MAGIC2;
	pkt->options[3] = DHCP_MAGIC3;
}

static size_t
dhcp_finish(dhcp_packet_t *pkt, uint32_t pos)
{
	size_t	total;

	if (pos >= DHCP_OPTIONS_LEN) {
		return (0);
	}
	pkt->options[pos++] = DHCP_OPT_END;

	total = DHCP_FIXED_LEN + pos;
	if (total < DHCP_MIN_PACKET) {
		total = DHCP_MIN_PACKET;
	}
	return (total);
}

static size_t
dhcp_build_discover(uint8_t *buf, const struct api_net_iface *iface,
    uint32_t xid)
{
	dhcp_packet_t	*pkt;
	uint32_t	pos;

	pkt = (dhcp_packet_t *)buf;
	dhcp_base(pkt, iface, xid, 0, 1);
	pos = 4;

	if (dhcp_put_u8(pkt->options, &pos, DHCP_OPTIONS_LEN,
	    DHCP_OPT_MSG_TYPE, DHCP_MSG_DISCOVER) != 0) {
		return (0);
	}
	if (dhcp_put_common(pkt->options, &pos, DHCP_OPTIONS_LEN,
	    iface) != 0) {
		return (0);
	}
	return (dhcp_finish(pkt, pos));
}

static size_t
dhcp_build_request(uint8_t *buf, const struct api_net_iface *iface,
    uint32_t xid, const dhcp_lease_t *lease, int renew)
{
	dhcp_packet_t	*pkt;
	uint32_t	pos;

	pkt = (dhcp_packet_t *)buf;
	dhcp_base(pkt, iface, xid, renew ? lease->address : 0,
	    renew ? 0 : 1);
	pos = 4;

	if (dhcp_put_u8(pkt->options, &pos, DHCP_OPTIONS_LEN,
	    DHCP_OPT_MSG_TYPE, DHCP_MSG_REQUEST) != 0) {
		return (0);
	}
	if (!renew) {
		if (dhcp_put_u32(pkt->options, &pos, DHCP_OPTIONS_LEN,
		    DHCP_OPT_REQ_IP, lease->address) != 0) {
			return (0);
		}
		if (lease->server != 0 &&
		    dhcp_put_u32(pkt->options, &pos, DHCP_OPTIONS_LEN,
		    DHCP_OPT_SERVER_ID, lease->server) != 0) {
			return (0);
		}
	}
	if (dhcp_put_common(pkt->options, &pos, DHCP_OPTIONS_LEN,
	    iface) != 0) {
		return (0);
	}
	return (dhcp_finish(pkt, pos));
}

static int
dhcp_parse(const uint8_t *buf, size_t len, uint32_t xid,
    const struct api_net_iface *iface, dhcp_lease_t *lease)
{
	const dhcp_packet_t	*pkt;
	const uint8_t		*opts;
	uint32_t		pos, opt_len, dns_pos, count;
	uint8_t			code, olen;

	if (!buf || len < DHCP_FIXED_LEN + 4 || !iface || !lease) {
		return (0);
	}

	pkt = (const dhcp_packet_t *)buf;
	if (pkt->op != DHCP_OP_REPLY || pkt->htype != DHCP_HTYPE_ETH ||
	    pkt->hlen != DHCP_HLEN_ETH || load_be32(pkt->xid) != xid) {
		return (0);
	}
	if (memcmp(pkt->chaddr, iface->mac, 6) != 0) {
		return (0);
	}

	opts = pkt->options;
	if (opts[0] != DHCP_MAGIC0 || opts[1] != DHCP_MAGIC1 ||
	    opts[2] != DHCP_MAGIC2 || opts[3] != DHCP_MAGIC3) {
		return (0);
	}

	memset(lease, 0, sizeof(*lease));
	lease->address = load_be32(pkt->yiaddr);
	pos = 4;
	opt_len = (uint32_t)(len - DHCP_FIXED_LEN);
	while (pos < opt_len) {
		code = opts[pos++];
		if (code == DHCP_OPT_PAD) {
			continue;
		}
		if (code == DHCP_OPT_END) {
			break;
		}
		if (pos >= opt_len) {
			break;
		}
		olen = opts[pos++];
		if (pos + olen > opt_len) {
			break;
		}

		switch (code) {
		case DHCP_OPT_MSG_TYPE:
			if (olen >= 1) {
				lease->message_type = opts[pos];
			}
			break;
		case DHCP_OPT_SERVER_ID:
			if (olen == 4) {
				lease->server = load_be32(opts + pos);
			}
			break;
		case DHCP_OPT_NETMASK:
			if (olen == 4) {
				lease->netmask = load_be32(opts + pos);
			}
			break;
		case DHCP_OPT_ROUTER:
			if (olen >= 4) {
				lease->router = load_be32(opts + pos);
			}
			break;
		case DHCP_OPT_DNS:
			count = olen / 4;
			if (count > DHCP_MAX_DNS) {
				count = DHCP_MAX_DNS;
			}
			lease->dns_count = count;
			for (dns_pos = 0; dns_pos < count; dns_pos++) {
				lease->dns[dns_pos] =
				    load_be32(opts + pos + dns_pos * 4);
			}
			break;
		case DHCP_OPT_LEASE:
			if (olen == 4) {
				lease->lease_seconds = load_be32(opts + pos);
			}
			break;
		case DHCP_OPT_RENEW:
			if (olen == 4) {
				lease->t1_seconds = load_be32(opts + pos);
			}
			break;
		case DHCP_OPT_REBIND:
			if (olen == 4) {
				lease->t2_seconds = load_be32(opts + pos);
			}
			break;
		default:
			break;
		}
		pos += olen;
	}

	return (lease->message_type != 0);
}

static void
dhcp_merge(dhcp_lease_t *lease, const dhcp_lease_t *fallback)
{
	uint32_t	i;

	if (!lease || !fallback) {
		return;
	}
	if (lease->address == 0) {
		lease->address = fallback->address;
	}
	if (lease->server == 0) {
		lease->server = fallback->server;
	}
	if (lease->netmask == 0) {
		lease->netmask = fallback->netmask;
	}
	if (lease->router == 0) {
		lease->router = fallback->router;
	}
	if (lease->lease_seconds == 0) {
		lease->lease_seconds = fallback->lease_seconds;
	}
	if (lease->t1_seconds == 0) {
		lease->t1_seconds = fallback->t1_seconds;
	}
	if (lease->t2_seconds == 0) {
		lease->t2_seconds = fallback->t2_seconds;
	}
	if (lease->dns_count == 0) {
		lease->dns_count = fallback->dns_count;
		for (i = 0; i < fallback->dns_count && i < DHCP_MAX_DNS; i++) {
			lease->dns[i] = fallback->dns[i];
		}
	}
}

static void
dhcp_read_config(dhcp_config_t *cfg)
{
	int	reg;

	cfg->timeout_ms = DHCP_DEFAULT_TIMEOUT;
	cfg->retries = DHCP_DEFAULT_RETRIES;

	reg = regOpen("NETWORK", "Dhcp.Client", API_REG_OPEN_READ);
	if (reg < 0) {
		return;
	}
	(void)regGetU32(reg, "TimeoutMs", &cfg->timeout_ms);
	(void)regGetU32(reg, "RetryCount", &cfg->retries);
	regClose(reg);

	if (cfg->timeout_ms < 500) {
		cfg->timeout_ms = 500;
	}
	if (cfg->retries == 0) {
		cfg->retries = 1;
	}
}

static int
dhcp_get_iface(struct api_net_iface *iface)
{
	int	fd;

	memset(iface, 0, sizeof(*iface));
	fd = netOpen(API_NET_PROTO_UDP, API_NET_MODE_DGRAM,
	    API_NET_OPEN_NONBLOCK);
	if (fd < 0) {
		printf("dhcpd: netOpen failed: %d\n", errno);
		return (-1);
	}
	if (netCtl(fd, API_NET_CTL_GET_IFACE, iface) != 0) {
		printf("dhcpd: netCtl(GET_IFACE) failed: %d\n", errno);
		dataClose(fd);
		return (-1);
	}
	dataClose(fd);
	return (0);
}

static int
dhcp_open_socket(const struct api_net_iface *iface)
{
	struct api_net_addr	addr;
	int			fd;

	fd = netOpen(API_NET_PROTO_UDP, API_NET_MODE_DGRAM,
	    API_NET_OPEN_NONBLOCK);
	if (fd < 0) {
		printf("dhcpd: netOpen failed: %d\n", errno);
		return (-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.port = DHCP_CLIENT_PORT;
	addr.ip = 0;
	addr.ifindex = iface->ifindex;

	if (netBind(fd, &addr) != 0) {
		printf("dhcpd: bind 0.0.0.0:%d failed: %d\n",
		    DHCP_CLIENT_PORT, errno);
		dataClose(fd);
		return (-1);
	}
	return (fd);
}

static int
dhcp_attach_read(int kq, int fd)
{
	struct kevent	change;

	memset(&change, 0, sizeof(change));
	change.ident = (uint64_t)fd;
	change.filter = EVFILT_READ;
	change.flags = EV_ADD | EV_CLEAR;
	if (eventWait(kq, &change, 1, NULL, 0, -1) < 0) {
		printf("dhcpd: EVFILT_READ add failed: %d\n", errno);
		return (-1);
	}
	return (0);
}

static int
dhcp_send(int fd, const struct api_net_iface *iface, uint32_t dst_ip,
    const uint8_t *buf, size_t len)
{
	struct api_net_addr	dst;
	struct api_net_msg	msg;
	ssize_t			n;

	memset(&dst, 0, sizeof(dst));
	dst.family = API_NET_ADDR_IP4;
	dst.port = DHCP_SERVER_PORT;
	dst.ip = dst_ip;
	dst.ifindex = iface->ifindex;

	memset(&msg, 0, sizeof(msg));
	msg.data = (void *)buf;
	msg.addr = &dst;
	msg.length = (uint32_t)len;

	n = netSend(fd, &msg);
	if (n < 0 || (size_t)n != len) {
		printf("dhcpd: netSend failed: %d\n", errno);
		return (-1);
	}
	return (0);
}

static int
dhcp_wait(int fd, int kq, const struct api_net_iface *iface,
    uint32_t xid, int want_a, int want_b, uint32_t timeout_ms,
    dhcp_lease_t *lease)
{
	struct api_net_addr	peer;
	struct api_net_msg	msg;
	struct kevent		event;
	dhcp_lease_t		got;
	uint8_t			buf[DHCP_PACKET_LEN];
	uint64_t		end, now;
	ssize_t			n;
	int			ret;

	end = now_ms() + timeout_ms;
	for (;;) {
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
				printf("dhcpd: netRecv failed: %d\n", errno);
				return (-1);
			}
			if (dhcp_parse(buf, (size_t)n, xid, iface, &got)) {
				if ((int)got.message_type == want_a ||
				    (int)got.message_type == want_b) {
					*lease = got;
					return ((int)got.message_type);
				}
			}
		}

		now = now_ms();
		if (now >= end) {
			return (0);
		}
		ret = eventWait(kq, NULL, 0, &event, 1,
		    (int64_t)(end - now));
		if (ret < 0) {
			printf("dhcpd: eventWait failed: %d\n", errno);
			return (-1);
		}
	}
}

static void
dhcp_drain(int fd)
{
	struct api_net_msg	msg;
	uint8_t			buf[256];
	ssize_t			n;

	for (;;) {
		memset(&msg, 0, sizeof(msg));
		msg.data = buf;
		msg.length = sizeof(buf);
		msg.flags = API_NET_MSG_NONBLOCK;
		n = netRecv(fd, &msg);
		if (n < 0) {
			return;
		}
	}
}

static void
dhcp_sleep(int fd, int kq, uint32_t ms)
{
	struct kevent	event;
	uint64_t	end, now;
	int		ret;

	end = now_ms() + ms;
	for (;;) {
		now = now_ms();
		if (now >= end) {
			return;
		}
		ret = eventWait(kq, NULL, 0, &event, 1,
		    (int64_t)(end - now));
		if (ret <= 0) {
			continue;
		}
		if (event.filter == EVFILT_READ) {
			dhcp_drain(fd);
		}
	}
}

static int
dhcp_set_dns(const dhcp_lease_t *lease)
{
	struct api_reg_value	value;
	char			text[16];
	uint8_t			data[DHCP_MAX_DNS * 16];
	uint32_t		i, pos, len;
	int			reg;

	reg = regOpen("NETWORK", "DNS", API_REG_OPEN_RW |
	    API_REG_OPEN_CREATE);
	if (reg < 0) {
		return (-1);
	}

	memset(data, 0, sizeof(data));
	pos = 0;
	if (lease->dns_count == 0) {
		data[pos++] = '\0';
	} else {
		for (i = 0; i < lease->dns_count && i < DHCP_MAX_DNS; i++) {
			ip_text(text, sizeof(text), lease->dns[i]);
			len = (uint32_t)strlen(text) + 1;
			if (pos + len > sizeof(data)) {
				break;
			}
			memcpy(data + pos, text, len);
			pos += len;
		}
	}

	memset(&value, 0, sizeof(value));
	value.name = "Servers";
	value.data = data;
	value.size = pos;
	value.type = API_REG_TYPE_MULTI_STRING;
	if (regSet(reg, &value) != 0) {
		regClose(reg);
		return (-1);
	}

	regClose(reg);
	return (0);
}

static int
dhcp_write_registry(const struct api_net_iface *iface,
    const dhcp_lease_t *lease)
{
	char	key[64];
	char	lease_key[96];
	int	reg, lease_reg, err;

	err = 0;
	snprintf(key, sizeof(key), "Interfaces.%s", iface->name);
	snprintf(lease_key, sizeof(lease_key), "Interfaces.%s.DhcpLease",
	    iface->name);

	reg = regOpen("NETWORK", key, API_REG_OPEN_RW |
	    API_REG_OPEN_CREATE);
	if (reg < 0) {
		return (-1);
	}
	if (regSetBool(reg, "Enabled", 1) != 0 && err == 0) {
		err = errno;
	}
	if (regSetBool(reg, "Dhcp", 1) != 0 && err == 0) {
		err = errno;
	}
	if (regSetIpv4(reg, "Address", lease->address) != 0 && err == 0) {
		err = errno;
	}
	if (regSetIpv4(reg, "Netmask", lease->netmask) != 0 && err == 0) {
		err = errno;
	}
	if (regSetIpv4(reg, "Gateway", lease->router) != 0 && err == 0) {
		err = errno;
	}
	regClose(reg);

	lease_reg = regOpen("NETWORK", lease_key, API_REG_OPEN_RW |
	    API_REG_OPEN_CREATE);
	if (lease_reg < 0 && err == 0) {
		err = errno;
	}
	if (lease_reg >= 0) {
		if (regSetIpv4(lease_reg, "Server",
		    lease->server) != 0 && err == 0) {
			err = errno;
		}
		if (regSetU32(lease_reg, "LeaseSeconds",
		    lease->lease_seconds) != 0 && err == 0) {
			err = errno;
		}
		if (regSetU32(lease_reg, "RenewSeconds",
		    lease->t1_seconds) != 0 && err == 0) {
			err = errno;
		}
		if (regSetU32(lease_reg, "RebindSeconds",
		    lease->t2_seconds) != 0 && err == 0) {
			err = errno;
		}
		regClose(lease_reg);
	}

	if (dhcp_set_dns(lease) != 0 && err == 0) {
		err = errno;
	}
	if (err != 0) {
		errno = err;
		return (-1);
	}
	if (regUpd(API_REG_CONSUMER_NET) != 0) {
		return (-1);
	}
	return (0);
}

static int
dhcp_discover(int fd, int kq, const struct api_net_iface *iface,
    const dhcp_config_t *cfg, dhcp_lease_t *out)
{
	dhcp_lease_t	offer;
	uint8_t		buf[DHCP_PACKET_LEN];
	uint32_t	xid, try_no;
	size_t		len;
	int		ret;

	for (try_no = 0; try_no < cfg->retries; try_no++) {
		xid = random_xid();
		len = dhcp_build_discover(buf, iface, xid);
		if (len == 0) {
			return (-1);
		}
		printf("dhcpd: discover xid=0x%x try=%u\n",
		    (unsigned int)xid, (unsigned int)try_no + 1);
		if (dhcp_send(fd, iface, DHCP_BROADCAST, buf, len) != 0) {
			return (-1);
		}

		ret = dhcp_wait(fd, kq, iface, xid, DHCP_MSG_OFFER, -1,
		    cfg->timeout_ms, &offer);
		if (ret <= 0) {
			continue;
		}
		if (offer.address == 0) {
			continue;
		}

		len = dhcp_build_request(buf, iface, xid, &offer, 0);
		if (len == 0) {
			return (-1);
		}
		if (dhcp_send(fd, iface, DHCP_BROADCAST, buf, len) != 0) {
			return (-1);
		}

		ret = dhcp_wait(fd, kq, iface, xid, DHCP_MSG_ACK,
		    DHCP_MSG_NAK, cfg->timeout_ms, out);
		if (ret == DHCP_MSG_ACK) {
			dhcp_merge(out, &offer);
			return (0);
		}
		if (ret == DHCP_MSG_NAK) {
			return (-1);
		}
	}

	return (-1);
}

static int
dhcp_renew(int fd, int kq, const struct api_net_iface *iface,
    const dhcp_config_t *cfg, const dhcp_lease_t *old, dhcp_lease_t *out)
{
	uint8_t		buf[DHCP_PACKET_LEN];
	uint32_t	xid, dst, try_no;
	size_t		len;
	int		ret;

	for (try_no = 0; try_no < cfg->retries; try_no++) {
		xid = random_xid();
		dst = old->server != 0 ? old->server : DHCP_BROADCAST;
		len = dhcp_build_request(buf, iface, xid, old,
		    dst != DHCP_BROADCAST);
		if (len == 0) {
			return (-1);
		}
		if (dhcp_send(fd, iface, dst, buf, len) != 0) {
			return (-1);
		}

		ret = dhcp_wait(fd, kq, iface, xid, DHCP_MSG_ACK,
		    DHCP_MSG_NAK, cfg->timeout_ms, out);
		if (ret == DHCP_MSG_ACK) {
			dhcp_merge(out, old);
			return (0);
		}
		if (ret == DHCP_MSG_NAK) {
			return (-1);
		}
	}
	return (-1);
}

static int
daemonize(void)
{
	int	pid;

	pid = procCopy();
	if (pid < 0) {
		printf("dhcpd: first fork failed: %d\n", errno);
		return (-1);
	}
	if (pid > 0) {
		procExit(0);
	}

	if (procSetsid() < 0) {
		printf("dhcpd: setsid failed: %d\n", errno);
		return (-1);
	}

	pid = procCopy();
	if (pid < 0) {
		printf("dhcpd: second fork failed: %d\n", errno);
		return (-1);
	}
	if (pid > 0) {
		procExit(0);
	}

	return (0);
}

static int
has_arg(int argc, char **argv, const char *needle)
{
	int	i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], needle) == 0) {
			return (1);
		}
	}
	return (0);
}

static int
serve(void)
{
	struct api_net_iface	iface;
	dhcp_config_t		cfg;
	dhcp_lease_t		lease;
	dhcp_lease_t		next;
	char			addr[16];
	char			server[16];
	uint32_t		wait_ms;
	int			fd, kq;

	dhcp_read_config(&cfg);
	if (dhcp_get_iface(&iface) != 0) {
		return (1);
	}

	fd = dhcp_open_socket(&iface);
	if (fd < 0) {
		return (1);
	}
	kq = eventKqueue();
	if (kq < 0) {
		printf("dhcpd: eventKqueue failed: %d\n", errno);
		dataClose(fd);
		return (1);
	}
	if (dhcp_attach_read(kq, fd) != 0) {
		eventClose(kq);
		dataClose(fd);
		return (1);
	}

	printf("dhcpd: using %s ifindex=%u "
	    "%02x:%02x:%02x:%02x:%02x:%02x\n",
	    iface.name, (unsigned int)iface.ifindex,
	    iface.mac[0], iface.mac[1], iface.mac[2], iface.mac[3],
	    iface.mac[4], iface.mac[5]);
	if (procPerm((uint32_t)procGetpid()) != API_PROC_PERM_KUSR) {
		printf("dhcpd: warning: not kusr, registry writes will fail\n");
	}

	for (;;) {
		memset(&lease, 0, sizeof(lease));
		if (dhcp_discover(fd, kq, &iface, &cfg, &lease) != 0) {
			printf("dhcpd: lease discovery failed\n");
			dhcp_sleep(fd, kq, DHCP_BACKOFF_MS);
			continue;
		}

		if (lease.netmask == 0) {
			lease.netmask = 0xFFFFFF00u;
		}
		if (lease.lease_seconds == 0) {
			lease.lease_seconds = DHCP_DEFAULT_LEASE;
		}
		if (lease.t1_seconds == 0) {
			lease.t1_seconds = lease.lease_seconds / 2;
		}
		if (lease.t2_seconds == 0) {
			lease.t2_seconds = (lease.lease_seconds * 7) / 8;
		}

		ip_text(addr, sizeof(addr), lease.address);
		ip_text(server, sizeof(server), lease.server);
		printf("dhcpd: lease %s server %s lease=%u t1=%u\n",
		    addr, server, (unsigned int)lease.lease_seconds,
		    (unsigned int)lease.t1_seconds);

		if (dhcp_write_registry(&iface, &lease) != 0) {
			printf("dhcpd: registry update failed: %d\n", errno);
			dhcp_sleep(fd, kq, DHCP_BACKOFF_MS);
			continue;
		}

		for (;;) {
			wait_ms = lease.t1_seconds * 1000u;
			if (wait_ms < 1000u) {
				wait_ms = 1000u;
			}
			dhcp_sleep(fd, kq, wait_ms);

			memset(&next, 0, sizeof(next));
			if (dhcp_renew(fd, kq, &iface, &cfg, &lease,
			    &next) != 0) {
				printf("dhcpd: renew failed, rediscovering\n");
				break;
			}
			lease = next;
			if (lease.netmask == 0) {
				lease.netmask = 0xFFFFFF00u;
			}
			if (lease.lease_seconds == 0) {
				lease.lease_seconds = DHCP_DEFAULT_LEASE;
			}
			if (lease.t1_seconds == 0) {
				lease.t1_seconds = lease.lease_seconds / 2;
			}
			if (lease.t2_seconds == 0) {
				lease.t2_seconds =
				    (lease.lease_seconds * 7) / 8;
			}
			if (dhcp_write_registry(&iface, &lease) != 0) {
				printf("dhcpd: registry renew write failed: %d\n",
				    errno);
				break;
			}
			ip_text(addr, sizeof(addr), lease.address);
			printf("dhcpd: renewed %s lease=%u t1=%u\n",
			    addr, (unsigned int)lease.lease_seconds,
			    (unsigned int)lease.t1_seconds);
		}
	}
}

int
main(int argc, char **argv, char **envp)
{
	int	foreground;

	(void)envp;
	personality(0);

	foreground = has_arg(argc, argv, "--foreground");
	if (!foreground && daemonize() != 0) {
		return (1);
	}
	return (serve());
}
