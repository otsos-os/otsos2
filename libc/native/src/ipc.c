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

$define %type api_ipc_message as native IPC message descriptor
$define %type api_ipc_call as native IPC call descriptor
$define %func ipcCreate as function with args const char *, uint32_t, uint32_t
$define %func ipcConnect as function with args const char *, uint32_t
$define %func ipcSend as function with args int, const api_ipc_message *
$define %func ipcRecv as function with args int, api_ipc_message *, uint32_t
$define %func ipcCall as function with args int, api_ipc_call *
$define %func ipcCtl as function with args int, uint32_t, void *

*/

/* !SPACE!

$space %export ipcCreate, ipcConnect, ipcSend, ipcRecv, ipcCall, ipcCtl

*/

#include <native.h>
#include "private.h"

int
ipcCreate(const char *name, uint32_t flags, uint32_t mode)
{
	return (__sysret_int(__syscall3(CALL_IPC_CREATE, (long)name,
	    (long)flags, (long)mode)));
}

int
ipcConnect(const char *name, uint32_t flags)
{
	return (__sysret_int(__syscall2(CALL_IPC_CONNECT, (long)name,
	    (long)flags)));
}

ssize_t
ipcSend(int handle, const struct api_ipc_message *message)
{
	return (__sysret(__syscall2(CALL_IPC_SEND, (long)handle,
	    (long)message)));
}

ssize_t
ipcRecv(int handle, struct api_ipc_message *message, uint32_t flags)
{
	return (__sysret(__syscall3(CALL_IPC_RECV, (long)handle,
	    (long)message, (long)flags)));
}

ssize_t
ipcCall(int handle, struct api_ipc_call *call)
{
	return (__sysret(__syscall2(CALL_IPC_CALL, (long)handle,
	    (long)call)));
}

int
ipcCtl(int handle, uint32_t op, void *arg)
{
	return (__sysret_int(__syscall3(CALL_IPC_CTL, (long)handle,
	    (long)op, (long)arg)));
}
