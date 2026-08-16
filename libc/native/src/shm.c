/* !DEFINES!

$define %type api_shmget_args as native shared memory create request
$define %type api_shmmap_args as native shared memory map request
$define %func shmGet as function with args uint64_t, size_t, uint32_t, int *
$define %func shmMap as function with args int, void *, size_t, uint32_t, uint32_t
$define %func shmCtl as function with args int, int, void *
$define %func shmClose as function with args int

*/

/* !SPACE!

$space %export shmGet, shmMap, shmCtl, shmClose

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

#include <errno.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "private.h"

int
shmGet(uint64_t key, size_t size, uint32_t flags, int *handle)
{
	struct api_shmget_args	args;
	int			ret;

	if (!handle) {
		errno = EINVAL;
		return (-1);
	}
	memset(&args, 0, sizeof(args));
	args.key = key;
	args.size = size;
	args.flags = flags;
	ret = __sysret_int(__syscall1(CALL_SHM_GET, (long)&args));
	if (ret != 0) {
		return (-1);
	}
	*handle = (int)args.id;
	return (0);
}

void *
shmMap(int handle, void *addr, size_t size, uint32_t prot, uint32_t flags)
{
	struct api_shmmap_args	args;
	long			ret;

	if (handle < 0 ||
	    (prot & (API_MAP_READ | API_MAP_WRITE)) == 0) {
		errno = EINVAL;
		return (NULL);
	}
	memset(&args, 0, sizeof(args));
	args.id = (uint32_t)handle;
	args.prot = prot;
	args.flags = flags | API_MAP_SHARED;
	args.addr = (uint64_t)(uintptr_t)addr;
	args.size = size;
	ret = __syscall1(CALL_SHM_MAP, (long)&args);
	if (ret < 0) {
		errno = (int)-ret;
		return (NULL);
	}
	return ((void *)ret);
}

int
shmCtl(int handle, int cmd, void *arg)
{
	return (__sysret_int(__syscall3(CALL_SHM_CTL, (long)handle,
	    (long)cmd, (long)arg)));
}

int
shmClose(int handle)
{
	return (entityClose(handle));
}
