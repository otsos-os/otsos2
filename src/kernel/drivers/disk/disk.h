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

$define %type u32 as 32 bit unsigned
$define %type u8 as 8 bit unsigned
$define %type int as 32 bit signed
$define %type disk_type_t as enum with disk types PATA, NVME, RAM, UNKNOWN
$define %type disk_t as struct with name, type, sector_size, sectors, ops

$define %func disk_manager_init as procedure with args void
$define %func disk_manager_is_initialized as function with args void
$define %func disk_register as function with args disk_t *
$define %func disk_get as function with args int
$define %func disk_count as function with args void
$define %func disk_read as procedure with args disk_t *, u32, u8 *
$define %func disk_write as procedure with args disk_t *, u32, u8 *

*/

/* !SPACE!

$space %export disk_manager_init, disk_manager_is_initialized
$space %export disk_register, disk_get, disk_count
$space %export disk_read, disk_write

*/

#ifndef KERNEL_DRIVERS_DISK_DISK_H
#define KERNEL_DRIVERS_DISK_DISK_H

#include <mlibc/mlibc.h>

typedef enum {
	DISK_TYPE_PATA,
	DISK_TYPE_NVME,
	DISK_TYPE_RAM,
	DISK_TYPE_UNKNOWN
} disk_type_t;

typedef struct disk {
	char		name[32];
	disk_type_t	type;
	u32		sector_size;
	u32		total_sectors;
	void		*private_data;
	void		(*read_sector)(struct disk *self, u32 lba,
			    u8 *buffer);
	void		(*write_sector)(struct disk *self, u32 lba,
			    u8 *buffer);
} disk_t;

void	disk_manager_init(void);
int	disk_manager_is_initialized(void);
int	disk_register(disk_t *disk);
disk_t	*disk_get(int index);
int	disk_count(void);
void	disk_read(disk_t *disk, u32 lba, u8 *buffer);
void	disk_write(disk_t *disk, u32 lba, u8 *buffer);

#endif
