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

#include <kernel/drivers/disk/pata/pata.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static void pata_wait_bsy() {
  while (inb(IDE_STATUS) & IDE_STATUS_BSY)
    ;
}

static void pata_wait_drq() {
  while (!(inb(IDE_STATUS) & IDE_STATUS_DRQ))
    ;
}

static unsigned short pata_dummy_buffer[256];

void pata_identify(unsigned short *target_buf) {
  com1_printf("PATA: Identifying drive...\n");
  outb(IDE_DRIVE_SEL, 0xA0);
  outb(IDE_SEC_COUNT, 0);
  outb(IDE_LBA_LOW, 0);
  outb(IDE_LBA_MID, 0);
  outb(IDE_LBA_HIGH, 0);
  outb(IDE_COMMAND, IDE_CMD_IDENTIFY);

  unsigned char status = inb(IDE_STATUS);
  if (status == 0) {
    com1_printf("PATA: No drive found.\n");
    return;
  }

  pata_wait_bsy();

  if (target_buf) {
    insw(IDE_DATA, target_buf, 256);
  } else {
    insw(IDE_DATA, pata_dummy_buffer, 256);
  }

  com1_printf("PATA: Drive identified successfully.\n");
}

void pata_read_sector(unsigned int lba, unsigned char *buffer) {
  pata_wait_bsy();

  outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
  outb(IDE_FEATURES, 0x00);
  outb(IDE_SEC_COUNT, 1);
  outb(IDE_LBA_LOW, (unsigned char)lba);
  outb(IDE_LBA_MID, (unsigned char)(lba >> 8));
  outb(IDE_LBA_HIGH, (unsigned char)(lba >> 16));
  outb(IDE_COMMAND, IDE_CMD_READ);

  pata_wait_bsy();
  pata_wait_drq();

  insw(IDE_DATA, buffer, 256);
}

void pata_write_sector(unsigned int lba, unsigned char *buffer) {
  pata_wait_bsy();

  outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
  outb(IDE_FEATURES, 0x00);
  outb(IDE_SEC_COUNT, 1);
  outb(IDE_LBA_LOW, (unsigned char)lba);
  outb(IDE_LBA_MID, (unsigned char)(lba >> 8));
  outb(IDE_LBA_HIGH, (unsigned char)(lba >> 16));
  outb(IDE_COMMAND, IDE_CMD_WRITE);

  pata_wait_bsy();
  pata_wait_drq();

  outsw(IDE_DATA, buffer, 256);

  outb(IDE_COMMAND, 0xE7);
  pata_wait_bsy();
}
