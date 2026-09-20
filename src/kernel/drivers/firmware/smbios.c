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

$define %type fw_smbios_t as located SMBIOS entry point and table

$define %func sm_checksum_ok as function with args const u8 *, u32
$define %func sm_parse_v3 as function with args const u8 *, u32, fw_smbios_t *
$define %func sm_parse_v2 as function with args const u8 *, u32, fw_smbios_t *
$define %func fw_smbios_parse as function with args const void *, u32, fw_smbios_t *

*/

/* !SPACE!

$space %internal sm_checksum_ok, sm_parse_v3, sm_parse_v2
$space %export fw_smbios_parse

*/

#include <kernel/drivers/firmware/firmware.h>
#include <mlibc/mlibc.h>


#define	SM3_LENGTH_OFF		0x06
#define	SM3_MAJOR_OFF		0x07
#define	SM3_MINOR_OFF		0x08
#define	SM3_TABLE_MAX_OFF	0x0C
#define	SM3_TABLE_ADDR_OFF	0x10
#define	SM3_NOMINAL_LEN		24
#define	SM2_LENGTH_OFF		0x05
#define	SM2_MAJOR_OFF		0x06
#define	SM2_MINOR_OFF		0x07
#define	SM2_TABLE_LEN_OFF	0x16
#define	SM2_TABLE_ADDR_OFF	0x18
#define	SM2_STRUCT_COUNT_OFF	0x1C
#define	SM2_NOMINAL_LEN		31

#define	SM_LEN_MIN		24
#define	SM_LEN_MAX		64


static int
sm_checksum_ok(const u8 *p, u32 len)
{
	u8	sum;
	u32	i;

	if (len < SM_LEN_MIN || len > SM_LEN_MAX) {
		return (0);
	}
	sum = 0;
	for (i = 0; i < len; i++) {
		sum = (u8)(sum + p[i]);
	}
	return (sum == 0);
}

static int
sm_parse_v3(const u8 *p, u32 size, fw_smbios_t *out)
{
	u32	len;

	len = p[SM3_LENGTH_OFF];
	if (len > size || !sm_checksum_ok(p, len)) {
		return (-1);
	}
	out->entry = (u64)(unsigned long)p;
	out->table = *(const u64 *)(p + SM3_TABLE_ADDR_OFF);
	out->table_length = *(const u32 *)(p + SM3_TABLE_MAX_OFF);

	out->structures = 0;
	out->major = p[SM3_MAJOR_OFF];
	out->minor = p[SM3_MINOR_OFF];
	return (out->table != 0 ? 0 : -1);
}

static int
sm_parse_v2(const u8 *p, u32 size, fw_smbios_t *out)
{
	u32	len;

	len = p[SM2_LENGTH_OFF];
	if (len > size || !sm_checksum_ok(p, len)) {
		return (-1);
	}
	out->entry = (u64)(unsigned long)p;
	out->table = (u64)*(const u32 *)(p + SM2_TABLE_ADDR_OFF);
	out->table_length = *(const u16 *)(p + SM2_TABLE_LEN_OFF);
	out->structures = *(const u16 *)(p + SM2_STRUCT_COUNT_OFF);
	out->major = p[SM2_MAJOR_OFF];
	out->minor = p[SM2_MINOR_OFF];
	return (out->table != 0 ? 0 : -1);
}


int
fw_smbios_parse(const void *anchor, u32 size, fw_smbios_t *out)
{
	const u8	*p;
	fw_smbios_t	tmp;
	int		ret;

	if (anchor == NULL || out == NULL || size < SM_LEN_MIN) {
		return (-1);
	}
	p = (const u8 *)anchor;
	memset(&tmp, 0, sizeof(tmp));

	if (memcmp(p, "_SM3_", 5) == 0) {
		ret = sm_parse_v3(p, size, &tmp);
	} else if (memcmp(p, "_SM_", 4) == 0) {
		ret = sm_parse_v2(p, size, &tmp);
	} else {
		return (-1);
	}
	if (ret != 0) {
		return (-1);
	}
	*out = tmp;
	return (0);
}
