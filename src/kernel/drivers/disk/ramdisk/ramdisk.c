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

#include "ramdisk.h"
#include "../disk.h"
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static void ramdisk_read_sector(disk_t *self, u32 lba, u8 *buffer) {
  if (!self->private_data)
    return;
  u8 *ram_start = (u8 *)self->private_data;
  u32 offset = lba * self->sector_size;

  if (offset + self->sector_size > self->total_sectors * self->sector_size) {
    com1_printf("[RAMDISK] Read out of bounds: lba=%u\n", lba);
    return;
  }

  memcpy(buffer, ram_start + offset, self->sector_size);
}

static void ramdisk_write_sector(disk_t *self, u32 lba, u8 *buffer) {
  if (!self->private_data)
    return;
  u8 *ram_start = (u8 *)self->private_data;
  u32 offset = lba * self->sector_size;

  if (offset + self->sector_size > self->total_sectors * self->sector_size) {
    com1_printf("[RAMDISK] Write out of bounds: lba=%u\n", lba);
    return;
  }

  memcpy(ram_start + offset, buffer, self->sector_size);
}

static disk_t ram_disk;

void ramdisk_init(void *location, u32 size) {
  com1_printf("[RAMDISK] Initializing at %p, size %u bytes\n", location, size);

  strcpy(ram_disk.name, "ramdisk0");
  ram_disk.type = DISK_TYPE_RAM;
  ram_disk.sector_size = 512;
  ram_disk.total_sectors = size / 512;
  ram_disk.private_data = location;
  ram_disk.read_sector = ramdisk_read_sector;
  ram_disk.write_sector = ramdisk_write_sector;

  disk_register(&ram_disk);
}
