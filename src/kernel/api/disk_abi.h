/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* !DEFINES!

$define %type dioc_u32 as 32 bit unsigned, from the compiler not from a libc
$define %type dioc_u64 as 64 bit unsigned, from the compiler not from a libc
$define %type char as 8 bit signed
$define %type dioc_info_t as disk geometry and identity reported to userspace
$define %type dioc_mkfs_chainfs_t as ChainFS format request
$define %type dioc_mkfs_fat32_t as FAT32 format request
$define %type dioc_gpt_add_t as GPT partition add request
$define %type dioc_file_t as file transfer request with offset and total size
$define %type dioc_bootinst_t as BIOS boot block installation request

$const DIOC_IFACE_NAME as the newbus interface name every block device publishes
$const DIOC_NAME_MAX as ceiling on a block device name including terminator

*/

/* !SPACE!

$space %export dioc_info_t, dioc_mkfs_chainfs_t, dioc_mkfs_fat32_t
$space %export dioc_gpt_add_t, dioc_file_t, dioc_bootinst_t

*/



#ifndef KERNEL_API_DISK_ABI_H
#define KERNEL_API_DISK_ABI_H

typedef __UINT32_TYPE__	dioc_u32;
typedef __UINT64_TYPE__	dioc_u64;


#define	DIOC_IFACE_NAME		"block"
#define	DIOC_NAME_MAX		32
#define	DIOC_GETINFO		0x4401
#define	DIOC_FLUSH		0x4402
#define	DIOC_RESCAN		0x4403
#define	DIOC_MKFS_CHAINFS	0x4404
#define	DIOC_GPT_INIT		0x4405
#define	DIOC_GPT_ADD		0x4406
#define	DIOC_GPT_COMMIT		0x4407
#define	DIOC_MKFS_FAT32		0x4408
#define	DIOC_CFS_WRITE		0x4409
#define	DIOC_CFS_MKDIR		0x440A
#define	DIOC_CFS_READ		0x440B
#define	DIOC_FAT_WRITE		0x440C
#define	DIOC_FAT_MKDIR		0x440D
#define	DIOC_CFS_TRUNC		0x440E
#define	DIOC_BOOTINST		0x440F
#define	DIOC_PROBEFS		0x4410
#define	DIOC_INFO_F_READONLY	0x0001
#define	DIOC_INFO_F_NOFLUSH	0x0002
#define	DIOC_INFO_F_SLICE	0x0004
#define	DIOC_INFO_F_CHAINFS	0x0008
#define	DIOC_INFO_F_FAT		0x0010
#define	DIOC_INFO_F_ROOT	0x0020
#define	DIOC_PART_NAME_MAX	36
#define	DIOC_PATH_MAX		128
#define	DIOC_FILE_MAX		(16U * 1024U * 1024U)
#define	DIOC_BOOT_SECTOR	512U
#define	DIOC_BOOT_CODE_MAX	440U
#define	DIOC_BOOT_PARAM_OFF	0x08U
#define	DIOC_BOOT_PARAM_MAGIC	0x31534f4fU
#define	DIOC_BOOT_STAGE2_MAX	(60U * DIOC_BOOT_SECTOR)
#define	DIOC_BOOT_SIG_OFF	510U
#define	DIOC_GPT_KIND_ESP	1
#define	DIOC_GPT_KIND_BIOS	2
#define	DIOC_GPT_KIND_ROOT	3
typedef struct dioc_info {
	char		name[DIOC_NAME_MAX];
	char		parent[DIOC_NAME_MAX];
	dioc_u64	total_sectors;
	dioc_u64	base_lba;
	dioc_u32	sector_size;
	dioc_u32	max_io_sectors;
	dioc_u32	type;
	dioc_u32	flags;
	dioc_u32	index;
	dioc_u32	pad;
} dioc_info_t;

typedef struct dioc_mkfs_chainfs {
	dioc_u32	max_files;
	dioc_u32	pad;
	dioc_u64	total_blocks;
} dioc_mkfs_chainfs_t;

typedef struct dioc_mkfs_fat32 {
	char		label[12];
	dioc_u32	pad;
} dioc_mkfs_fat32_t;

typedef struct dioc_gpt_add {
	char		name[DIOC_PART_NAME_MAX];
	dioc_u32	kind;
	dioc_u64	size_sectors;
	dioc_u32	index;
	dioc_u32	pad;
	dioc_u64	first_lba;
	dioc_u64	last_lba;
} dioc_gpt_add_t;

typedef struct dioc_file {
	char		path[DIOC_PATH_MAX];
	dioc_u64	data;
	dioc_u64	size;
	dioc_u64	done;
	dioc_u64	offset;
	dioc_u64	total;
} dioc_file_t;

typedef struct dioc_bootinst {
	dioc_u64	stage1;
	dioc_u64	stage1_size;
	dioc_u64	stage2;
	dioc_u64	stage2_size;
	dioc_u64	stage2_lba;
} dioc_bootinst_t;

#endif
