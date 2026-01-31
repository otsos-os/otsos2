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
