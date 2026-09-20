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

$define %type int as 32 bit signed
$define %type uint64_t as 64 bit unsigned
$define %type ldisk_dev_t as an opened block device

$define %func ldisk_boot_install as function with args ldisk_dev_t *, const void *, uint64_t, const void *, uint64_t, uint64_t

*/

/* !SPACE!

$space %export ldisk_boot_install

*/

#include <errno.h>
#include <native.h>
#include <string.h>

#include "disk_int.h"


int
ldisk_boot_install(ldisk_dev_t *dev, const void *stage1, uint64_t stage1_size,
    const void *stage2, uint64_t stage2_size, uint64_t stage2_lba)
{
	dioc_bootinst_t	req;

	if (!ldisk_dev_ok(dev)) {
		return (-1);
	}
	if ((dev->info.flags & LDISK_F_SLICE) != 0) {
		errno = EINVAL;
		return (-1);
	}
	if ((dev->info.flags & LDISK_F_READONLY) != 0) {
		errno = EROFS;
		return (-1);
	}
	if ((dev->info.flags & LDISK_F_ROOT) != 0) {
		errno = EBUSY;
		return (-1);
	}
	if (stage1 == NULL || stage2 == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if (stage1_size != DIOC_BOOT_SECTOR) {
		errno = EINVAL;
		return (-1);
	}

	if (stage2_size == 0 || stage2_size > DIOC_BOOT_STAGE2_MAX) {
		errno = E2BIG;
		return (-1);
	}
	if (stage2_lba == 0) {
		errno = EINVAL;
		return (-1);
	}

	memset(&req, 0, sizeof(req));
	req.stage1 = (uint64_t)(uintptr_t)stage1;
	req.stage1_size = stage1_size;
	req.stage2 = (uint64_t)(uintptr_t)stage2;
	req.stage2_size = stage2_size;
	req.stage2_lba = stage2_lba;

	if (entityIoctl(dev->handle, DIOC_BOOTINST, &req) != 0) {
		return (-1);
	}
	if ((dev->info.flags & LDISK_F_NOFLUSH) == 0) {
		(void)ldisk_flush(dev);
	}
	return (0);
}
