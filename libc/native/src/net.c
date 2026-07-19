/* !DEFINES!

$define %type uint32_t as 32 bit unsigned
$define %type api_net_addr as native IPv4 endpoint address
$define %type api_net_msg as native network message descriptor
$define %func netOpen as function with args int, int, uint32_t
$define %func netBind as function with args int, const api_net_addr *
$define %func netConnect as function with args int, const api_net_addr *
$define %func netListen as function with args int, int
$define %func netAccept as function with args int, api_net_addr *, uint32_t
$define %func netSend as function with args int, const api_net_msg *
$define %func netRecv as function with args int, api_net_msg *
$define %func netCtl as function with args int, int, void *

*/

/* !SPACE!

$space %export netOpen, netBind, netConnect
$space %export netListen, netAccept
$space %export netSend, netRecv, netCtl

*/

#include <native.h>
#include <stdint.h>
#include "private.h"

int
netOpen(int proto, int mode, uint32_t flags)
{
	return (__sysret_int(__syscall3(CALL_NET_OPEN, (long)proto,
	    (long)mode, (long)flags)));
}

int
netBind(int handle, const struct api_net_addr *addr)
{
	return (__sysret_int(__syscall2(CALL_NET_BIND, (long)handle,
	    (long)addr)));
}

int
netConnect(int handle, const struct api_net_addr *addr)
{
	return (__sysret_int(__syscall2(CALL_NET_CONNECT, (long)handle,
	    (long)addr)));
}

int
netListen(int handle, int backlog)
{
	return (__sysret_int(__syscall2(CALL_NET_LISTEN, (long)handle,
	    (long)backlog)));
}

int
netAccept(int handle, struct api_net_addr *addr, uint32_t flags)
{
	return (__sysret_int(__syscall3(CALL_NET_ACCEPT, (long)handle,
	    (long)addr, (long)flags)));
}

ssize_t
netSend(int handle, const struct api_net_msg *msg)
{
	return ((ssize_t)__sysret(__syscall2(CALL_NET_SEND,
	    (long)handle, (long)msg)));
}

ssize_t
netRecv(int handle, struct api_net_msg *msg)
{
	return ((ssize_t)__sysret(__syscall2(CALL_NET_RECV,
	    (long)handle, (long)msg)));
}

int
netCtl(int handle, int op, void *arg)
{
	return (__sysret_int(__syscall3(CALL_NET_CTL, (long)handle,
	    (long)op, (long)arg)));
}
