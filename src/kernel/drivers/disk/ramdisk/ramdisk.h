#ifndef KERNEL_DRIVERS_DISK_RAMDISK_H
#define KERNEL_DRIVERS_DISK_RAMDISK_H

#include <mlibc/mlibc.h>

void ramdisk_init(void *location, u32 size);

#endif
