/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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

$define %func fw_rsdp_checksum_ok as function with args const void *

*/

/* !SPACE!

$space %export fw_rsdp_checksum_ok

*/

#include <kernel/drivers/firmware/firmware.h>
#include <mlibc/mlibc.h>

#define	FW_RSDP_SIGNATURE	"RSD PTR "
#define	FW_RSDP_SIGNATURE_LEN	8
#define	FW_RSDP_V1_LEN		20

int
fw_rsdp_checksum_ok(const void *rsdp)
{
	const u8	*p;
	u8		sum;
	u32		i;

	if (rsdp == NULL) {
		return (0);
	}
	p = (const u8 *)rsdp;
	if (memcmp(p, FW_RSDP_SIGNATURE, FW_RSDP_SIGNATURE_LEN) != 0) {
		return (0);
	}
	sum = 0;
	for (i = 0; i < FW_RSDP_V1_LEN; i++) {
		sum = (u8)(sum + p[i]);
	}
	return (sum == 0);
}
