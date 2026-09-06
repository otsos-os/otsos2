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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type disk_type_t as enum with disk types PATA, NVME, RAM, UNKNOWN
$define %type disk_ops_t as struct with the submit and timeout ops of a device
$define %type disk_t as struct with one registered block device and its geometry

$define %func disk_manager_init as procedure with args void
$define %func disk_manager_is_initialized as function with args void
$define %func disk_register as function with args disk_t *
$define %func disk_unregister as function with args disk_t *
$define %func disk_get as function with args int
$define %func disk_find as function with args const char *
$define %func disk_count as function with args void
$define %func disk_capacity_bytes as function with args const disk_t *
$define %func disk_dump as procedure with args void

$const DISK_NAME_MAX as ceiling on a block device name including terminator
$const DISK_MAX as ceiling on registered block devices
$const DISK_F_READONLY as flag marking a device that rejects writes
$const DISK_F_NO_FLUSH as flag marking a device with no volatile write cache

*/

/* !SPACE!

$space %export disk_manager_init, disk_manager_is_initialized
$space %export disk_register, disk_unregister
$space %export disk_get, disk_find, disk_count
$space %export disk_capacity_bytes, disk_dump

*/

#ifndef KERNEL_DRIVERS_DISK_DISK_H
#define KERNEL_DRIVERS_DISK_DISK_H

#include <mlibc/mlibc.h>
#define	DISK_NAME_MAX	32
#define	DISK_MAX	8
#define	DISK_F_READONLY	0x0001
#define	DISK_F_NO_FLUSH	0x0002

struct disk;
struct bio;

typedef enum {
	DISK_TYPE_PATA,
	DISK_TYPE_NVME,
	DISK_TYPE_RAM,
	DISK_TYPE_UNKNOWN
} disk_type_t;

typedef struct disk_ops {
	int	(*submit)(struct disk *disk, struct bio *bio);
	int	(*timeout)(struct disk *disk, struct bio *bio);
} disk_ops_t;

typedef struct disk {
	char			name[DISK_NAME_MAX];
	const disk_ops_t	*ops;
	void			*private_data;
	u64			total_sectors;
	u32			sector_size;
	u32			max_io_sectors;
	disk_type_t		type;
	u32			flags;
	int			index;
	int			pad;
} disk_t;

void	disk_manager_init(void);
int	disk_manager_is_initialized(void);

int	disk_register(disk_t *disk);
int	disk_unregister(disk_t *disk);
disk_t	*disk_get(int index);
disk_t	*disk_find(const char *name);
int	disk_count(void);

u64	disk_capacity_bytes(const disk_t *disk);
void	disk_dump(void);

#endif
