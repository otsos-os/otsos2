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

#include "disk.h"
#include <lib/com1.h>

#define MAX_DISKS 8

static disk_t *disks[MAX_DISKS];
static int disk_count_val = 0;
static int disk_manager_initialized = 0;

void disk_manager_init(void) {
  for (int i = 0; i < MAX_DISKS; i++) {
    disks[i] = 0;
  }
  disk_count_val = 0;
  disk_manager_initialized = 1;
  com1_printf("[DISK] Disk manager initialized\n");
}

int disk_manager_is_initialized(void) { return disk_manager_initialized; }

int disk_register(disk_t *disk) {
  if (disk_count_val >= MAX_DISKS) {
    com1_printf("[DISK] Error: Max disks reached\n");
    return -1;
  }
  disks[disk_count_val] = disk;
  com1_printf("[DISK] Registered disk %d: %s (Type: %d, Sectors: %u)\n",
              disk_count_val, disk->name, disk->type, disk->total_sectors);
  return disk_count_val++;
}

disk_t *disk_get(int index) {
  if (index < 0 || index >= disk_count_val) {
    return 0;
  }
  return disks[index];
}

int disk_count(void) { return disk_count_val; }

void disk_read(disk_t *disk, u32 lba, u8 *buffer) {
  if (disk && disk->read_sector) {
    disk->read_sector(disk, lba, buffer);
  }
}

void disk_write(disk_t *disk, u32 lba, u8 *buffer) {
  if (disk && disk->write_sector) {
    disk->write_sector(disk, lba, buffer);
  }
}
