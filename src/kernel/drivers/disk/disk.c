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
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type disk_type_t as enum with disk types PATA, NVME, RAM, UNKNOWN
$define %type disk_t as struct with one registered block device and its geometry

$define %func disk_type_name as function with args disk_type_t
$define %func disk_manager_init as procedure with args void
$define %func disk_manager_is_initialized as function with args void
$define %func disk_register as function with args disk_t *
$define %func disk_unregister as function with args disk_t *
$define %func disk_get as function with args int
$define %func disk_find as function with args const char *
$define %func disk_count as function with args void
$define %func disk_capacity_bytes as function with args const disk_t *
$define %func disk_dump as procedure with args void

*/

/* !SPACE!

$space %internal disk_type_name
$space %export disk_manager_init, disk_manager_is_initialized
$space %export disk_register, disk_unregister
$space %export disk_get, disk_find, disk_count
$space %export disk_capacity_bytes, disk_dump

*/

#include "disk.h"
#include <kernel/drivers/disk/bio.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

static disk_t	*disks[DISK_MAX];
static int	disk_count_val;
static int	disk_manager_initialized_val;

static const char *
disk_type_name(disk_type_t type)
{
	switch (type) {
	case DISK_TYPE_PATA:
		return ("pata");
	case DISK_TYPE_NVME:
		return ("nvme");
	case DISK_TYPE_RAM:
		return ("ram");
	default:
		return ("unknown");
	}
}

void
disk_manager_init(void)
{
	int	i;

	for (i = 0; i < DISK_MAX; i++) {
		disks[i] = NULL;
	}
	disk_count_val = 0;
	disk_manager_initialized_val = 1;
	drivers_log("[DISK] Disk manager initialized\n");
}

int
disk_manager_is_initialized(void)
{
	return (disk_manager_initialized_val);
}

int
disk_register(disk_t *disk)
{
	int	i;

	if (disk == NULL) {
		return (-1);
	}
	if (disk->ops == NULL || disk->ops->submit == NULL) {
		drivers_log("[DISK] '%s' rejected: no submit op\n", disk->name);
		return (-1);
	}
	if (disk->sector_size == 0 || disk->total_sectors == 0 ||
	    disk->max_io_sectors == 0) {
		drivers_log("[DISK] '%s' rejected: geometry %u x %u sectors, "
		    "max io %u\n", disk->name, disk->sector_size,
		    (u32)disk->total_sectors, disk->max_io_sectors);
		return (-1);
	}
	if (disk_count_val >= DISK_MAX) {
		drivers_log("[DISK] '%s' rejected: %d device limit reached\n",
		    disk->name, DISK_MAX);
		return (-1);
	}
	if (disk->name[0] == '\0') {
		drivers_log("[DISK] rejected: unnamed device\n");
		return (-1);
	}
	for (i = 0; i < disk_count_val; i++) {
		if (disks[i] == disk) {
			return (disks[i]->index);
		}
		if (strcmp(disks[i]->name, disk->name) == 0) {
			drivers_log("[DISK] '%s' rejected: name already "
			    "registered\n", disk->name);
			return (-1);
		}
	}

	disk->index = disk_count_val;
	disks[disk_count_val] = disk;
	drivers_log("[DISK] Registered %s: %s, %u x %u-byte sectors "
	    "(%u MB), max io %u sectors%s\n", disk->name,
	    disk_type_name(disk->type), (u32)disk->total_sectors,
	    disk->sector_size,
	    (u32)(disk_capacity_bytes(disk) >> 20), disk->max_io_sectors,
	    (disk->flags & DISK_F_READONLY) != 0 ? ", read-only" : "");
	return (disk_count_val++);
}

int
disk_unregister(disk_t *disk)
{
	int	i, j;

	if (disk == NULL) {
		return (-1);
	}
	for (i = 0; i < disk_count_val; i++) {
		if (disks[i] != disk) {
			continue;
		}
		
		for (j = i; j < disk_count_val - 1; j++) {
			disks[j] = disks[j + 1];
			disks[j]->index = j;
		}
		disks[disk_count_val - 1] = NULL;
		disk_count_val--;
		disk->index = -1;
		drivers_log("[DISK] Unregistered %s\n", disk->name);
		return (0);
	}
	return (-1);
}

disk_t *
disk_get(int index)
{
	if (index < 0 || index >= disk_count_val) {
		return (NULL);
	}
	return (disks[index]);
}

disk_t *
disk_find(const char *name)
{
	int	i;

	if (name == NULL) {
		return (NULL);
	}
	for (i = 0; i < disk_count_val; i++) {
		if (disks[i] != NULL && strcmp(disks[i]->name, name) == 0) {
			return (disks[i]);
		}
	}
	return (NULL);
}

int
disk_count(void)
{
	return (disk_count_val);
}

u64
disk_capacity_bytes(const disk_t *disk)
{
	if (disk == NULL) {
		return (0);
	}
	return (disk->total_sectors * (u64)disk->sector_size);
}

void
disk_dump(void)
{
	disk_t	*d;
	int	i;

	printk("disk: %d device(s)\n", disk_count_val);
	for (i = 0; i < disk_count_val; i++) {
		d = disks[i];
		if (d == NULL) {
			continue;
		}
		printk("  %d: %-12s %-8s %u sectors x %u (%u MB) maxio %u%s\n",
		    i, d->name, disk_type_name(d->type),
		    (u32)d->total_sectors, d->sector_size,
		    (u32)(disk_capacity_bytes(d) >> 20), d->max_io_sectors,
		    (d->flags & DISK_F_READONLY) != 0 ? " ro" : "");
	}
	bio_dump();
}

static void
disk_core_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "disk_core", 0) == NULL) {
		device_add_child(parent, "disk_core", 0);
	}
}

static int
disk_core_attach(device_t dev)
{
	(void)dev;
	disk_manager_init();
	return (0);
}

static devclass_t disk_core_devclass = {
	.name		= "disk",
	.maxunit	= 1,
};

static driver_t disk_core_driver = {
	.name		= "disk_core",
	.identify	= disk_core_identify,
	.probe		= NULL,
	.attach		= disk_core_attach,
};

PSEUDO_DRIVER_MODULE(disk_core, disk_core_driver, disk_core_devclass,
    NEWBUS_PASS_CORE, NEWBUS_ORDER_FIRST);
